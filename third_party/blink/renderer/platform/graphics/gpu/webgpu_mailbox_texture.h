// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_control_client_holder.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "ui/gfx/geometry/rect.h"

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
      const wgpu::Device& device,
      wgpu::TextureUsage usage,
      scoped_refptr<StaticBitmapImage> image,
      const SkImageInfo& info,
      const gfx::Rect& image_sub_rect,
      bool is_dummy_mailbox_texture);

  static scoped_refptr<WebGPUMailboxTexture> FromCanvasResource(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      const wgpu::Device& device,
      wgpu::TextureUsage usage,
      std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource);

  static scoped_refptr<WebGPUMailboxTexture> FromExistingSharedImage(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      const wgpu::Device& device,
      const wgpu::TextureDescriptor& desc,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token,
      gpu::webgpu::MailboxFlags mailbox_flags =
          gpu::webgpu::WEBGPU_MAILBOX_NONE,
      wgpu::TextureUsage additional_internal_usage = wgpu::TextureUsage::None,
      base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback =
          {});

  static scoped_refptr<WebGPUMailboxTexture> FromVideoFrame(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      const wgpu::Device& device,
      wgpu::TextureUsage usage,
      scoped_refptr<media::VideoFrame> video_frame);

  void SetNeedsPresent(bool needs_present);

  void SetAlphaClearer(scoped_refptr<WebGPUTextureAlphaClearer> alpha_clearer);

  // Dissociates this mailbox texture from WebGPU, presenting the image if
  // necessary. Returns a sync token which will satisfy when the mailbox's
  // commands have been fully processed; this return value can safely be ignored
  // if the mailbox texture is not going to be accessed further.
  gpu::SyncToken Dissociate();

  // Sets a SyncToken which gates recycling of the associated recyclable canvas
  // resource. A recyclable canvas resource must be set to use this method.
  void SetCompletionSyncToken(const gpu::SyncToken& token);

  ~WebGPUMailboxTexture();

  const wgpu::Texture& GetTexture();

 private:
  WebGPUMailboxTexture(
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      const wgpu::Device& device,
      const wgpu::TextureDescriptor& desc,
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      const gpu::SyncToken& sync_token,
      gpu::webgpu::MailboxFlags mailbox_flags,
      wgpu::TextureUsage additional_internal_usage,
      base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback,
      std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource);

  base::WeakPtr<WebGPUMailboxTexture> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  wgpu::Device device_;
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  std::unique_ptr<gpu::WebGPUTextureScopedAccess> scoped_access_;
  base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback_;
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource_;
  scoped_refptr<WebGPUTextureAlphaClearer> alpha_clearer_;
  base::WeakPtrFactory<WebGPUMailboxTexture> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_WEBGPU_MAILBOX_TEXTURE_H_
