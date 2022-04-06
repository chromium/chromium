// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"

#include "media/base/video_frame.h"
#include "media/base/wait_and_replace_sync_token_client.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_external_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlvideoelement_videoframe.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

namespace blink {

// static
GPUExternalTexture* GPUExternalTexture::Create(
    GPUDevice* device,
    const GPUExternalTextureDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  scoped_refptr<media::VideoFrame> media_video_frame;
  media::PaintCanvasVideoRenderer* video_renderer = nullptr;
  switch (webgpu_desc->source()->GetContentType()) {
    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kHTMLVideoElement: {
      HTMLVideoElement* video = webgpu_desc->source()->GetAsHTMLVideoElement();

      if (!video || !video->videoWidth() || !video->videoHeight()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                          "Missing video source");
        return nullptr;
      }

      if (video->WouldTaintOrigin()) {
        exception_state.ThrowSecurityError(
            "Video element is tainted by cross-origin data and may not be "
            "loaded.");
        return nullptr;
      }

      if (auto* wmp = video->GetWebMediaPlayer()) {
        media_video_frame = wmp->GetCurrentFrame();
        video_renderer = wmp->GetPaintCanvasVideoRenderer();
      }
      break;
    }

    case V8UnionHTMLVideoElementOrVideoFrame::ContentType::kVideoFrame: {
      VideoFrame* frame = webgpu_desc->source()->GetAsVideoFrame();

      if (!frame || !frame->codedWidth() || !frame->codedHeight()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                          "Missing video source");
        return nullptr;
      }

      if (frame->WouldTaintOrigin()) {
        exception_state.ThrowSecurityError(
            "VideoFrame is tainted by cross-origin data and may not be "
            "loaded.");
        return nullptr;
      }

      media_video_frame = frame->frame();
      break;
    }
  }

  if (!media_video_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    if (!media_video_frame) {
      device->AddConsoleWarning(
          "Cannot get valid video frame, maybe the"
          "HTMLVideoElement is not loaded");
    }

    return nullptr;
  }

  // TODO(crbug.com/1306753): Use SharedImageProducer and CompositeSharedImage
  // rather than check 'is_webgpu_compatible'.
  if (media_video_frame->HasTextures() &&
      (media_video_frame->format() == media::PIXEL_FORMAT_NV12) &&
      media_video_frame->metadata().is_webgpu_compatible) {
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
        WebGPUMailboxTexture::FromVideoFrame(
            device->GetDawnControlClient(), device->GetHandle(),
            WGPUTextureUsage::WGPUTextureUsage_TextureBinding,
            media_video_frame);

    WGPUTextureViewDescriptor view_desc = {
        .format = WGPUTextureFormat_R8Unorm,
        .mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED,
        .arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED,
        .aspect = WGPUTextureAspect_Plane0Only};
    WGPUTextureView plane0 = device->GetProcs().textureCreateView(
        mailbox_texture->GetTexture(), &view_desc);
    view_desc.format = WGPUTextureFormat_RG8Unorm;
    view_desc.aspect = WGPUTextureAspect_Plane1Only;
    WGPUTextureView plane1 = device->GetProcs().textureCreateView(
        mailbox_texture->GetTexture(), &view_desc);

    WGPUExternalTextureDescriptor external_texture_desc = {};
    external_texture_desc.plane0 = plane0;
    external_texture_desc.plane1 = plane1;
    external_texture_desc.colorSpace = WGPUPredefinedColorSpace_Srgb;

    GPUExternalTexture* external_texture =
        MakeGarbageCollected<GPUExternalTexture>(
            device,
            device->GetProcs().deviceCreateExternalTexture(
                device->GetHandle(), &external_texture_desc),
            std::move(mailbox_texture));

    // The texture view will be referenced during external texture creation, so
    // by calling release here we ensure this texture view will be destructed
    // when the external texture is destructed.
    device->GetProcs().textureViewRelease(plane0);
    device->GetProcs().textureViewRelease(plane1);

    return external_texture;
  }
  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost())
    return nullptr;

  const auto intrinsic_size = media_video_frame->natural_size();

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      device->GetDawnControlClient()->GetOrCreateCanvasResource(
          SkImageInfo::MakeN32Premul(intrinsic_size.width(),
                                     intrinsic_size.height()),
          /*is_origin_top_left=*/true);
  if (!recyclable_canvas_resource) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  CanvasResourceProvider* resource_provider =
      recyclable_canvas_resource->resource_provider();
  DCHECK(resource_provider);

  viz::RasterContextProvider* raster_context_provider = nullptr;
  if (auto* context_provider = context_provider_wrapper->ContextProvider())
    raster_context_provider = context_provider->RasterContextProvider();

  // TODO(crbug.com/1174809): This isn't efficient for VideoFrames which are
  // already available as a shared image. A WebGPUMailboxTexture should be
  // created directly from the VideoFrame instead.
  // TODO(crbug.com/1174809): VideoFrame cannot extract video_renderer.
  // DrawVideoFrameIntoResourceProvider() creates local_video_renderer always.
  // This might affect performance, maybe a cache local_video_renderer could
  // help.
  const auto dest_rect = gfx::Rect(media_video_frame->natural_size());
  if (!DrawVideoFrameIntoResourceProvider(
          std::move(media_video_frame), resource_provider,
          raster_context_provider, dest_rect, video_renderer)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromCanvasResource(
          device->GetDawnControlClient(), device->GetHandle(),
          WGPUTextureUsage::WGPUTextureUsage_TextureBinding,
          std::move(recyclable_canvas_resource));

  WGPUTextureViewDescriptor view_desc = {};
  view_desc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
  view_desc.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
  WGPUTextureView plane0 = device->GetProcs().textureCreateView(
      mailbox_texture->GetTexture(), &view_desc);

  WGPUExternalTextureDescriptor dawn_desc = {};
  dawn_desc.plane0 = plane0;

  GPUExternalTexture* external_texture =
      MakeGarbageCollected<GPUExternalTexture>(
          device,
          device->GetProcs().deviceCreateExternalTexture(device->GetHandle(),
                                                         &dawn_desc),
          mailbox_texture);

  // The texture view will be referenced during external texture creation, so by
  // calling release here we ensure this texture view will be destructed when
  // the external texture is destructed.
  device->GetProcs().textureViewRelease(plane0);

  return external_texture;
}

GPUExternalTexture::GPUExternalTexture(
    GPUDevice* device,
    WGPUExternalTexture external_texture,
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture)
    : DawnObject<WGPUExternalTexture>(device, external_texture),
      mailbox_texture_(mailbox_texture) {}

void GPUExternalTexture::Destroy() {
  WGPUTexture texture = mailbox_texture_->GetTexture();
  GetProcs().textureReference(texture);
  mailbox_texture_.reset();
  GetProcs().textureDestroy(texture);
  GetProcs().textureRelease(texture);
}

}  // namespace blink
