/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/validator.h"

#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "config.h"

#include "src/binary-reader.h"
#include "src/cast.h"
#include "src/expr-visitor.h"
#include "src/ir.h"
#include "src/shared-validator.h"

namespace wabt {

namespace {

class ScriptValidator {
 public:
  WABT_DISALLOW_COPY_AND_ASSIGN(ScriptValidator);
  ScriptValidator(Errors*, const Script*, const ValidateOptions& options);

  Result CheckScript();

 private:
  struct ActionResult {
    ActionResult(Result result) : result(result) {}
    ActionResult(Result::Enum result) : result(result) {}
    ActionResult(Result result, TypeVector types)
        : result(result), types(types) {}

    Result result;
    TypeVector types;
  };

  void WABT_PRINTF_FORMAT(3, 4)
      PrintError(const Location* loc, const char* fmt, ...);
  void CheckTypeIndex(const Location* loc,
                      Type actual,
                      Type expected,
                      const char* desc,
                      Index index,
                      const char* index_kind);
  void CheckResultTypes(const Location* loc,
                        const TypeVector& actual,
                        const TypeVector& expected,
                        const char* desc);

  ActionResult CheckInvoke(const InvokeAction* action);
  ActionResult CheckGet(const GetAction* action);
  ActionResult CheckAction(const Action* action);
  void CheckCommand(const Command* command);

  const ValidateOptions& options_;
  Errors* errors_ = nullptr;
  const Script* script_ = nullptr;

  Result result_ = Result::Ok;
};

class Validator : public ExprVisitor::Delegate {
 public:
  Validator(Errors*, const Module* module, const ValidateOptions& options);

  Result CheckModule();

  Result OnArrayGetExpr(ArrayGetExpr*) override;
  Result OnArrayLenExpr(ArrayLenExpr*) override;
  Result OnArrayNewExpr(ArrayNewExpr*) override;
  Result OnArraySetExpr(ArraySetExpr*) override;
  Result OnBinaryExpr(BinaryExpr*) override;
  Result BeginBlockExpr(BlockExpr*) override;
  Result EndBlockExpr(BlockExpr*) override;
  Result OnBrExpr(BrExpr*) override;
  Result OnBrIfExpr(BrIfExpr*) override;
  Result OnBrOnExnExpr(BrOnExnExpr*) override;
  Result OnBrTableExpr(BrTableExpr*) override;
  Result OnCallExpr(CallExpr*) override;
  Result OnCallIndirectExpr(CallIndirectExpr*) override;
  Result OnCompareExpr(CompareExpr*) override;
  Result OnConstExpr(ConstExpr*) override;
  Result OnConvertExpr(ConvertExpr*) override;
  Result OnDropExpr(DropExpr*) override;
  Result OnGlobalGetExpr(GlobalGetExpr*) override;
  Result OnGlobalSetExpr(GlobalSetExpr*) override;
  Result BeginIfExpr(IfExpr*) override;
  Result AfterIfTrueExpr(IfExpr*) override;
  Result EndIfExpr(IfExpr*) override;
  Result OnLoadExpr(LoadExpr*) override;
  Result OnLocalGetExpr(LocalGetExpr*) override;
  Result OnLocalSetExpr(LocalSetExpr*) override;
  Result OnLocalTeeExpr(LocalTeeExpr*) override;
  Result BeginLoopExpr(LoopExpr*) override;
  Result EndLoopExpr(LoopExpr*) override;
  Result OnMemoryCopyExpr(MemoryCopyExpr*) override;
  Result OnDataDropExpr(DataDropExpr*) override;
  Result OnMemoryFillExpr(MemoryFillExpr*) override;
  Result OnMemoryGrowExpr(MemoryGrowExpr*) override;
  Result OnMemoryInitExpr(MemoryInitExpr*) override;
  Result OnMemorySizeExpr(MemorySizeExpr*) override;
  Result OnTableCopyExpr(TableCopyExpr*) override;
  Result OnElemDropExpr(ElemDropExpr*) override;
  Result OnTableInitExpr(TableInitExpr*) override;
  Result OnTableGetExpr(TableGetExpr*) override;
  Result OnTableSetExpr(TableSetExpr*) override;
  Result OnTableGrowExpr(TableGrowExpr*) override;
  Result OnTableSizeExpr(TableSizeExpr*) override;
  Result OnTableFillExpr(TableFillExpr*) override;
  Result OnRefFuncExpr(RefFuncExpr*) override;
  Result OnRefNullExpr(RefNullExpr*) override;
  Result OnRefIsNullExpr(RefIsNullExpr*) override;
  Result OnNopExpr(NopExpr*) override;
  Result OnReturnExpr(ReturnExpr*) override;
  Result OnReturnCallExpr(ReturnCallExpr*) override;
  Result OnReturnCallIndirectExpr(ReturnCallIndirectExpr*) override;
  Result OnSelectExpr(SelectExpr*) override;
  Result OnStructGetExpr(StructGetExpr*) override;
  Result OnStructNewExpr(StructNewExpr*) override;
  Result OnStructSetExpr(StructSetExpr*) override;
  Result OnStoreExpr(StoreExpr*) override;
  Result OnUnaryExpr(UnaryExpr*) override;
  Result OnUnreachableExpr(UnreachableExpr*) override;
  Result BeginTryExpr(TryExpr*) override;
  Result OnCatchExpr(TryExpr*) override;
  Result EndTryExpr(TryExpr*) override;
  Result OnThrowExpr(ThrowExpr*) override;
  Result OnRethrowExpr(RethrowExpr*) override;
  Result OnAtomicWaitExpr(AtomicWaitExpr*) override;
  Result OnAtomicNotifyExpr(AtomicNotifyExpr*) override;
  Result OnAtomicLoadExpr(AtomicLoadExpr*) override;
  Result OnAtomicStoreExpr(AtomicStoreExpr*) override;
  Result OnAtomicRmwExpr(AtomicRmwExpr*) override;
  Result OnAtomicRmwCmpxchgExpr(AtomicRmwCmpxchgExpr*) override;
  Result OnTernaryExpr(TernaryExpr*) override;
  Result OnSimdLaneOpExpr(SimdLaneOpExpr*) override;
  Result OnSimdShuffleOpExpr(SimdShuffleOpExpr*) override;
  Result OnLoadSplatExpr(LoadSplatExpr*) override;

