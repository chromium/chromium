// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_IMAGE_BITMAP_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_IMAGE_BITMAP_HANDLER_H_

#include <dawn/webgpu.h>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

struct WebGPUImageUploadSizeInfo {
  uint64_t size_in_bytes;
  uint32_t wgpu_bytes_per_row;
};

class IntRect;
class StaticBitmapImage;

WebGPUImageUploadSizeInfo PLATFORM_EXPORT
ComputeImageBitmapWebGPUUploadSizeInfo(
    const IntRect& rect,
    const WGPUTextureFormat& destination_format);
bool PLATFORM_EXPORT
CopyBytesFromImageBitmapForWebGPU(scoped_refptr<StaticBitmapImage> image,
                                  base::span<uint8_t> dst,
                                  const IntRect& rect,
                                  const WGPUTextureFormat destination_format,
                                  bool premultipliedAlpha,
                                  bool flipY);

uint64_t PLATFORM_EXPORT
DawnTextureFormatBytesPerPixel(const WGPUTextureFormat color_type);

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_IMAGE_BITMAP_HANDLER_H_
