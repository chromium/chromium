// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUTextureUsage : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_texture_usage.idl
  static constexpr uint32_t kCopySrc = 1;
  static constexpr uint32_t kCopyDst = 2;
  static constexpr uint32_t kSampled = 4;
  static constexpr uint32_t kStorage = 8;
  static constexpr uint32_t kOutputAttachment = 16;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUTextureUsage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_TEXTURE_USAGE_H_
