// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/scoped_ort_types.h"

namespace webnn::ort {

#define SCOPED_ORT_TYPE_PTR_DEFINITION(ort_type, ort_api)        \
  ScopedOrt##ort_type##Ptr::ScopedOrt##ort_type##Ptr() {         \
    pptr_ = std::make_unique<Ort##ort_type*>(nullptr);           \
  }                                                              \
  ScopedOrt##ort_type##Ptr::~ScopedOrt##ort_type##Ptr() {        \
    if (pptr_) {                                                 \
      Get##ort_api()->Release##ort_type(*pptr_);                 \
    }                                                            \
  }                                                              \
  ScopedOrt##ort_type##Ptr::ScopedOrt##ort_type##Ptr(            \
      ScopedOrt##ort_type##Ptr&&) = default;                     \
  ScopedOrt##ort_type##Ptr& ScopedOrt##ort_type##Ptr::operator=( \
      ScopedOrt##ort_type##Ptr&&) = default;

SCOPED_ORT_TYPE_PTR_DEFINITION(Env, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(Session, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(SessionOptions, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(Value, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(MemoryInfo, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(OpAttr, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(TypeInfo, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(TensorTypeAndShapeInfo, OrtApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(ValueInfo, OrtModelBuilderApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(Node, OrtModelBuilderApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(Graph, OrtModelBuilderApi)
SCOPED_ORT_TYPE_PTR_DEFINITION(Model, OrtModelBuilderApi)

}  // namespace webnn::ort
