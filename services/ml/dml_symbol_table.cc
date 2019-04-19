// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/dml_symbol_table.h"

namespace ml {

LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(DMLSymbolTable, "d3d12.dll")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(DMLSymbolTable, sym)
DML_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(DMLSymbolTable)

DMLSymbolTable* GetDMLSymbolTable() {
  static DMLSymbolTable* dml_symbol_table = new DMLSymbolTable();
  dml_symbol_table->Load();
  return dml_symbol_table;
}

}  // namespace ml