 private:
  Type GetDeclarationType(const FuncDeclaration&);
  Var GetFuncTypeIndex(const Location&, const FuncDeclaration&);

  const ValidateOptions& options_;
  Errors* errors_ = nullptr;
  SharedValidator validator_;
  const Module* current_module_ = nullptr;
  Result result_ = Result::Ok;
};

ScriptValidator::ScriptValidator(Errors* errors,
                                 const Script* script,
                                 const ValidateOptions& options)
    : options_(options), errors_(errors), script_(script) {}

void ScriptValidator::PrintError(const Location* loc, const char* format, ...) {
  result_ = Result::Error;
  WABT_SNPRINTF_ALLOCA(buffer, length, format);
  errors_->emplace_back(ErrorLevel::Error, *loc, buffer);
}

void ScriptValidator::CheckTypeIndex(const Location* loc,
                                     Type actual,
                                     Type expected,
                                     const char* desc,
                                     Index index,
                                     const char* index_kind) {
  if (Failed(TypeChecker::CheckType(actual, expected))) {
    PrintError(loc,
               "type mismatch for %s %" PRIindex " of %s. got %s, expected %s",
               index_kind, index, desc, actual.GetName(), expected.GetName());
  }
}

void ScriptValidator::CheckResultTypes(const Location* loc,
                                       const TypeVector& actual,
                                       const TypeVector& expected,
                                       const char* desc) {
  if (actual.size() == expected.size()) {
    for (size_t i = 0; i < actual.size(); ++i) {
      CheckTypeIndex(loc, actual[i], expected[i], desc, i, "result");
    }
  } else {
    PrintError(loc, "expected %" PRIzd " results, got %" PRIzd, expected.size(),
               actual.size());
  }
}

Type Validator::GetDeclarationType(const FuncDeclaration& decl) {
  if (decl.has_func_type) {
    return Type(decl.type_var.index());
  }
  if (decl.sig.param_types.empty()) {
    if (decl.sig.result_types.empty()) {
      return Type::Void;
    }
    if (decl.sig.result_types.size() == 1) {
      return decl.sig.result_types[0];
    }
  }
  return Type(current_module_->GetFuncTypeIndex(decl));
}

Var Validator::GetFuncTypeIndex(const Location& default_loc,
                                const FuncDeclaration& decl) {
  if (decl.has_func_type) {
    return decl.type_var;
  }
  return Var(current_module_->GetFuncTypeIndex(decl), default_loc);
}

Result Validator::OnBinaryExpr(BinaryExpr* expr) {
  result_ |= validator_.OnBinary(expr->loc, expr->opcode);
  return Result::Ok;
}

Result Validator::BeginBlockExpr(BlockExpr* expr) {
  result_ |=
      validator_.OnBlock(expr->loc, GetDeclarationType(expr->block.decl));
  return Result::Ok;
}

Result Validator::EndBlockExpr(BlockExpr* expr) {
  result_ |= validator_.OnEnd(expr->block.end_loc);
  return Result::Ok;
}

Result Validator::OnBrExpr(BrExpr* expr) {
  result_ |= validator_.OnBr(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnBrIfExpr(BrIfExpr* expr) {
  result_ |= validator_.OnBrIf(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnBrOnExnExpr(BrOnExnExpr* expr) {
  result_ |= validator_.OnBrOnExn(expr->loc, expr->label_var,
                                  expr->event_var);
  return Result::Ok;
}

Result Validator::OnBrTableExpr(BrTableExpr* expr) {
  result_ |= validator_.BeginBrTable(expr->loc);
  for (const Var& var : expr->targets) {
    result_ |= validator_.OnBrTableTarget(expr->loc, var);
  }
  result_ |= validator_.OnBrTableTarget(expr->loc, expr->default_target);
  result_ |= validator_.EndBrTable(expr->loc);
  return Result::Ok;
}

Result Validator::OnCallExpr(CallExpr* expr) {
  result_ |= validator_.OnCall(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnCallIndirectExpr(CallIndirectExpr* expr) {
  result_ |= validator_.OnCallIndirect(
      expr->loc, GetFuncTypeIndex(expr->loc, expr->decl), expr->table);
  return Result::Ok;
}

Result Validator::OnCompareExpr(CompareExpr* expr) {
  result_ |= validator_.OnCompare(expr->loc, expr->opcode);
  return Result::Ok;
}

Result Validator::OnConstExpr(ConstExpr* expr) {
  result_ |= validator_.OnConst(expr->loc, expr->const_.type());
  return Result::Ok;
}

Result Validator::OnConvertExpr(ConvertExpr* expr) {
  result_ |= validator_.OnConvert(expr->loc, expr->opcode);
  return Result::Ok;
}

Result Validator::OnDropExpr(DropExpr* expr) {
  result_ |= validator_.OnDrop(expr->loc);
  return Result::Ok;
}

Result Validator::OnGlobalGetExpr(GlobalGetExpr* expr) {
  result_ |= validator_.OnGlobalGet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnGlobalSetExpr(GlobalSetExpr* expr) {
  result_ |= validator_.OnGlobalSet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::BeginIfExpr(IfExpr* expr) {
  result_ |= validator_.OnIf(expr->loc, GetDeclarationType(expr->true_.decl));
  return Result::Ok;
}

Result Validator::AfterIfTrueExpr(IfExpr* expr) {
  if (!expr->false_.empty()) {
    result_ |= validator_.OnElse(expr->true_.end_loc);
  }
  return Result::Ok;
}

Result Validator::EndIfExpr(IfExpr* expr) {
  result_ |= validator_.OnEnd(expr->false_.empty() ? expr->true_.end_loc
                                                   : expr->false_end_loc);
  return Result::Ok;
}

Result Validator::OnLoadExpr(LoadExpr* expr) {
  result_ |= validator_.OnLoad(expr->loc, expr->opcode,
                               expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnLocalGetExpr(LocalGetExpr* expr) {
  result_ |= validator_.OnLocalGet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnLocalSetExpr(LocalSetExpr* expr) {
  result_ |= validator_.OnLocalSet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnLocalTeeExpr(LocalTeeExpr* expr) {
  result_ |= validator_.OnLocalTee(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::BeginLoopExpr(LoopExpr* expr) {
  result_ |= validator_.OnLoop(expr->loc, GetDeclarationType(expr->block.decl));
  return Result::Ok;
}

Result Validator::EndLoopExpr(LoopExpr* expr) {
  result_ |= validator_.OnEnd(expr->block.end_loc);
  return Result::Ok;
}

Result Validator::OnMemoryCopyExpr(MemoryCopyExpr* expr) {
  result_ |= validator_.OnMemoryCopy(expr->loc);
  return Result::Ok;
}

Result Validator::OnDataDropExpr(DataDropExpr* expr) {
  result_ |= validator_.OnDataDrop(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnMemoryFillExpr(MemoryFillExpr* expr) {
  result_ |= validator_.OnMemoryFill(expr->loc);
  return Result::Ok;
}

Result Validator::OnMemoryGrowExpr(MemoryGrowExpr* expr) {
  result_ |= validator_.OnMemoryGrow(expr->loc);
  return Result::Ok;
}

Result Validator::OnMemoryInitExpr(MemoryInitExpr* expr) {
  result_ |= validator_.OnMemoryInit(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnMemorySizeExpr(MemorySizeExpr* expr) {
  result_ |= validator_.OnMemorySize(expr->loc);
  return Result::Ok;
}

Result Validator::OnTableCopyExpr(TableCopyExpr* expr) {
  result_ |=
      validator_.OnTableCopy(expr->loc, expr->dst_table, expr->src_table);
  return Result::Ok;
}

Result Validator::OnElemDropExpr(ElemDropExpr* expr) {
  result_ |= validator_.OnElemDrop(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnTableInitExpr(TableInitExpr* expr) {
  result_ |=
      validator_.OnTableInit(expr->loc, expr->segment_index, expr->table_index);
  return Result::Ok;
}

Result Validator::OnTableGetExpr(TableGetExpr* expr) {
  result_ |= validator_.OnTableGet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnTableSetExpr(TableSetExpr* expr) {
  result_ |= validator_.OnTableSet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnTableGrowExpr(TableGrowExpr* expr) {
  result_ |= validator_.OnTableGrow(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnTableSizeExpr(TableSizeExpr* expr) {
  result_ |= validator_.OnTableSize(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnTableFillExpr(TableFillExpr* expr) {
  result_ |= validator_.OnTableFill(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnRefFuncExpr(RefFuncExpr* expr) {
  result_ |= validator_.OnRefFunc(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnRefNullExpr(RefNullExpr* expr) {
  result_ |= validator_.OnRefNull(expr->loc);
  return Result::Ok;
}

Result Validator::OnRefIsNullExpr(RefIsNullExpr* expr) {
  result_ |= validator_.OnRefIsNull(expr->loc);
  return Result::Ok;
}

Result Validator::OnNopExpr(NopExpr* expr) {
  result_ |= validator_.OnNop(expr->loc);
  return Result::Ok;
}

Result Validator::OnReturnExpr(ReturnExpr* expr) {
  result_ |= validator_.OnReturn(expr->loc);
  return Result::Ok;
}

Result Validator::OnReturnCallExpr(ReturnCallExpr* expr) {
  result_ |= validator_.OnReturnCall(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnReturnCallIndirectExpr(ReturnCallIndirectExpr* expr) {
  result_ |= validator_.OnReturnCallIndirect(
      expr->loc, GetFuncTypeIndex(expr->loc, expr->decl), expr->table);
  return Result::Ok;
}

Result Validator::OnSelectExpr(SelectExpr* expr) {
  assert(expr->result_type.size());
  result_ |= validator_.OnSelect(expr->loc, expr->result_type[0]);
  // TODO: Existing behavior fails when select fails.
#if 0
  return Result::Ok;
#else
  return result_;
#endif
}

Result Validator::OnStructGetExpr(StructGetExpr* expr) {
  result_ |=
      validator_.OnStructGet(expr->loc, expr->struct_var, expr->field_var);
  return Result::Ok;
}

Result Validator::OnStructNewExpr(StructNewExpr* expr) {
  result_ |= validator_.OnStructNew(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnStructSetExpr(StructSetExpr* expr) {
  result_ |=
      validator_.OnStructSet(expr->loc, expr->struct_var, expr->field_var);
  return Result::Ok;
}

Result Validator::OnArrayGetExpr(ArrayGetExpr* expr) {
  result_ |= validator_.OnArrayGet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnArrayNewExpr(ArrayNewExpr* expr) {
  result_ |= validator_.OnArrayNew(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnArraySetExpr(ArraySetExpr* expr) {
  result_ |= validator_.OnArraySet(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnArrayLenExpr(ArrayLenExpr* expr) {
  result_ |= validator_.OnArrayLen(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnStoreExpr(StoreExpr* expr) {
  result_ |= validator_.OnStore(expr->loc, expr->opcode,
                                expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnUnaryExpr(UnaryExpr* expr) {
  result_ |= validator_.OnUnary(expr->loc, expr->opcode);
  return Result::Ok;
}

Result Validator::OnUnreachableExpr(UnreachableExpr* expr) {
  result_ |= validator_.OnUnreachable(expr->loc);
  return Result::Ok;
}

Result Validator::BeginTryExpr(TryExpr* expr) {
  result_ |= validator_.OnTry(expr->loc, GetDeclarationType(expr->block.decl));
  return Result::Ok;
}

Result Validator::OnCatchExpr(TryExpr* expr) {
  result_ |= validator_.OnCatch(expr->loc);
  return Result::Ok;
}

Result Validator::EndTryExpr(TryExpr* expr) {
  result_ |= validator_.OnEnd(expr->block.end_loc);
  return Result::Ok;
}

Result Validator::OnThrowExpr(ThrowExpr* expr) {
  result_ |= validator_.OnThrow(expr->loc, expr->var);
  return Result::Ok;
}

Result Validator::OnRethrowExpr(RethrowExpr* expr) {
  result_ |= validator_.OnRethrow(expr->loc);
  return Result::Ok;
}

Result Validator::OnAtomicWaitExpr(AtomicWaitExpr* expr) {
  result_ |= validator_.OnAtomicWait(expr->loc, expr->opcode,
                                     expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnAtomicNotifyExpr(AtomicNotifyExpr* expr) {
  result_ |= validator_.OnAtomicNotify(expr->loc, expr->opcode,
                                       expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnAtomicLoadExpr(AtomicLoadExpr* expr) {
  result_ |= validator_.OnAtomicLoad(expr->loc, expr->opcode,
                                     expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnAtomicStoreExpr(AtomicStoreExpr* expr) {
  result_ |= validator_.OnAtomicStore(expr->loc, expr->opcode,
                                      expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnAtomicRmwExpr(AtomicRmwExpr* expr) {
  result_ |= validator_.OnAtomicRmw(expr->loc, expr->opcode,
                                    expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnAtomicRmwCmpxchgExpr(AtomicRmwCmpxchgExpr* expr) {
  result_ |= validator_.OnAtomicRmwCmpxchg(
      expr->loc, expr->opcode, expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Result Validator::OnTernaryExpr(TernaryExpr* expr) {
  result_ |= validator_.OnTernary(expr->loc, expr->opcode);
  return Result::Ok;
}

Result Validator::OnSimdLaneOpExpr(SimdLaneOpExpr* expr) {
  result_ |= validator_.OnSimdLaneOp(expr->loc, expr->opcode, expr->val);
  return Result::Ok;
}

Result Validator::OnSimdShuffleOpExpr(SimdShuffleOpExpr* expr) {
  result_ |= validator_.OnSimdShuffleOp(expr->loc, expr->opcode, expr->val);
  return Result::Ok;
}

Result Validator::OnLoadSplatExpr(LoadSplatExpr* expr) {
  result_ |= validator_.OnLoadSplat(expr->loc, expr->opcode,
                                    expr->opcode.GetAlignment(expr->align));
  return Result::Ok;
}

Validator::Validator(Errors* errors,
                     const Module* module,
                     const ValidateOptions& options)
    : options_(options),
      errors_(errors),
      validator_(errors_, options_),
      current_module_(module) {}

static TypeVector ToTypeVector(const TypeVarVector& vec) {
  TypeVector result;
  for (auto&& type_var : vec) {
    assert(type_var.var.is_index());
    assert(!type_var.type.IsRefT() ||
           type_var.type.GetRefTIndex() == type_var.var.index());
    result.push_back(type_var.type);
  }
  return result;
}

Result Validator::CheckModule() {
  const Module* module = current_module_;

  // Type section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<TypeModuleField>(&field)) {
      switch (f->type->kind()) {
        case TypeEntryKind::Func: {
          FuncType* func_type = cast<FuncType>(f->type.get());
          TypeVector param_types = ToTypeVector(func_type->sig.param_types);
          TypeVector result_types = ToTypeVector(func_type->sig.result_types);
          result_ |= validator_.OnFuncType(
              field.loc, param_types.size(), param_types.data(),
              result_types.size(), result_types.data());
          break;
        }

        case TypeEntryKind::Struct: {
          StructType* struct_type = cast<StructType>(f->type.get());
          TypeMutVector type_muts;
          for (auto&& field : struct_type->fields) {
            type_muts.push_back(TypeMut{field.type, field.mutable_});
          }
          result_ |= validator_.OnStructType(field.loc, type_muts.size(),
                                             type_muts.data());
          break;
        }

        case TypeEntryKind::Array: {
          ArrayType* array_type = cast<ArrayType>(f->type.get());
          result_ |= validator_.OnArrayType(
              field.loc,
              TypeMut{array_type->field.type, array_type->field.mutable_});
          break;
        }
      }
    }
  }

  // Import section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<ImportModuleField>(&field)) {
      switch (f->import->kind()) {
        case ExternalKind::Func: {
          auto&& func = cast<FuncImport>(f->import.get())->func;
          result_ |= validator_.OnFunction(
              field.loc, GetFuncTypeIndex(field.loc, func.decl));
          break;
        }

        case ExternalKind::Table: {
          auto&& table = cast<TableImport>(f->import.get())->table;
          result_ |=
              validator_.OnTable(field.loc, table.elem_type, table.elem_limits);
          break;
        }

        case ExternalKind::Memory: {
          auto&& memory = cast<MemoryImport>(f->import.get())->memory;
          result_ |= validator_.OnMemory(field.loc, memory.page_limits);
          break;
        }

        case ExternalKind::Global: {
          auto&& global = cast<GlobalImport>(f->import.get())->global;
          result_ |= validator_.OnGlobalImport(field.loc, global.type,
                                               global.mutable_);
          break;
        }

        case ExternalKind::Event: {
          auto&& event = cast<EventImport>(f->import.get())->event;
          result_ |= validator_.OnEvent(
              field.loc, GetFuncTypeIndex(field.loc, event.decl));
          break;
        }
      }
    }
  }

  // Func section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<FuncModuleField>(&field)) {
      result_ |= validator_.OnFunction(
          field.loc, GetFuncTypeIndex(field.loc, f->func.decl));
    }
  }

  // Table section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<TableModuleField>(&field)) {
      result_ |= validator_.OnTable(field.loc, f->table.elem_type,
                                    f->table.elem_limits);
    }
  }

  // Memory section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<MemoryModuleField>(&field)) {
      result_ |= validator_.OnMemory(field.loc, f->memory.page_limits);
    }
  }

  // Global section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<GlobalModuleField>(&field)) {
      result_ |=
          validator_.OnGlobal(field.loc, f->global.type, f->global.mutable_);

      if (f->global.init_expr.size() == 1) {
        const Expr* expr = &f->global.init_expr.front();

        switch (expr->type()) {
          case ExprType::Const:
            result_ |= validator_.OnGlobalInitExpr_Const(
                expr->loc, cast<ConstExpr>(expr)->const_.type());
            break;

          case ExprType::GlobalGet: {
            Var var = cast<GlobalGetExpr>(expr)->var;
            result_ |= validator_.OnGlobalInitExpr_GlobalGet(expr->loc, var);
            break;
          }

          case ExprType::RefFunc:
            result_ |= validator_.OnGlobalInitExpr_RefFunc(
                expr->loc, cast<RefFuncExpr>(expr)->var);
            break;

          case ExprType::RefNull:
            result_ |= validator_.OnGlobalInitExpr_RefNull(expr->loc);
            break;

          default:
            result_ |= validator_.OnGlobalInitExpr_Other(field.loc);
            break;
        }
      } else {
        result_ |= validator_.OnGlobalInitExpr_Other(field.loc);
      }
    }
  }

  // Event section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<EventModuleField>(&field)) {
      result_ |= validator_.OnEvent(field.loc,
                                    GetFuncTypeIndex(field.loc, f->event.decl));
    }
  }

  // Export section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<ExportModuleField>(&field)) {
      result_ |= validator_.OnExport(field.loc, f->export_.kind, f->export_.var,
                                     f->export_.name);
    }
  }

  // Start section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<StartModuleField>(&field)) {
      result_ |= validator_.OnStart(field.loc, f->start);
    }
  }

  // Elem segment section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<ElemSegmentModuleField>(&field)) {
      result_ |= validator_.OnElemSegment(field.loc, f->elem_segment.table_var,
                                          f->elem_segment.kind);

      validator_.OnElemSegmentElemType(f->elem_segment.elem_type);

      // Init expr.
      if (f->elem_segment.offset.size() == 1) {
        const Expr* expr = &f->elem_segment.offset.front();

        switch (expr->type()) {
          case ExprType::Const:
            result_ |= validator_.OnElemSegmentInitExpr_Const(
                expr->loc, cast<ConstExpr>(expr)->const_.type());
            break;

          case ExprType::GlobalGet: {
            Var var = cast<GlobalGetExpr>(expr)->var;
            result_ |=
                validator_.OnElemSegmentInitExpr_GlobalGet(expr->loc, var);
            break;
          }

          default:
            result_ |= validator_.OnElemSegmentInitExpr_Other(field.loc);
            break;
        }
      } else if (f->elem_segment.offset.size() > 1) {
        result_ |= validator_.OnElemSegmentInitExpr_Other(field.loc);
      }

      // Element expr.
      for (auto&& elem_expr : f->elem_segment.elem_exprs) {
        switch (elem_expr.kind) {
          case ElemExprKind::RefNull:
            // TODO: better location?
            result_ |= validator_.OnElemSegmentElemExpr_RefNull(field.loc);
            break;

          case ElemExprKind::RefFunc:
            result_ |= validator_.OnElemSegmentElemExpr_RefFunc(
                elem_expr.var.loc, elem_expr.var);
            break;
        }
      }
    }
  }

  // DataCount section.
  validator_.OnDataCount(module->data_segments.size());

  // Code section.
  Index func_index = module->num_func_imports;
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<FuncModuleField>(&field)) {
      result_ |= validator_.BeginFunctionBody(field.loc, func_index++);

      for (auto&& decl : f->func.local_types.decls()) {
        // TODO: Better location?
        result_ |= validator_.OnLocalDecl(field.loc, decl.second, decl.first);
      }

      ExprVisitor visitor(this);
      result_ |= visitor.VisitExprList(const_cast<ExprList&>(f->func.exprs));
      result_ |= validator_.EndFunctionBody(field.loc);
    }
  }

  // Data segment section.
  for (const ModuleField& field : module->fields) {
    if (auto* f = dyn_cast<DataSegmentModuleField>(&field)) {
      result_ |= validator_.OnDataSegment(
          field.loc, f->data_segment.memory_var, f->data_segment.kind);

      // Init expr.
      if (f->data_segment.offset.size() == 1) {
        const Expr* expr = &f->data_segment.offset.front();

        switch (expr->type()) {
          case ExprType::Const:
            result_ |= validator_.OnDataSegmentInitExpr_Const(
                expr->loc, cast<ConstExpr>(expr)->const_.type());
            break;

          case ExprType::GlobalGet: {
            Var var = cast<GlobalGetExpr>(expr)->var;
            result_ |=
                validator_.OnDataSegmentInitExpr_GlobalGet(expr->loc, var);
            break;
          }

          default:
            result_ |= validator_.OnDataSegmentInitExpr_Other(field.loc);
            break;
        }
      } else if (f->data_segment.offset.size() > 1) {
        result_ |= validator_.OnDataSegmentInitExpr_Other(field.loc);
      }
    }
  }

  result_ |= validator_.EndModule();

  return result_;
}

// Returns the result type of the invoked function, checked by the caller;
// returning nullptr means that another error occured first, so the result type
// should be ignored.
ScriptValidator::ActionResult ScriptValidator::CheckInvoke(
    const InvokeAction* action) {
  const Module* module = script_->GetModule(action->module_var);
  if (!module) {
    PrintError(&action->loc, "unknown module");
    return Result::Error;
  }

  const Export* export_ = module->GetExport(action->name);
  if (!export_) {
    PrintError(&action->loc, "unknown function export \"%s\"",
               action->name.c_str());
    return Result::Error;
  }

  const Func* func = module->GetFunc(export_->var);
  if (!func) {
    // This error will have already been reported, just skip it.
    return Result::Error;
  }

  size_t actual_args = action->args.size();
  size_t expected_args = func->GetNumParams();
  if (expected_args != actual_args) {
    PrintError(&action->loc,
               "too %s parameters to function. got %" PRIzd
               ", expected %" PRIzd,
               actual_args > expected_args ? "many" : "few", actual_args,
               expected_args);
    return Result::Error;
  }
  for (size_t i = 0; i < actual_args; ++i) {
    const Const* const_ = &action->args[i];
    CheckTypeIndex(&const_->loc, const_->type(), func->GetParamType(i),
                   "invoke", i, "argument");
  }

  return ActionResult{Result::Ok, ToTypeVector(func->decl.sig.result_types)};
}

ScriptValidator::ActionResult ScriptValidator::CheckGet(
    const GetAction* action) {
  const Module* module = script_->GetModule(action->module_var);
  if (!module) {
    PrintError(&action->loc, "unknown module");
    return Result::Error;
  }

  const Export* export_ = module->GetExport(action->name);
  if (!export_) {
    PrintError(&action->loc, "unknown global export \"%s\"",
               action->name.c_str());
    return Result::Error;
  }

  const Global* global = module->GetGlobal(export_->var);
  if (!global) {
    // This error will have already been reported, just skip it.
    return Result::Error;
  }

  return ActionResult{Result::Ok, {global->type}};
}

ScriptValidator::ActionResult ScriptValidator::CheckAction(
    const Action* action) {
  switch (action->type()) {
    case ActionType::Invoke:
      return CheckInvoke(cast<InvokeAction>(action));

    case ActionType::Get:
      return CheckGet(cast<GetAction>(action));

    default:
      return Result::Error;
  }
}

void ScriptValidator::CheckCommand(const Command* command) {
  switch (command->type) {
    case CommandType::Module: {
      Validator module_validator(errors_, &cast<ModuleCommand>(command)->module,
                                 options_);
      module_validator.CheckModule();
      break;
    }

    case CommandType::Action:
      // Ignore result type.
      CheckAction(cast<ActionCommand>(command)->action.get());
      break;

    case CommandType::Register:
    case CommandType::AssertMalformed:
    case CommandType::AssertInvalid:
    case CommandType::AssertUnlinkable:
    case CommandType::AssertUninstantiable:
      // Ignore.
      break;

    case CommandType::AssertReturn: {
      auto* assert_return_command = cast<AssertReturnCommand>(command);
      const Action* action = assert_return_command->action.get();
      ActionResult result = CheckAction(action);
      // Here we take the concrete expected output types verify those actains
      // the types that are the result of the action.
      TypeVector actual_types;
      for (auto ex : assert_return_command->expected) {
        actual_types.push_back(ex.type());
      }
      if (Succeeded(result.result)) {
        CheckResultTypes(&action->loc, actual_types, result.types, "action");
      }
      break;
    }

    case CommandType::AssertTrap:
      // ignore result type.
      CheckAction(cast<AssertTrapCommand>(command)->action.get());
      break;
    case CommandType::AssertExhaustion:
      // ignore result type.
      CheckAction(cast<AssertExhaustionCommand>(command)->action.get());
      break;
  }
}

Result ScriptValidator::CheckScript() {
  for (const std::unique_ptr<Command>& command : script_->commands)
    CheckCommand(command.get());
  return result_;
}

}  // end anonymous namespace

Result ValidateScript(const Script* script,
                      Errors* errors,
                      const ValidateOptions& options) {
  ScriptValidator validator(errors, script, options);

  return validator.CheckScript();
}

Result ValidateModule(const Module* module,
                      Errors* errors,
                      const ValidateOptions& options) {
  Validator validator(errors, module, options);

  return validator.CheckModule();
}

}  // namespace wabt
