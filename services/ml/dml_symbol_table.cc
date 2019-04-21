// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/dml_symbol_table.h"

namespace ml {

// // The D3D symbols.
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(D3DSymbolTable, "d3d12.dll")
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DEFINE_ENTRY(D3DSymbolTable, sym)
D3D_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DEFINE_END(D3DSymbolTable)

D3DSymbolTable* GetD3DSymbolTable() {
  static D3DSymbolTable* dml_symbol_table = new D3DSymbolTable();
  dml_symbol_table->Load();
  return dml_symbol_table;
}

// The DirectML symbols.
LATE_BINDING_SYMBOL_TABLE_DEFINE_BEGIN(DMLSymbolTable, "directml.dll")
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
