// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/dnnl_symbol_table.h"

#include "build/build_config.h"

namespace ml {

#if defined(OS_LINUX)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(DnnlSymbolTable, "libdnnl.so")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(DnnlSymbolTable, sym)
DNNL_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(DnnlSymbolTable)
#endif
#if defined(__APPLE__)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(DnnlSymbolTable, "libdnnl.1.1.dylib")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(DnnlSymbolTable, sym)
DNNL_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(DnnlSymbolTable)
#endif

DnnlSymbolTable* GetDnnlSymbolTable() {
  static DnnlSymbolTable* dnnl_symbol_table = new DnnlSymbolTable();
  return dnnl_symbol_table;
}

}  // namespace ml
