// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_EXTERNAL_WEIGHTS_MANAGER_H_
#define SERVICES_WEBNN_ORT_EXTERNAL_WEIGHTS_MANAGER_H_

#include "absl/container/flat_hash_set.h"
#include "base/containers/heap_array.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

// `ExternalWeightsManager` is used for letting ORT manage the lifecycle of
// the external weights by itself so that the external weights can be released
// promptly when they are no longer needed. `ExternalWeightsManager` must not
// be destroyed until all the external weights have been released.
class ExternalWeightsManager : public OrtAllocator {
 public:
  ExternalWeightsManager();
  ExternalWeightsManager(const ExternalWeightsManager&) = delete;
  ExternalWeightsManager& operator=(const ExternalWeightsManager&) = delete;
  ~ExternalWeightsManager();

  // Create an initializer with external data which is stored in
  // `ExternalWeightsManager` and will be kept alive until ORT calls
  // `FreeImpl()` to release it.
  ScopedOrtValue CreateInitializer(base::HeapArray<uint8_t> data,
                                   base::span<const int64_t> shape,
                                   ONNXTensorElementDataType data_type);

 private:
  void FreeImpl(void* p);

  ScopedOrtMemoryInfo cpu_memory_info_;

  struct ExternalWeightsHash {
    using is_transparent = void;
    size_t operator()(const base::HeapArray<uint8_t>& weights) const;
    size_t operator()(const void* p) const;
  };
  struct ExternalWeightsEqual {
    using is_transparent = void;
    bool operator()(const base::HeapArray<uint8_t>& lhs,
                    const base::HeapArray<uint8_t>& rhs) const;
    bool operator()(const base::HeapArray<uint8_t>& lhs, const void* rhs) const;
  };
  absl::flat_hash_set<base::HeapArray<uint8_t>,
                      ExternalWeightsHash,
                      ExternalWeightsEqual>
      external_weights_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_EXTERNAL_WEIGHTS_MANAGER_H_
