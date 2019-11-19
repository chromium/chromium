// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COLOR_WRITE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COLOR_WRITE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUColorWrite : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_color_write.idl
  static constexpr uint32_t kRed = 1;
  static constexpr uint32_t kGreen = 2;
  static constexpr uint32_t kBlue = 4;
  static constexpr uint32_t kAlpha = 8;
  static constexpr uint32_t kAll = 15;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUColorWrite);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COLOR_WRITE_H_
