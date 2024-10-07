// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"

#include "base/numerics/safe_conversions.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "media/base/video_frame.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_texture_alpha_clearer.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

wgpu::TextureFormat VizToWGPUFormat(const viz::SharedImageFormat& format) {
  // This function provides the inverse mapping of `WGPUFormatToViz` (located in
  // webgpu_swap_buffer_provider.cc).
  if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return wgpu::TextureFormat::BGRA8Unorm;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return wgpu::TextureFormat::RGBA8Unorm;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return wgpu::TextureFormat::RGBA16Float;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unexpected canvas format: " << format.ToString();
  return wgpu::TextureFormat::RGBA8Unorm;
}

}  // namespace

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromStaticBitmapImage(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    wgpu::TextureUsage usage,
    scoped_refptr<StaticBitmapImage> image,
    const SkImageInfo& info,
    const gfx::Rect& image_sub_rect,
    bool is_dummy_mailbox_texture) {
  // TODO(crbugs.com/1217160) Mac uses IOSurface in SharedImageBackingGLImage
  // which can be shared to dawn directly aftter passthrough command buffer
  // supported on mac os.
  // We should wrap the StaticBitmapImage directly for mac when passthrough
  // command buffer has been supported.

  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost())
    return nullptr;

  // For noop webgpu mailbox construction, creating mailbox texture with minimum
  // size.
  const int mailbox_texture_width =
      is_dummy_mailbox_texture && image_sub_rect.width() == 0
          ? 1
          : image_sub_rect.width();
  const int mailbox_texture_height =
      is_dummy_mailbox_texture && image_sub_rect.height() == 0
          ? 1
          : image_sub_rect.height();

  // If source image cannot be wrapped into webgpu mailbox texture directly,
  // applied cache with the sub rect size.
  SkImageInfo recyclable_canvas_resource_info =
      info.makeWH(mailbox_texture_width, mailbox_texture_height);
  // Get a recyclable resource for producing WebGPU-compatible shared images.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      dawn_control_client->GetOrCreateCanvasResource(
          recyclable_canvas_resource_info);

  if (!recyclable_canvas_resource) {
    return nullptr;
  }

  CanvasResourceProvider* resource_provider =
      recyclable_canvas_resource->resource_provider();
  DCHECK(resource_provider);

  // Skip copy if constructing dummy mailbox texture.
  if (!is_dummy_mailbox_texture) {
    if (!image->CopyToResourceProvider(resource_provider, image_sub_rect)) {
      return nullptr;
    }
  }

  return WebGPUMailboxTexture::FromCanvasResource(
      dawn_control_client, device, usage,
      std::move(recyclable_canvas_resource));
}

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromCanvasResource(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    wgpu::TextureUsage usage,
    std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource) {
  scoped_refptr<CanvasResource> canvas_resource =
      recyclable_canvas_resource->resource_provider()->ProduceCanvasResource(
          FlushReason::kWebGPUTexture);
  DCHECK(canvas_resource->IsValid());

  const gpu::Mailbox& mailbox =
      canvas_resource->GetClientSharedImage()->mailbox();
  gpu::SyncToken sync_token = canvas_resource->GetSyncToken();
  gfx::Size size = canvas_resource->Size();

  wgpu::TextureDescriptor tex_desc = {
      .usage = usage,
      .size = {base::checked_cast<uint32_t>(size.width()),
               base::checked_cast<uint32_t>(size.height())},
      .format = VizToWGPUFormat(canvas_resource->GetSharedImageFormat()),
  };
  return base::AdoptRef(new WebGPUMailboxTexture(
      std::move(dawn_control_client), device, tex_desc, mailbox, sync_token,
      gpu::webgpu::WEBGPU_MAILBOX_NONE, wgpu::TextureUsage::None,
      base::OnceCallback<void(const gpu::SyncToken&)>(),
      std::move(recyclable_canvas_resource)));
}

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromExistingMailbox(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    const wgpu::TextureDescriptor& desc,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    gpu::webgpu::MailboxFlags mailbox_flags,
    wgpu::TextureUsage additional_internal_usage,
    base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback) {
  DCHECK(dawn_control_client->GetContextProviderWeakPtr());

  return base::AdoptRef(new WebGPUMailboxTexture(
      std::move(dawn_control_client), device, desc, mailbox, sync_token,
      mailbox_flags, additional_internal_usage,
      std::move(finished_access_callback), nullptr));
}

//  static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromVideoFrame(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    wgpu::TextureUsage usage,
    scoped_refptr<media::VideoFrame> video_frame) {
  auto context_provider = dawn_control_client->GetContextProviderWeakPtr();
  if (!context_provider ||
      context_provider->ContextProvider()->IsContextLost()) {
    return nullptr;
  }

  auto finished_access_callback = base::BindOnce(
      [](base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
         media::VideoFrame* frame, const gpu::SyncToken& sync_token) {
        if (context_provider) {
          // Update the sync token before unreferencing the video frame.
          media::WaitAndReplaceSyncTokenClient client(
              context_provider->ContextProvider()->WebGPUInterface());
          frame->UpdateReleaseSyncToken(&client);
        }
      },
      context_provider, base::RetainedRef(video_frame));

  wgpu::TextureDescriptor desc = {
      .usage = wgpu::TextureUsage::TextureBinding,
  };
  return base::AdoptRef(new WebGPUMailboxTexture(
      std::move(dawn_control_client), device, desc,
      video_frame->shared_image()->mailbox(), video_frame->acquire_sync_token(),
      gpu::webgpu::WEBGPU_MAILBOX_NONE, wgpu::TextureUsage::None,
      std::move(finished_access_callback), nullptr));
}

