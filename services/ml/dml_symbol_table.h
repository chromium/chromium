// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_DML_SYMBOL_TABLE_H_
#define SERVICES_ML_DML_SYMBOL_TABLE_H_

#include "services/ml/late_binding_symbol_table.h"

namespace ml {

// The d3d12 symbols we need, as an X-Macro list.
// This list must contain precisely every mkldnn function that is used in
// the LINUX Device.

#define DML_SYMBOLS_LIST X(D3D12CreateDevice)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(DMLSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(DMLSymbolTable, sym)
DML_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(DMLSymbolTable)

DMLSymbolTable* GetDMLSymbolTable();

#if defined(OS_WIN)
#define LATE(sym) LATESYM_GET(DMLSymbolTable, GetDMLSymbolTable(), sym)
#else
#define LATE(sym) sym
#endif

}  // namespace ml

#endif  // SERVICES_ML_DML_SYMBOL_TABLE_H_
