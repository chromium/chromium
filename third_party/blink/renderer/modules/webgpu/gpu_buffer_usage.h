// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_USAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_USAGE_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_usage.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUBufferUsage : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_buffer_usage.idl
  static constexpr uint32_t kMapRead = V8GPUBufferUsage::Constant::kMapRead;
  static constexpr uint32_t kMapWrite = V8GPUBufferUsage::Constant::kMapWrite;
  static constexpr uint32_t kCopySrc = V8GPUBufferUsage::Constant::kCopySrc;
  static constexpr uint32_t kCopyDst = V8GPUBufferUsage::Constant::kCopyDst;
  static constexpr uint32_t kIndex = V8GPUBufferUsage::Constant::kIndex;
  static constexpr uint32_t kVertex = V8GPUBufferUsage::Constant::kVertex;
  static constexpr uint32_t kUniform = V8GPUBufferUsage::Constant::kUniform;
  static constexpr uint32_t kStorage = V8GPUBufferUsage::Constant::kStorage;
  static constexpr uint32_t kIndirect = V8GPUBufferUsage::Constant::kIndirect;
  static constexpr uint32_t kQueryResolve =
      V8GPUBufferUsage::Constant::kQueryResolve;

  GPUBufferUsage(const GPUBufferUsage&) = delete;
  GPUBufferUsage& operator=(const GPUBufferUsage&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_BUFFER_USAGE_H_