WebGPUMailboxTexture::WebGPUMailboxTexture(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    const wgpu::Device& device,
    const wgpu::TextureDescriptor& desc,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    gpu::webgpu::MailboxFlags mailbox_flags,
    wgpu::TextureUsage additional_internal_usage,
    base::OnceCallback<void(const gpu::SyncToken&)> finished_access_callback,
    std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource)
    : dawn_control_client_(std::move(dawn_control_client)),
      device_(device),
      mailbox_(mailbox),
      finished_access_callback_(std::move(finished_access_callback)),
      recyclable_canvas_resource_(std::move(recyclable_canvas_resource)) {
  DCHECK(dawn_control_client_->GetContextProviderWeakPtr());

  gpu::webgpu::WebGPUInterface* webgpu =
      dawn_control_client_->GetContextProviderWeakPtr()
          ->ContextProvider()
          ->WebGPUInterface();

  // Wait on any work using the image.
  webgpu->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  // Produce and inject image to WebGPU texture
  gpu::webgpu::ReservedTexture reservation = webgpu->ReserveTexture(
      device_.Get(), &static_cast<const WGPUTextureDescriptor&>(desc));
  DCHECK(reservation.texture);

  wire_device_id_ = reservation.deviceId;
  wire_device_generation_ = reservation.deviceGeneration;
  wire_texture_id_ = reservation.id;
  wire_texture_generation_ = reservation.generation;
  texture_ = wgpu::Texture::Acquire(reservation.texture);

  const wgpu::DawnTextureInternalUsageDescriptor* internal_usage_desc = nullptr;
  if (const wgpu::ChainedStruct* next_in_chain = desc.nextInChain) {
    // The internal usage descriptor is the only valid struct to chain.
    CHECK_EQ(next_in_chain->sType,
             wgpu::SType::DawnTextureInternalUsageDescriptor);
    internal_usage_desc =
        static_cast<const wgpu::DawnTextureInternalUsageDescriptor*>(
            next_in_chain);
  }
  auto internal_usage = internal_usage_desc ? internal_usage_desc->internalUsage
                                            : wgpu::TextureUsage::None;
  internal_usage |= additional_internal_usage;

  // This may fail because gl_backing resource cannot produce dawn
  // representation.
  webgpu->AssociateMailbox(
      wire_device_id_, wire_device_generation_, wire_texture_id_,
      wire_texture_generation_, static_cast<uint64_t>(desc.usage),
      static_cast<uint64_t>(internal_usage),
      reinterpret_cast<const WGPUTextureFormat*>(desc.viewFormats),
      base::checked_cast<GLuint>(desc.viewFormatCount), mailbox_flags, mailbox);
}

void WebGPUMailboxTexture::SetAlphaClearer(
    scoped_refptr<WebGPUTextureAlphaClearer> alpha_clearer) {
  alpha_clearer_ = std::move(alpha_clearer);
}

gpu::SyncToken WebGPUMailboxTexture::Dissociate() {
  gpu::SyncToken finished_access_token;
  if (wire_texture_id_ != 0) {
    if (base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider =
            dawn_control_client_->GetContextProviderWeakPtr()) {
      gpu::webgpu::WebGPUInterface* webgpu =
          context_provider->ContextProvider()->WebGPUInterface();
      if (alpha_clearer_) {
        alpha_clearer_->ClearAlpha(texture_);
        alpha_clearer_ = nullptr;
      }
      if (needs_present_) {
        webgpu->DissociateMailboxForPresent(
            wire_device_id_, wire_device_generation_, wire_texture_id_,
            wire_texture_generation_);
      } else {
        webgpu->DissociateMailbox(wire_texture_id_, wire_texture_generation_);
      }
      wire_texture_id_ = 0;

      webgpu->GenUnverifiedSyncTokenCHROMIUM(finished_access_token.GetData());
      if (recyclable_canvas_resource_) {
        recyclable_canvas_resource_->SetCompletionSyncToken(
            finished_access_token);
      }
      if (finished_access_callback_) {
        std::move(finished_access_callback_).Run(finished_access_token);
      }
    }
  }
  return finished_access_token;
}

void WebGPUMailboxTexture::SetCompletionSyncToken(const gpu::SyncToken& token) {
  // This should only be called after Dissociate().
  CHECK_EQ(wire_texture_id_, 0u);

  // This is only allowed if we have an associated recyclable canvas resource.
  CHECK(recyclable_canvas_resource_);
  recyclable_canvas_resource_->SetCompletionSyncToken(token);
}

WebGPUMailboxTexture::~WebGPUMailboxTexture() {
  Dissociate();
}

}  // namespace blink
