// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_MAP_MODE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_MAP_MODE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class GPUMapMode : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // gpu_map_mode.idl
  static constexpr uint32_t kRead = 1;
  static constexpr uint32_t kWrite = 2;

  GPUMapMode(const GPUMapMode&) = delete;
  GPUMapMode& operator=(const GPUMapMode&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_MAP_MODE_H_
