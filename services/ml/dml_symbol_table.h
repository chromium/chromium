// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_D3D_SYMBOL_TABLE_H_
#define SERVICES_ML_D3D_SYMBOL_TABLE_H_

#include "services/ml/late_binding_symbol_table.h"

namespace ml {

// The d3d12 symbols we need, as an X-Macro list.
// This list must contain precisely every d3d function that is used in
// the Windows Device.
#define D3D_SYMBOLS_LIST         \
  X(D3D12CreateDevice)           \
  X(D3D12SerializeRootSignature) \
  X(D3D12GetDebugInterface)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(D3DSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(D3DSymbolTable, sym)
D3D_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(D3DSymbolTable)

D3DSymbolTable* GetD3DSymbolTable();

#if defined(OS_WIN)
#define D3D(sym) LATESYM_GET(D3DSymbolTable, GetD3DSymbolTable(), sym)
#else
#define D3D(sym) sym
#endif

// The DirectML symbols we need, as an X-Macro list.
// This list must contain precisely every directML function that is used in
// the Windows Device.
#define DML_SYMBOLS_LIST X(DMLCreateDevice)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(DMLSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(DMLSymbolTable, sym)
DML_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(DMLSymbolTable)

DMLSymbolTable* GetDMLSymbolTable();

#if defined(OS_WIN)
#define DML(sym) LATESYM_GET(DMLSymbolTable, GetDMLSymbolTable(), sym)
#else
#define DML(sym) sym
#endif
}  // namespace ml

#endif  // SERVICES_ML_D3D_SYMBOL_TABLE_H_
