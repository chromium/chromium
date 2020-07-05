// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_IENN_SYMBOL_TABLE_H_
#define SERVICES_ML_IENN_SYMBOL_TABLE_H_

#include "services/ml/late_binding_symbol_table.h"

namespace ml {

// The ienn symbols we need, as an X-Macro list.
#define IE_SYMBOLS_LIST               \
  X(ie_model_create)                  \
  X(ie_model_free)                    \
  X(ie_model_add_operand)             \
  X(ie_model_set_operand_value)       \
  X(ie_model_add_operation)           \
  X(ie_model_identify_inputs_outputs) \
  X(ie_compilation_create)            \
  X(ie_compilation_set_preference)    \
  X(ie_compilation_finish)            \
  X(ie_compilation_free)              \
  X(ie_execution_create)              \
  X(ie_execution_set_input)           \
  X(ie_execution_set_output)          \
  X(ie_execution_start_compute)       \
  X(ie_execution_free)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(IESymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(IESymbolTable, sym)
IE_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(IESymbolTable)

IESymbolTable* GetIESymbolTable();

#if defined(OS_WIN) || defined(OS_LINUX)
#define IE(sym) LATESYM_GET(IESymbolTable, GetIESymbolTable(), sym)
#else
#define IE(sym) sym
#endif

}  // namespace ml

#endif  // SERVICES_ML_IENN_SYMBOL_TABLE_H_
