// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_usage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"

namespace blink {

bool GPUTextureUsage::usedDeprecatedOutputAttachment = false;

namespace {

WGPUTextureDescriptor AsDawnType(const GPUTextureDescriptor* webgpu_desc,
                                 std::string* label,
                                 GPUDevice* device) {
  DCHECK(webgpu_desc);
  DCHECK(label);
  DCHECK(device);

  if (webgpu_desc->usage() & GPUTextureUsage::kRenderAttachment &&
      GPUTextureUsage::usedDeprecatedOutputAttachment) {
    GPUTextureUsage::usedDeprecatedOutputAttachment = false;
    device->AddConsoleWarning(
        "GPUTextureUsage.OUTPUT_ATTACHMENT has been "
        "renamed to GPUTextureUsage.RENDER_ATTACHMENT.");
  }

  WGPUTextureDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = static_cast<WGPUTextureUsage>(webgpu_desc->usage());
  dawn_desc.dimension =
      AsDawnEnum<WGPUTextureDimension>(webgpu_desc->dimension());
  dawn_desc.size = AsDawnType(&webgpu_desc->size());
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

}  // anonymous namespace

// static
GPUTexture* GPUTexture::Create(GPUDevice* device,
                               const GPUTextureDescriptor* webgpu_desc,
                               ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  // Check size is correctly formatted before further processing.
  const UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& size =
      webgpu_desc->size();
  if (size.IsUnsignedLongEnforceRangeSequence() &&
      size.GetAsUnsignedLongEnforceRangeSequence().size() != 3) {
    exception_state.ThrowRangeError("size length must be 3");
    return nullptr;
  }

  std::string label;
  WGPUTextureDescriptor dawn_desc = AsDawnType(webgpu_desc, &label, device);

  return MakeGarbageCollected<GPUTexture>(
      device,
      device->GetProcs().deviceCreateTexture(device->GetHandle(), &dawn_desc),
      dawn_desc.format);
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
        "Video element contains cross-origin data and may not be loaded.");
    return nullptr;
  }

  // Create a CanvasResourceProvider for producing WebGPU-compatible shared
  // images.
  // TODO(crbug.com/1174809): This should recycle resources instead of creating
  // a new shared image every time.
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateWebGPUImageProvider(
          IntSize(video->videoWidth(), video->videoHeight()),
          kLow_SkFilterQuality,
          CanvasResourceParams(CanvasColorSpace::kSRGB, kN32_SkColorType,
                               kPremul_SkAlphaType),
          CanvasResourceProvider::ShouldInitialize::kNo,
          SharedGpuContext::ContextProviderWrapper());

  if (!resource_provider) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video");
    return nullptr;
  }

  // Copy the video frame into the shared image.
  video->PaintCurrentFrame(
      resource_provider->Canvas(),
      IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())),
      nullptr);

  // Acquire the CanvasResource wrapping the shared image.
  scoped_refptr<CanvasResource> canvas_resource =
      resource_provider->ProduceCanvasResource();
  DCHECK(canvas_resource->IsValid());
  DCHECK(canvas_resource->IsAccelerated());

  // Extract the format. This is only used to validate copyImageBitmapToTexture
  // right now. We may want to reflect it from this function or validate it
  // against some input parameters.
  WGPUTextureFormat format;
  switch (canvas_resource->CreateSkImageInfo().colorType()) {
    case SkColorType::kRGBA_8888_SkColorType:
      format = WGPUTextureFormat_RGBA8Unorm;
      break;
    case SkColorType::kBGRA_8888_SkColorType:
      format = WGPUTextureFormat_BGRA8Unorm;
      break;
    case SkColorType::kRGBA_1010102_SkColorType:
      format = WGPUTextureFormat_RGB10A2Unorm;
      break;
    case SkColorType::kRGBA_F16_SkColorType:
      format = WGPUTextureFormat_RGBA16Float;
      break;
    case SkColorType::kRGBA_F32_SkColorType:
      format = WGPUTextureFormat_RGBA32Float;
      break;
    case SkColorType::kR8G8_unorm_SkColorType:
      format = WGPUTextureFormat_RG8Unorm;
      break;
    case SkColorType::kR16G16_float_SkColorType:
      format = WGPUTextureFormat_RG16Float;
      break;
    default:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kOperationError,
          "Failed to import texture from video. Unsupported format.");
      return nullptr;
  }

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromCanvasResource(device->GetDawnControlClient(),
                                               device->GetHandle(), usage,
                                               std::move(canvas_resource));
  DCHECK(mailbox_texture->GetTexture() != nullptr);

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
  return MakeGarbageCollected<GPUTextureView>(
      device_, GetProcs().textureCreateView(GetHandle(), &dawn_desc));
}

void GPUTexture::destroy() {
  GetProcs().textureDestroy(GetHandle());
  mailbox_texture_.reset();
}

}  // namespace blink
