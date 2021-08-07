// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExecutionContext;

class GPUTextureUsage : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_texture_usage.idl
  static constexpr uint32_t kCopySrc = 1;
  static constexpr uint32_t kCopyDst = 2;
  static constexpr uint32_t kTextureBinding = 4;
  static constexpr uint32_t kStorageBinding = 8;
  static constexpr uint32_t kRenderAttachment = 16;

  static unsigned SAMPLED(ExecutionContext* execution_context);
  static unsigned STORAGE(ExecutionContext* execution_context);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUTextureUsage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_
