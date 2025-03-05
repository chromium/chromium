// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H_
#define SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H_

#include <type_traits>

#include "base/scoped_generic.h"
#include "services/webnn/ort/utils_ort.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

namespace internal {

template <typename T>
  requires std::is_pointer<T>::value
struct ScopedOrtTypeTraitsHelper;

template <typename T>
  requires std::is_pointer<T>::value
struct ScopedOrtTypeTraits {
  static T InvalidValue() { return nullptr; }
  static void Free(T value) { ScopedOrtTypeTraitsHelper<T>::Free(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtEnv*> {
  static void Free(OrtEnv* value) { GetOrtApi()->ReleaseEnv(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtSession*> {
  static void Free(OrtSession* value) { GetOrtApi()->ReleaseSession(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtSessionOptions*> {
  static void Free(OrtSessionOptions* value) {
    GetOrtApi()->ReleaseSessionOptions(value);
  }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtStatus*> {
  static void Free(OrtStatus* value) { GetOrtApi()->ReleaseStatus(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtValue*> {
  static void Free(OrtValue* value) { GetOrtApi()->ReleaseValue(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtMemoryInfo*> {
  static void Free(OrtMemoryInfo* value) {
    GetOrtApi()->ReleaseMemoryInfo(value);
  }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtOpAttr*> {
  static void Free(OrtOpAttr* value) { GetOrtApi()->ReleaseOpAttr(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtTypeInfo*> {
  static void Free(OrtTypeInfo* value) { GetOrtApi()->ReleaseTypeInfo(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtTensorTypeAndShapeInfo*> {
  static void Free(OrtTensorTypeAndShapeInfo* value) {
    GetOrtApi()->ReleaseTensorTypeAndShapeInfo(value);
  }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtValueInfo*> {
  static void Free(OrtValueInfo* value) {
    GetOrtApi()->ReleaseValueInfo(value);
  }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtNode*> {
  static void Free(OrtNode* value) { GetOrtApi()->ReleaseNode(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtGraph*> {
  static void Free(OrtGraph* value) { GetOrtApi()->ReleaseGraph(value); }
};

template <>
struct ScopedOrtTypeTraitsHelper<OrtModel*> {
  static void Free(OrtModel* value) { GetOrtApi()->ReleaseModel(value); }
};

template <typename T>
  requires std::is_pointer<T>::value
using ScopedOrtType = base::ScopedGeneric<T, ScopedOrtTypeTraits<T>>;

}  // namespace internal

using ScopedOrtEnv = internal::ScopedOrtType<OrtEnv*>;
using ScopedOrtSession = internal::ScopedOrtType<OrtSession*>;
using ScopedOrtSessionOptions = internal::ScopedOrtType<OrtSessionOptions*>;
using ScopedOrtStatus = internal::ScopedOrtType<OrtStatus*>;
using ScopedOrtValue = internal::ScopedOrtType<OrtValue*>;
using ScopedOrtMemoryInfo = internal::ScopedOrtType<OrtMemoryInfo*>;
using ScopedOrtOpAttr = internal::ScopedOrtType<OrtOpAttr*>;
using ScopedOrtTypeInfo = internal::ScopedOrtType<OrtTypeInfo*>;
using ScopedOrtTensorTypeAndShapeInfo =
    internal::ScopedOrtType<OrtTensorTypeAndShapeInfo*>;
using ScopedOrtValueInfo = internal::ScopedOrtType<OrtValueInfo*>;
using ScopedOrtNode = internal::ScopedOrtType<OrtNode*>;
using ScopedOrtGraph = internal::ScopedOrtType<OrtGraph*>;
using ScopedOrtModel = internal::ScopedOrtType<OrtModel*>;

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_SCOPED_ORT_TYPES_H_
