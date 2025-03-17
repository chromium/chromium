// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_CALCULATORS_MEDIAPIPE_WEBGPU_UTILS_H_
#define SERVICES_VIDEO_EFFECTS_CALCULATORS_MEDIAPIPE_WEBGPU_UTILS_H_

#include "base/notreached.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer_format.h"

namespace video_effects {

constexpr mediapipe::GpuBufferFormat WebGpuTextureFormatToGpuBufferFormat(
    wgpu::TextureFormat format) {
  switch (format) {
    case wgpu::TextureFormat::RGBA8Unorm:
      return mediapipe::GpuBufferFormat::kRGBA32;
    case wgpu::TextureFormat::RGBA32Float:
      return mediapipe::GpuBufferFormat::kRGBAFloat128;
    default:
      NOTREACHED();
  }
}

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_CALCULATORS_MEDIAPIPE_WEBGPU_UTILS_H_
