// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_IMAGE_BITMAP_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_IMAGE_BITMAP_HANDLER_H_

#include <dawn/webgpu.h>

#include "base/containers/span.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
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
                                  const WGPUTextureFormat destination_format);

uint64_t PLATFORM_EXPORT
DawnTextureFormatBytesPerPixel(const WGPUTextureFormat color_type);

class PLATFORM_EXPORT DawnTextureFromImageBitmap
    : public RefCounted<DawnTextureFromImageBitmap> {
 public:
  DawnTextureFromImageBitmap(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      uint64_t device_client_id);

  ~DawnTextureFromImageBitmap();
  WGPUTexture ProduceDawnTextureFromImageBitmap(
      scoped_refptr<StaticBitmapImage> image);
  void FinishDawnTextureFromImageBitmapAccess();

  uint32_t GetTextureIdForTest() { return wire_texture_id_; }
  uint32_t GetTextureGenerationForTest() { return wire_texture_generation_; }
  uint64_t GetDeviceClientIdForTest() { return device_client_id_; }

 private:
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  gpu::Mailbox associated_resource_;
  uint64_t device_client_id_;
  uint32_t wire_texture_id_ = 0;
  uint32_t wire_texture_generation_ = 0;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_IMAGE_BITMAP_HANDLER_H_
