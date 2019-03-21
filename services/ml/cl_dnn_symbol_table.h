// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_CL_DNN_SYMBOL_TABLE_H_
#define SERVICES_ML_CL_DNN_SYMBOL_TABLE_H_

#include "services/ml/late_binding_symbol_table.h"

namespace ml {

// The clDNN symbols we need, as an X-Macro list.
// This list must contain precisely every libclDNN function that is used in
// the LINUX Device.

#define CL_DNN_SYMBOLS_LIST             \
  X(cldnn_get_version)                  \
  X(cldnn_get_engine_count)             \
  X(cldnn_get_last_error_message)       \
  X(cldnn_get_engine_info)              \
  X(cldnn_create_engine)                \
  X(cldnn_create_topology)              \
  X(cldnn_release_memory)               \
  X(cldnn_release_topology)             \
  X(cldnn_release_engine)               \
  X(cldnn_input_layout_type_id)         \
  X(cldnn_reorder_type_id)              \
  X(cldnn_add_primitive)                \
  X(cldnn_allocate_memory)              \
  X(cldnn_lock_memory)                  \
  X(cldnn_unlock_memory)                \
  X(cldnn_data_type_id)                 \
  X(cldnn_activation_type_id)           \
  X(cldnn_eltwise_type_id)              \
  X(cldnn_convolution_type_id)          \
  X(cldnn_pooling_type_id)              \
  X(cldnn_softmax_type_id)              \
  X(cldnn_reshape_type_id)              \
  X(cldnn_fully_connected_type_id)      \
  X(cldnn_concatenation_type_id)        \
  X(cldnn_custom_gpu_primitive_type_id) \
  X(cldnn_release_program)              \
  X(cldnn_build_program)                \
  X(cldnn_attach_memory)                \
  X(cldnn_allocate_network)             \
  X(cldnn_release_network)              \
  X(cldnn_set_network_input)            \
  X(cldnn_execute_network)              \
  X(cldnn_get_network_output_memory)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(ClDnnSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(ClDnnSymbolTable, sym)
CL_DNN_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(ClDnnSymbolTable)

ClDnnSymbolTable* GetClDnnSymbolTable();

// Accesses clDNN functions through our late-binding symbol table instead of
// directly. This way we don't have to link to libclDNN, which means our binary
// will work on systems that don't have it.
#if defined(OS_LINUX)
#define LATE(sym) LATESYM_GET(ClDnnSymbolTable, GetClDnnSymbolTable(), sym)
#else
#define LATE(sym) sym
#endif

}  // namespace ml

#endif  // SERVICES_ML_CL_DNN_SYMBOL_TABLE_H_
