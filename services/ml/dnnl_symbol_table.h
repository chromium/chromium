// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DNNL_SYMBOL_TABLE_H_
#define SERVICES_DNNL_SYMBOL_TABLE_H_

#include "services/ml/late_binding_symbol_table.h"

namespace ml {

// The dnnl symbols we need, as an X-Macro list.
// This list must contain precisely every mkldnn function that is used in
// the LINUX Device.

#define DNNL_SYMBOLS_LIST                       \
  X(dnnl_engine_create)                         \
  X(dnnl_engine_destroy)                        \
  X(dnnl_memory_create)                         \
  X(dnnl_memory_desc_equal)                     \
  X(dnnl_memory_desc_get_size)                  \
  X(dnnl_memory_desc_init_by_tag)               \
  X(dnnl_memory_destroy)                        \
  X(dnnl_memory_get_memory_desc)                \
  X(dnnl_memory_get_data_handle)                \
  X(dnnl_memory_set_data_handle)                \
  X(dnnl_primitive_desc_create)                 \
  X(dnnl_primitive_desc_destroy)                \
  X(dnnl_primitive_desc_query_md)               \
  X(dnnl_primitive_execute)                     \
  X(dnnl_convolution_forward_desc_init)         \
  X(dnnl_dilated_convolution_forward_desc_init) \
  X(dnnl_pooling_forward_desc_init)             \
  X(dnnl_softmax_forward_desc_init)             \
  X(dnnl_inner_product_forward_desc_init)       \
  X(dnnl_reorder_primitive_desc_create)         \
  X(dnnl_sum_primitive_desc_create)             \
  X(dnnl_concat_primitive_desc_create)          \
  X(dnnl_primitive_create)                      \
  X(dnnl_primitive_get_primitive_desc)          \
  X(dnnl_primitive_destroy)                     \
  X(dnnl_stream_create)                         \
  X(dnnl_stream_wait)                           \
  X(dnnl_stream_destroy)                        \
  X(dnnl_post_ops_create)                       \
  X(dnnl_post_ops_destroy)                      \
  X(dnnl_post_ops_append_eltwise)               \
  X(dnnl_primitive_attr_create)                 \
  X(dnnl_primitive_attr_destroy)                \
  X(dnnl_primitive_attr_set_post_ops)           \
  X(dnnl_eltwise_forward_desc_init)

LATE_BINDING_SYMBOL_TABLE_DECLARE_BEGIN(DnnlSymbolTable)
#define X(sym) LATE_BINDING_SYMBOL_TABLE_DECLARE_ENTRY(DnnlSymbolTable, sym)
DNNL_SYMBOLS_LIST
#undef X
LATE_BINDING_SYMBOL_TABLE_DECLARE_END(DnnlSymbolTable)

DnnlSymbolTable* GetDnnlSymbolTable();

#if defined(OS_LINUX) || defined(OS_MACOSX)
#define LATE(sym) LATESYM_GET(DnnlSymbolTable, GetDnnlSymbolTable(), sym)
#else
#define LATE(sym) sym
#endif

}  // namespace ml

#endif  // SERVICES_DNNL_SYMBOL_TABLE_H_
