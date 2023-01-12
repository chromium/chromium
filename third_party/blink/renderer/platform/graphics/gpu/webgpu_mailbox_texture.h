// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_

#include <dawn/webgpu.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace blink {

class DawnControlClientHolder;
class StaticBitmapImage;
class WebGPUTextureAlphaClearer;

class PLATFORM_EXPORT WebGPUMailboxTexture
    : public RefCounted<WebGPUMailboxTexture> {
 public:
  static scoped_refptr<WebGPUMailboxTexture> FromStaticBitmapImage(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureUsage usage,
      scoped_refptr<StaticBitmapImage> image,
      SkColorType color_type);

  static scoped_refptr<WebGPUMailboxTexture> FromCanvasResource(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureUsage usage,
      std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource);

  static scoped_refptr<WebGPUMailboxTexture> FromExistingMailbox(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      const WGPUTextureDescriptor& desc,
      const gpu::Mailbox& mailbox,
      const gpu::SyncToken& sync_token,
      gpu::webgpu::MailboxFlags mailbox_flags =
          gpu::webgpu::WEBGPU_MAILBOX_NONE,
      base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback =
          {});

  static scoped_refptr<WebGPUMailboxTexture> FromVideoFrame(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      WGPUTextureUsage usage,
      scoped_refptr<media::VideoFrame> video_frame);

  void SetNeedsPresent(bool needs_present) { needs_present_ = needs_present; }
  void SetAlphaClearer(scoped_refptr<WebGPUTextureAlphaClearer> alpha_clearer);
  void Dissociate();

  ~WebGPUMailboxTexture();

  WGPUTexture GetTexture() { return texture_; }
  uint32_t GetTextureIdForTest() { return wire_texture_id_; }
  uint32_t GetTextureGenerationForTest() { return wire_texture_generation_; }
  WGPUDevice GetDeviceForTest() { return device_; }

 private:
  WebGPUMailboxTexture(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUDevice device,
      const WGPUTextureDescriptor& desc,
      const gpu::Mailbox& mailbox,
      const gpu::SyncToken& sync_token,
      gpu::webgpu::MailboxFlags mailbox_flags,
      base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback,
      std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource);

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  WGPUDevice device_;
  base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback_;
  WGPUTexture texture_;
  uint32_t wire_device_id_ = 0;
  uint32_t wire_device_generation_ = 0;
  uint32_t wire_texture_id_ = 0;
  uint32_t wire_texture_generation_ = 0;
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource_;
  bool needs_present_ = false;
  scoped_refptr<WebGPUTextureAlphaClearer> alpha_clearer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_
