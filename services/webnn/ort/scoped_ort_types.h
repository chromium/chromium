// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H
#define SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H

#include <memory>

#include "services/webnn/ort/utils_ort.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

#define SCOPED_ORT_TYPE_PTR_DECLARATION(ort_type)                          \
  class ScopedOrt##ort_type##Ptr {                                         \
   public:                                                                 \
    ScopedOrt##ort_type##Ptr();                                            \
    ~ScopedOrt##ort_type##Ptr();                                           \
    ScopedOrt##ort_type##Ptr(const ScopedOrt##ort_type##Ptr&) = delete;    \
    ScopedOrt##ort_type##Ptr& operator=(const ScopedOrt##ort_type##Ptr&) = \
        delete;                                                            \
    ScopedOrt##ort_type##Ptr(ScopedOrt##ort_type##Ptr&&);                  \
    ScopedOrt##ort_type##Ptr& operator=(ScopedOrt##ort_type##Ptr&&);       \
    operator Ort##ort_type *() const {                                     \
      return *pptr_;                                                       \
    }                                                                      \
    Ort##ort_type* Get() const {                                           \
      return *pptr_;                                                       \
    }                                                                      \
    Ort##ort_type** GetAddressOf() const {                                 \
      return pptr_.get();                                                  \
    }                                                                      \
    Ort##ort_type* Release() {                                             \
      return *pptr_.release();                                             \
    }                                                                      \
                                                                           \
   private:                                                                \
    std::unique_ptr<Ort##ort_type*> pptr_;                                 \
  };

SCOPED_ORT_TYPE_PTR_DECLARATION(Value)
SCOPED_ORT_TYPE_PTR_DECLARATION(MemoryInfo)
SCOPED_ORT_TYPE_PTR_DECLARATION(OpAttr)
SCOPED_ORT_TYPE_PTR_DECLARATION(TypeInfo)
SCOPED_ORT_TYPE_PTR_DECLARATION(TensorTypeAndShapeInfo)
SCOPED_ORT_TYPE_PTR_DECLARATION(ValueInfo)
SCOPED_ORT_TYPE_PTR_DECLARATION(Node)
SCOPED_ORT_TYPE_PTR_DECLARATION(Graph)
SCOPED_ORT_TYPE_PTR_DECLARATION(Model)

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H
