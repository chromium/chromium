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
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"

namespace blink {

namespace {
void GetYUVToRGBMatrix(gfx::ColorSpace colorSpace, float matrix[12]) {
  // Get the appropriate YUV to RGB conversion matrix.
  SkYUVColorSpace srcSkColorSpace;
  colorSpace.ToSkYUVColorSpace(8, &srcSkColorSpace);
  SkColorMatrix skColorMatrix = SkColorMatrix::YUVtoRGB(srcSkColorSpace);
  float yuvM[20];
  skColorMatrix.getRowMajor(yuvM);
  // Only use columns 1-3 (3x3 conversion matrix) and column 5 (bias values)
  matrix[0] = yuvM[0];
  matrix[1] = yuvM[1];
  matrix[2] = yuvM[2];
  matrix[3] = yuvM[4];
  matrix[4] = yuvM[5];
  matrix[5] = yuvM[6];
  matrix[6] = yuvM[7];
  matrix[7] = yuvM[9];
  matrix[8] = yuvM[10];
  matrix[9] = yuvM[11];
  matrix[10] = yuvM[12];
  matrix[11] = yuvM[14];
}

void GetColorSpaceConversionConstants(gfx::ColorSpace srcColorSpace,
                                      gfx::ColorSpace dstColorSpace,
                                      float gamutMatrix[9],
                                      float srcTransferConstants[7],
                                      float dstTransferConstants[7]) {
  // Get primary matrices for the source and destination color spaces.
  // Multiply the source primary matrix with the inverse destination primary
  // matrix to create a single transformation matrix.
  skcms_Matrix3x3 srcPrimaryMatrixToXYZD50;
  skcms_Matrix3x3 dstPrimaryMatrixToXYZD50;
  srcColorSpace.GetPrimaryMatrix(&srcPrimaryMatrixToXYZD50);
  dstColorSpace.GetPrimaryMatrix(&dstPrimaryMatrixToXYZD50);

  skcms_Matrix3x3 dstPrimaryMatrixFromXYZD50;
  skcms_Matrix3x3_invert(&dstPrimaryMatrixToXYZD50,
                         &dstPrimaryMatrixFromXYZD50);

  skcms_Matrix3x3 transformM = skcms_Matrix3x3_concat(
      &srcPrimaryMatrixToXYZD50, &dstPrimaryMatrixFromXYZD50);
  // From row major matrix to col major matrix
  gamutMatrix[0] = transformM.vals[0][0];
  gamutMatrix[1] = transformM.vals[1][0];
  gamutMatrix[2] = transformM.vals[2][0];
  gamutMatrix[3] = transformM.vals[0][1];
  gamutMatrix[4] = transformM.vals[1][1];
  gamutMatrix[5] = transformM.vals[2][1];
  gamutMatrix[6] = transformM.vals[0][2];
  gamutMatrix[7] = transformM.vals[1][2];
  gamutMatrix[8] = transformM.vals[2][2];

  // Set constants for source transfer function.
  skcms_TransferFunction src_transfer_fn;
  srcColorSpace.GetInverseTransferFunction(&src_transfer_fn);
  srcTransferConstants[0] = src_transfer_fn.g;
  srcTransferConstants[1] = src_transfer_fn.a;
  srcTransferConstants[2] = src_transfer_fn.b;
  srcTransferConstants[3] = src_transfer_fn.c;
  srcTransferConstants[4] = src_transfer_fn.d;
  srcTransferConstants[5] = src_transfer_fn.e;
  srcTransferConstants[6] = src_transfer_fn.f;

  // Set constants for destination transfer function.
  skcms_TransferFunction dst_transfer_fn;
  dstColorSpace.GetTransferFunction(&dst_transfer_fn);
  dstTransferConstants[0] = dst_transfer_fn.g;
  dstTransferConstants[1] = dst_transfer_fn.a;
  dstTransferConstants[2] = dst_transfer_fn.b;
  dstTransferConstants[3] = dst_transfer_fn.c;
  dstTransferConstants[4] = dst_transfer_fn.d;
  dstTransferConstants[5] = dst_transfer_fn.e;
  dstTransferConstants[6] = dst_transfer_fn.f;
}
}  // namespace

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

  gfx::ColorSpace dstColorSpace;
  switch (webgpu_desc->colorSpace().AsEnum()) {
    case V8GPUPredefinedColorSpace::Enum::kSRGB:
      dstColorSpace = gfx::ColorSpace::CreateSRGB();
  }

  // TODO(crbug.com/1306753): Use SharedImageProducer and CompositeSharedImage
  // rather than check 'is_webgpu_compatible'.
  bool device_support_zero_copy =
      device->adapter()->features()->FeatureNameSet().Contains(
          "multi-planar-formats");

  if (media_video_frame->HasTextures() &&
      (media_video_frame->format() == media::PIXEL_FORMAT_NV12) &&
      device_support_zero_copy &&
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
    external_texture_desc.colorSpace = AsDawnEnum(webgpu_desc->colorSpace());

    gfx::ColorSpace srcColorSpace = media_video_frame->ColorSpace();

    float yuvToRgbMatrix[12];
    GetYUVToRGBMatrix(srcColorSpace, yuvToRgbMatrix);
    external_texture_desc.yuvToRgbConversionMatrix = yuvToRgbMatrix;

    float gamutConversionMatrix[9];
    float srcTransferFn[7];
    float dstTransferFn[7];

    GetColorSpaceConversionConstants(srcColorSpace, dstColorSpace,
                                     gamutConversionMatrix, srcTransferFn,
                                     dstTransferFn);

    external_texture_desc.gamutConversionMatrix = gamutConversionMatrix;
    external_texture_desc.srcTransferFunctionParameters = srcTransferFn;
    external_texture_desc.dstTransferFunctionParameters = dstTransferFn;

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
  dawn_desc.colorSpace = AsDawnEnum(webgpu_desc->colorSpace());

  // The method that performs YUV to RGB conversion
  // (DrawVideoFrameIntoResourceProvider) on the video frame is hardcoded to use
  // BT.601, so we must specify that as our source color space here.
  gfx::ColorSpace srcColorSpace = gfx::ColorSpace::CreateREC601();
  float gamutMatrix[9];
  float srcTransferFn[7];
  float dstTransferFn[7];

  GetColorSpaceConversionConstants(srcColorSpace, dstColorSpace, gamutMatrix,
                                   srcTransferFn, dstTransferFn);

  dawn_desc.gamutConversionMatrix = gamutMatrix;
  dawn_desc.srcTransferFunctionParameters = srcTransferFn;
  dawn_desc.dstTransferFunctionParameters = dstTransferFn;

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
