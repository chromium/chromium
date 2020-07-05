// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/ienn_symbol_table.h"

namespace ml {

// The ie_nn_c_api symbols.
#if defined(__linux__)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(IESymbolTable, "libie_nn_c_api.so")
#elif defined(_WIN32) || defined(_WIN64)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(IESymbolTable, "ie_nn_c_api.dll")
#endif
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(IESymbolTable, sym)
IE_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(IESymbolTable)

IESymbolTable* GetIESymbolTable() {
  static IESymbolTable* ienn_symbol_table = new IESymbolTable();
  ienn_symbol_table->Load();
  return ienn_symbol_table;
}

}  // namespace ml
