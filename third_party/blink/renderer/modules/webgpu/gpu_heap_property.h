// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_HEAP_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_HEAP_PROPERTY_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_heap_property.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUHeapProperty : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_heap_property.idl
  static constexpr uint32_t kDeviceLocal =
      V8GPUHeapProperty::Constant::kDeviceLocal;
  static constexpr uint32_t kHostVisible =
      V8GPUHeapProperty::Constant::kHostVisible;
  static constexpr uint32_t kHostCoherent =
      V8GPUHeapProperty::Constant::kHostCoherent;
  static constexpr uint32_t kHostUncached =
      V8GPUHeapProperty::Constant::kHostUncached;
  static constexpr uint32_t kHostCached =
      V8GPUHeapProperty::Constant::kHostCached;

  GPUHeapProperty(const GPUHeapProperty&) = delete;
  GPUHeapProperty& operator=(const GPUHeapProperty&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_HEAP_PROPERTY_H_
