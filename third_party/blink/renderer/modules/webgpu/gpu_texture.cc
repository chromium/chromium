// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_usage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"

namespace blink {

namespace {

WGPUTextureDescriptor AsDawnType(const GPUTextureDescriptor* webgpu_desc,
                                 std::string* label,
                                 GPUDevice* device) {
  DCHECK(webgpu_desc);
  DCHECK(label);
  DCHECK(device);

  WGPUTextureDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = static_cast<WGPUTextureUsage>(webgpu_desc->usage());
  dawn_desc.dimension =
      AsDawnEnum<WGPUTextureDimension>(webgpu_desc->dimension());
  dawn_desc.size = AsDawnType(&webgpu_desc->size(), device);
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.sampleCount = webgpu_desc->sampleCount();

  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

WGPUTextureViewDescriptor AsDawnType(
    const GPUTextureViewDescriptor* webgpu_desc,
    std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  WGPUTextureViewDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  dawn_desc.dimension =
      AsDawnEnum<WGPUTextureViewDimension>(webgpu_desc->dimension());
  dawn_desc.baseMipLevel = webgpu_desc->baseMipLevel();
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.baseArrayLayer = webgpu_desc->baseArrayLayer();
  dawn_desc.arrayLayerCount = webgpu_desc->arrayLayerCount();
  dawn_desc.aspect = AsDawnEnum<WGPUTextureAspect>(webgpu_desc->aspect());
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

WGPUTextureFormat SkColorTypeToWGPUTextureFormat(SkColorType color_type) {
  switch (color_type) {
    case SkColorType::kRGBA_8888_SkColorType:
      return WGPUTextureFormat_RGBA8Unorm;
    case SkColorType::kBGRA_8888_SkColorType:
      return WGPUTextureFormat_BGRA8Unorm;
    case SkColorType::kRGBA_1010102_SkColorType:
      return WGPUTextureFormat_RGB10A2Unorm;
    case SkColorType::kRGBA_F16_SkColorType:
      return WGPUTextureFormat_RGBA16Float;
    case SkColorType::kRGBA_F32_SkColorType:
      return WGPUTextureFormat_RGBA32Float;
    case SkColorType::kR8G8_unorm_SkColorType:
      return WGPUTextureFormat_RG8Unorm;
    case SkColorType::kR16G16_float_SkColorType:
      return WGPUTextureFormat_RG16Float;
    default:
      return WGPUTextureFormat_Undefined;
  }
}

}  // anonymous namespace

// static
GPUTexture* GPUTexture::Create(GPUDevice* device,
                               const GPUTextureDescriptor* webgpu_desc,
                               ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  WGPUTextureDescriptor dawn_desc = AsDawnType(webgpu_desc, &label, device);

  GPUTexture* texture = MakeGarbageCollected<GPUTexture>(
      device,
      device->GetProcs().deviceCreateTexture(device->GetHandle(), &dawn_desc),
      dawn_desc.format);
  texture->setLabel(webgpu_desc->label());
  return texture;
}

// static
GPUTexture* GPUTexture::FromVideo(GPUDevice* device,
                                  HTMLVideoElement* video,
                                  WGPUTextureUsage usage,
                                  ExceptionState& exception_state) {
  if (!video || !video->videoWidth() || !video->videoHeight()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing video source");
    return nullptr;
  }

  if (video->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "Video element is tainted by cross-origin data and may not be loaded.");
    return nullptr;
  }

  media::PaintCanvasVideoRenderer* video_renderer = nullptr;
  scoped_refptr<media::VideoFrame> media_video_frame;
  if (auto* wmp = video->GetWebMediaPlayer()) {
    media_video_frame = wmp->GetCurrentFrame();
    video_renderer = wmp->GetPaintCanvasVideoRenderer();
  }

  if (!media_video_frame || !video_renderer) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost())
    return nullptr;

  const CanvasResourceParams params(CanvasColorSpace::kSRGB, kN32_SkColorType,
                                    kPremul_SkAlphaType);
  const auto intrinsic_size = IntSize(media_video_frame->natural_size());

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      device->GetDawnControlClient()->GetOrCreateCanvasResource(
          intrinsic_size, params, /*is_origin_top_left=*/true);
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
  const auto dest_rect = gfx::Rect(media_video_frame->natural_size());
  if (!DrawVideoFrameIntoResourceProvider(
          std::move(media_video_frame), resource_provider,
          raster_context_provider, dest_rect, video_renderer)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  // Extract the format. This is only used to validate experimentalImportTexture
  // right now. We may want to reflect it from this function or validate it
  // against some input parameters.
  WGPUTextureFormat format = SkColorTypeToWGPUTextureFormat(
      resource_provider->ColorParams().GetSkColorType());
  if (format == WGPUTextureFormat_Undefined) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Failed to import texture from video. Unsupported format.");
    return nullptr;
  }

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromCanvasResource(
          device->GetDawnControlClient(), device->GetHandle(), usage,
          std::move(recyclable_canvas_resource));

