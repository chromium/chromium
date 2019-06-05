// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/mkl_dnn_symbol_table.h"

#include "build/build_config.h"

namespace ml {

#if defined(OS_LINUX)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(MklDnnSymbolTable, "libmkldnn.so")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(MklDnnSymbolTable, sym)
MKL_DNN_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(MklDnnSymbolTable)
#endif
#if defined(__APPLE__)
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(MklDnnSymbolTable, "libmkldnn.0.dylib")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(MklDnnSymbolTable, sym)
MKL_DNN_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(MklDnnSymbolTable)
#endif

MklDnnSymbolTable* GetMklDnnSymbolTable() {
  static MklDnnSymbolTable* mkl_dnn_symbol_table = new MklDnnSymbolTable();
  return mkl_dnn_symbol_table;
}

}  // namespace ml
