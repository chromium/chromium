// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_MKL_DNN_SYMBOL_TABLE_H_
#define SERVICES_ML_MKL_DNN_SYMBOL_TABLE_H_

#include "services/ml/late_binding_symbol_table.h"

namespace ml {

// The mkldnn symbols we need, as an X-Macro list.
// This list must contain precisely every mkldnn function that is used in
// the LINUX Device.

#define MKL_DNN_SYMBOLS_LIST                      \
  X(mkldnn_engine_create)                         \
  X(mkldnn_engine_destroy)                        \
  X(mkldnn_memory_desc_init)                      \
  X(mkldnn_memory_primitive_desc_create)          \
  X(mkldnn_memory_primitive_desc_equal)           \
  X(mkldnn_memory_primitive_desc_get_size)        \
  X(mkldnn_memory_get_data_handle)                \
  X(mkldnn_memory_set_data_handle)                \
  X(mkldnn_primitive_desc_create)                 \
  X(mkldnn_primitive_desc_create_v2)              \
  X(mkldnn_primitive_desc_destroy)                \
  X(mkldnn_primitive_desc_query_pd)               \
  X(mkldnn_primitive_desc_query_memory_d)         \
  X(mkldnn_convolution_forward_desc_init)         \
  X(mkldnn_dilated_convolution_forward_desc_init) \
  X(mkldnn_pooling_forward_desc_init)             \
  X(mkldnn_reorder_primitive_desc_create)         \
  X(mkldnn_primitive_create)                      \
  X(mkldnn_primitive_get_primitive_desc)          \
  X(mkldnn_primitive_at)                          \
  X(mkldnn_primitive_destroy)                     \
  X(mkldnn_stream_create)                         \
  X(mkldnn_stream_submit)                         \
  X(mkldnn_stream_wait)                           \
  X(mkldnn_stream_destroy)                        \
  X(mkldnn_post_ops_create)                       \
  X(mkldnn_post_ops_destroy)                      \
  X(mkldnn_post_ops_append_eltwise)               \
  X(mkldnn_primitive_attr_create)                 \
  X(mkldnn_primitive_attr_destroy)                \
  X(mkldnn_primitive_attr_set_post_ops)           \
  X(mkldnn_eltwise_forward_desc_init)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(MklDnnSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(MklDnnSymbolTable, sym)
MKL_DNN_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(MklDnnSymbolTable)

MklDnnSymbolTable* GetMklDnnSymbolTable();

#if defined(OS_LINUX)
#define LATE(sym) LATESYM_GET(MklDnnSymbolTable, GetMklDnnSymbolTable(), sym)
#else
#define LATE(sym) sym
#endif

}  // namespace ml

#endif  // SERVICES_ML_MKL_DNN_SYMBOL_TABLE_H_