  DCHECK(mailbox_texture->GetTexture() != nullptr);

  return MakeGarbageCollected<GPUTexture>(device, format,
                                          std::move(mailbox_texture));
}

// static
GPUTexture* GPUTexture::FromCanvas(GPUDevice* device,
                                   HTMLCanvasElement* canvas,
                                   WGPUTextureUsage usage,
                                   ExceptionState& exception_state) {
  if (!canvas || !canvas->width() || !canvas->height()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing canvas source");
    return nullptr;
  }

  if (!canvas->OriginClean()) {
    exception_state.ThrowSecurityError(
        "Canvas element is tainted by cross-origin data and may not be "
        "loaded.");
    return nullptr;
  }

  // TODO: Webgpu contexts also return true for Is3d(), but most of the webgl
  // specific CanvasRenderingContext methods don't work for webgpu.
  auto* canvas_context = canvas->RenderingContext();
  if (!canvas_context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing canvas rendering context");
    return nullptr;
  }

  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Shared GPU context lost");
    return nullptr;
  }

  const CanvasResourceParams params(CanvasColorSpace::kSRGB, kN32_SkColorType,
                                    kPremul_SkAlphaType);

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  // First texel i.e. UV (0, 0) should be mapped to top left of the source.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      device->GetDawnControlClient()->GetOrCreateCanvasResource(
          canvas->Size(), params, /*is_origin_top_left=*/true);
  if (!recyclable_canvas_resource) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to create resource provider");
    return nullptr;
  }

  CanvasResourceProvider* resource_provider =
      recyclable_canvas_resource->resource_provider();
  DCHECK(resource_provider);

  // Extract the format. This is only used to validate experimentalImportTexture
  // right now. We may want to reflect it from this function or validate it
  // against some input parameters.
  WGPUTextureFormat format = SkColorTypeToWGPUTextureFormat(
      resource_provider->ColorParams().GetSkColorType());
  if (format == WGPUTextureFormat_Undefined) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Unsupported format for import texture");
    return nullptr;
  }

  if (!canvas_context->CopyRenderingResultsFromDrawingBuffer(resource_provider,
                                                             kBackBuffer)) {
    // Fallback to static bitmap image.
    SourceImageStatus source_image_status = kInvalidSourceImageStatus;
    auto image = canvas->GetSourceImageForCanvas(&source_image_status,
                                                 FloatSize(canvas->Size()));
    if (source_image_status != kNormalSourceImageStatus) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Failed to get image from canvas");
      return nullptr;
    }
    auto* static_bitmap_image = DynamicTo<StaticBitmapImage>(image.get());
    if (!static_bitmap_image ||
        !static_bitmap_image->CopyToResourceProvider(resource_provider)) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Failed to import texture from canvas");
      return nullptr;
    }
  }

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromCanvasResource(
          device->GetDawnControlClient(), device->GetHandle(), usage,
          std::move(recyclable_canvas_resource));
  DCHECK(mailbox_texture->GetTexture());

  return MakeGarbageCollected<GPUTexture>(device, format,
                                          std::move(mailbox_texture));
}

GPUTexture::GPUTexture(GPUDevice* device,
                       WGPUTexture texture,
                       WGPUTextureFormat format)
    : DawnObject<WGPUTexture>(device, texture), format_(format) {}

GPUTexture::GPUTexture(GPUDevice* device,
                       WGPUTextureFormat format,
                       scoped_refptr<WebGPUMailboxTexture> mailbox_texture)
    : DawnObject<WGPUTexture>(device, mailbox_texture->GetTexture()),
      format_(format),
      mailbox_texture_(std::move(mailbox_texture)) {}

GPUTextureView* GPUTexture::createView(
    const GPUTextureViewDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  std::string label;
  WGPUTextureViewDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);
  GPUTextureView* view = MakeGarbageCollected<GPUTextureView>(
      device_, GetProcs().textureCreateView(GetHandle(), &dawn_desc));
  view->setLabel(webgpu_desc->label());
  return view;
}

void GPUTexture::destroy() {
  GetProcs().textureDestroy(GetHandle());
  mailbox_texture_.reset();
}

}  // namespace blink
