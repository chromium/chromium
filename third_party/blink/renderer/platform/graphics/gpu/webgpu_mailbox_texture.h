// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_

#include <dawn/webgpu.h>

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class CanvasResource;
class DawnControlClientHolder;
class StaticBitmapImage;

class PLATFORM_EXPORT WebGPUMailboxTexture
    : public RefCounted<WebGPUMailboxTexture> {
 public:
  static scoped_refptr<WebGPUMailboxTexture> FromStaticBitmapImage(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureUsage usage,
      scoped_refptr<StaticBitmapImage> image);

  static scoped_refptr<WebGPUMailboxTexture> FromCanvasResource(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureUsage usage,
      scoped_refptr<CanvasResource> canvas_resource);

  ~WebGPUMailboxTexture();

  WGPUTexture GetTexture() { return texture_; }
  uint32_t GetTextureIdForTest() { return wire_texture_id_; }
  uint32_t GetTextureGenerationForTest() { return wire_texture_generation_; }
  WGPUDevice GetDeviceForTest() { return device_; }

 private:
  WebGPUMailboxTexture(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureUsage usage,
      const gpu::Mailbox& mailbox,
      const gpu::SyncToken& sync_token,
      base::OnceCallback<void(const gpu::SyncToken&)> destroy_callback);

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  WGPUDevice device_;
  base::OnceCallback<void(const gpu::SyncToken&)> destroy_callback_;
  WGPUTexture texture_;
  uint32_t wire_texture_id_ = 0;
  uint32_t wire_texture_generation_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_
