// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_usage.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUTextureUsage : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_texture_usage.idl
  static constexpr uint32_t kCopySrc = V8GPUTextureUsage::Constant::kCopySrc;
  static constexpr uint32_t kCopyDst = V8GPUTextureUsage::Constant::kCopyDst;
  static constexpr uint32_t kTextureBinding =
      V8GPUTextureUsage::Constant::kTextureBinding;
  static constexpr uint32_t kStorageBinding =
      V8GPUTextureUsage::Constant::kStorageBinding;
  static constexpr uint32_t kRenderAttachment =
      V8GPUTextureUsage::Constant::kRenderAttachment;

  GPUTextureUsage(const GPUTextureUsage&) = delete;
  GPUTextureUsage& operator=(const GPUTextureUsage&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_
