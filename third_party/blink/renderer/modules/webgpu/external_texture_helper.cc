// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/external_texture_helper.h"

#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace blink {
namespace {
WGPUExternalTextureRotation FromVideoRotation(media::VideoRotation rotation) {
  switch (rotation) {
    case media::VIDEO_ROTATION_0:
      return WGPUExternalTextureRotation_Rotate0Degrees;
    case media::VIDEO_ROTATION_90:
      return WGPUExternalTextureRotation_Rotate90Degrees;
    case media::VIDEO_ROTATION_180:
      return WGPUExternalTextureRotation_Rotate180Degrees;
    case media::VIDEO_ROTATION_270:
      return WGPUExternalTextureRotation_Rotate270Degrees;
  }
  NOTREACHED();
}
}  // namespace

std::array<float, 12> GetYUVToRGBMatrix(gfx::ColorSpace color_space,
                                        size_t bit_depth) {
  // Get the appropriate YUV to RGB conversion matrix.
  SkYUVColorSpace src_sk_color_space;
  color_space.ToSkYUVColorSpace(static_cast<int>(bit_depth),
                                &src_sk_color_space);
  SkColorMatrix sk_color_matrix = SkColorMatrix::YUVtoRGB(src_sk_color_space);
  float yuv_matrix[20];
  sk_color_matrix.getRowMajor(yuv_matrix);
  // Only use columns 1-3 (3x3 conversion matrix) and column 5 (bias values)
  return std::array<float, 12>{yuv_matrix[0],  yuv_matrix[1],  yuv_matrix[2],
                               yuv_matrix[4],  yuv_matrix[5],  yuv_matrix[6],
                               yuv_matrix[7],  yuv_matrix[9],  yuv_matrix[10],
                               yuv_matrix[11], yuv_matrix[12], yuv_matrix[14]};
}

ColorSpaceConversionConstants GetColorSpaceConversionConstants(
    gfx::ColorSpace src_color_space,
    gfx::ColorSpace dst_color_space) {
  ColorSpaceConversionConstants color_space_conversion_constants;
  // Get primary matrices for the source and destination color spaces.
  // Multiply the source primary matrix with the inverse destination primary
  // matrix to create a single transformation matrix.
  skcms_Matrix3x3 src_primary_matrix_to_XYZD50;
  skcms_Matrix3x3 dst_primary_matrix_to_XYZD50;
  src_color_space.GetPrimaryMatrix(&src_primary_matrix_to_XYZD50);
  dst_color_space.GetPrimaryMatrix(&dst_primary_matrix_to_XYZD50);

  skcms_Matrix3x3 dst_primary_matrix_from_XYZD50;
  skcms_Matrix3x3_invert(&dst_primary_matrix_to_XYZD50,
                         &dst_primary_matrix_from_XYZD50);

  skcms_Matrix3x3 transform_matrix = skcms_Matrix3x3_concat(
      &dst_primary_matrix_from_XYZD50, &src_primary_matrix_to_XYZD50);
  // From row major matrix to col major matrix
  color_space_conversion_constants.gamut_conversion_matrix =
      std::array<float, 9>{
          transform_matrix.vals[0][0], transform_matrix.vals[1][0],
          transform_matrix.vals[2][0], transform_matrix.vals[0][1],
          transform_matrix.vals[1][1], transform_matrix.vals[2][1],
          transform_matrix.vals[0][2], transform_matrix.vals[1][2],
          transform_matrix.vals[2][2]};

  // Set constants for source transfer function.
  skcms_TransferFunction src_transfer_fn;
  src_color_space.GetTransferFunction(&src_transfer_fn);
  color_space_conversion_constants.src_transfer_constants =
      std::array<float, 7>{src_transfer_fn.g, src_transfer_fn.a,
                           src_transfer_fn.b, src_transfer_fn.c,
                           src_transfer_fn.d, src_transfer_fn.e,
                           src_transfer_fn.f};

  // Set constants for destination transfer function.
  skcms_TransferFunction dst_transfer_fn;
  dst_color_space.GetInverseTransferFunction(&dst_transfer_fn);
  color_space_conversion_constants.dst_transfer_constants =
      std::array<float, 7>{dst_transfer_fn.g, dst_transfer_fn.a,
                           dst_transfer_fn.b, dst_transfer_fn.c,
                           dst_transfer_fn.d, dst_transfer_fn.e,
                           dst_transfer_fn.f};

  return color_space_conversion_constants;
}

bool IsSameGamutAndGamma(gfx::ColorSpace src_color_space,
                         gfx::ColorSpace dst_color_space) {
  if (src_color_space.GetPrimaryID() == dst_color_space.GetPrimaryID()) {
    skcms_TransferFunction src;
    skcms_TransferFunction dst;
    if (src_color_space.GetTransferFunction(&src) &&
        dst_color_space.GetTransferFunction(&dst)) {
      return (src.a == dst.a && src.b == dst.b && src.c == dst.c &&
              src.d == dst.d && src.e == dst.e && src.f == dst.f &&
              src.g == dst.g);
    }
  }
  return false;
}

// Copy this helper function from media/renderers/paint_canvas_video_renderer.cc
// to workaround issue crbug.com/1407112. We need to ensure no color space
// conversion happens during all conversions. And leverage Dawn to do the color
// space conversion
// TODO(crbug.com/1407112): Remove this after fixing crbug.com/1407112
gfx::ColorSpace GetVideoFrameRGBColorSpacePreferringSRGB(
    const media::VideoFrame* frame) {
  const auto rgb_color_space = frame->ColorSpace().GetAsFullRangeRGB();
  auto primary_id = rgb_color_space.GetPrimaryID();
  switch (primary_id) {
    case gfx::ColorSpace::PrimaryID::CUSTOM:
      return rgb_color_space;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      break;
    default:
      break;
  }
  auto transfer_id = rgb_color_space.GetTransferID();
  switch (transfer_id) {
    case gfx::ColorSpace::TransferID::CUSTOM:
    case gfx::ColorSpace::TransferID::CUSTOM_HDR:
      return rgb_color_space;
    case gfx::ColorSpace::TransferID::BT709_APPLE:
    case gfx::ColorSpace::TransferID::SMPTE170M:
    case gfx::ColorSpace::TransferID::SMPTE240M:
      transfer_id = gfx::ColorSpace::TransferID::SRGB;
      break;
    default:
      break;
  }
  return gfx::ColorSpace(primary_id, transfer_id);
}

ExternalTextureSource GetExternalTextureSourceFromVideoElement(
    HTMLVideoElement* video,
    ExceptionState& exception_state) {
  ExternalTextureSource source;

  if (!video) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing video source");
    return source;
  }

  if (video->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "Video element is tainted by cross-origin data and may not be "
        "loaded.");
    return source;
  }

  if (auto* wmp = video->GetWebMediaPlayer()) {
    source.media_video_frame = wmp->GetCurrentFrameThenUpdate();
    source.video_renderer = wmp->GetPaintCanvasVideoRenderer();
  }

  if (!source.media_video_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video "
                                      "element that doesn't have back "
                                      "resource.");
    return source;
  }

  source.media_video_frame_unique_id = source.media_video_frame->unique_id();
  source.valid = true;

  return source;
}

ExternalTextureSource GetExternalTextureSourceFromVideoFrame(
    VideoFrame* frame,
    ExceptionState& exception_state) {
  ExternalTextureSource source;

  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing video frame");
    return source;
  }
  // Tainted blink::VideoFrames are not supposed to be possible.
  DCHECK(!frame->WouldTaintOrigin());

  source.media_video_frame = frame->frame();
  if (!source.media_video_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to import texture from video "
                                      "frame that doesn't have back resource");
    return source;
  }

  if (!source.media_video_frame->coded_size().width() ||
      !source.media_video_frame->coded_size().height()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "Cannot import from zero sized video frame");
    return source;
  }

  source.valid = true;

  return source;
}

ExternalTexture CreateExternalTexture(
    GPUDevice* device,
    gfx::ColorSpace src_color_space,
    gfx::ColorSpace dst_color_space,
    scoped_refptr<media::VideoFrame> media_video_frame,
    media::PaintCanvasVideoRenderer* video_renderer) {
  DCHECK(media_video_frame);

  ExternalTexture external_texture = {};

  // TODO(crbug.com/1306753): Use SharedImageProducer and CompositeSharedImage
  // rather than check 'is_webgpu_compatible'.
  bool device_support_zero_copy =
      device->adapter()->SupportsMultiPlanarFormats();

  WGPUExternalTextureDescriptor external_texture_desc = {};

  // Set ExternalTexture visibleSize and visibleOrigin. 0-copy path
  // uses this metadata.
  gfx::Rect visible_rect = media_video_frame->visible_rect();
  DCHECK(visible_rect.x() >= 0 && visible_rect.y() >= 0 &&
         visible_rect.width() >= 0 && visible_rect.height() >= 0);

  external_texture_desc.visibleOrigin = {
      static_cast<uint32_t>(visible_rect.x()),
      static_cast<uint32_t>(visible_rect.y())};
  external_texture_desc.visibleSize = {
      static_cast<uint32_t>(visible_rect.width()),
      static_cast<uint32_t>(visible_rect.height())};

  const bool zero_copy =
      (media_video_frame->HasTextures() &&
       (media_video_frame->format() == media::PIXEL_FORMAT_NV12) &&
       device_support_zero_copy &&
       media_video_frame->metadata().is_webgpu_compatible);
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webgpu"),
                       "CreateExternalTexture", TRACE_EVENT_SCOPE_THREAD,
                       "zero_copy", !!zero_copy, "video_frame",
                       media_video_frame->AsHumanReadableString());
  if (zero_copy) {
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
        WebGPUMailboxTexture::FromVideoFrame(
            device->GetDawnControlClient(), device->GetHandle(),
            WGPUTextureUsage::WGPUTextureUsage_TextureBinding,
            media_video_frame);
    if (!mailbox_texture) {
      return {};
    }

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

    // Set Planes for ExternalTexture
    external_texture_desc.plane0 = plane0;
    external_texture_desc.plane1 = plane1;

    // Set color space transformation metas for ExternalTexture
    std::array<float, 12> yuvToRgbMatrix =
        GetYUVToRGBMatrix(src_color_space, media_video_frame->BitDepth());
    external_texture_desc.yuvToRgbConversionMatrix = yuvToRgbMatrix.data();

    // Decide whether color space conversion could be skipped.
    external_texture_desc.doYuvToRgbConversionOnly =
        IsSameGamutAndGamma(src_color_space, dst_color_space);

    ColorSpaceConversionConstants color_space_conversion_constants =
        GetColorSpaceConversionConstants(src_color_space, dst_color_space);

    external_texture_desc.gamutConversionMatrix =
        color_space_conversion_constants.gamut_conversion_matrix.data();
    external_texture_desc.srcTransferFunctionParameters =
        color_space_conversion_constants.src_transfer_constants.data();
    external_texture_desc.dstTransferFunctionParameters =
        color_space_conversion_constants.dst_transfer_constants.data();

    // Set ExternalTexture rotation and Y-axis flipY
    const media::VideoFrameMetadata& metadata = media_video_frame->metadata();
    if (metadata.transformation) {
      external_texture_desc.rotation =
          FromVideoRotation(metadata.transformation->rotation);
      external_texture_desc.flipY = metadata.transformation->mirrored;
    }

    external_texture.wgpu_external_texture =
        device->GetProcs().deviceCreateExternalTexture(device->GetHandle(),
                                                       &external_texture_desc);

    // The texture view will be referenced during external texture creation, so
    // by calling release here we ensure this texture view will be destructed
    // when the external texture is destructed.
    device->GetProcs().textureViewRelease(plane0);
    device->GetProcs().textureViewRelease(plane1);

    external_texture.mailbox_texture = std::move(mailbox_texture);
    external_texture.is_zero_copy = true;
    return external_texture;
  }
  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost())
    return external_texture;

  // In 0-copy path, uploading shares the whole frame into dawn and apply
  // visible rect and sample from it. For 1-copy path, we should obey the
  // same behaviour by:
  // - Get recycle cache with video frame visible size.
  // - Draw video frame visible rect into recycle cache, uses visible size.
  // - Reset origin of visible rect in ExternalTextureDesc and use internal
  // shader to
  //   handle visible rect.
  const auto intrinsic_size =
      gfx::Size(media_video_frame->visible_rect().width(),
                media_video_frame->visible_rect().height());

  external_texture_desc.visibleOrigin = {};

  // Try to workaround crbug.com/1407112 by keeping no color space conversion
  // DrawVideoFrameIntoResourceProvider by setting the canvas resource's
  // colorspace to the specific ones. However not all color space can be
  // converted to SkColorSpace, in that case default to sRGB.
  // TODO(crbug.com/1407112): set recyclable_canvas_resource_color_space to dest
  // color space after fixing crbug.com/1407112.
  gfx::ColorSpace recyclable_canvas_resource_color_space =
      GetVideoFrameRGBColorSpacePreferringSRGB(media_video_frame.get());
  if (!recyclable_canvas_resource_color_space.ToSkColorSpace()) {
    recyclable_canvas_resource_color_space = gfx::ColorSpace::CreateSRGB();
  }

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      device->GetDawnControlClient()->GetOrCreateCanvasResource(
          SkImageInfo::MakeN32Premul(
              intrinsic_size.width(), intrinsic_size.height(),
              recyclable_canvas_resource_color_space.ToSkColorSpace()),
          /*is_origin_top_left=*/true);
  if (!recyclable_canvas_resource) {
    return external_texture;
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
  const auto dest_rect = gfx::Rect(intrinsic_size);
  if (!DrawVideoFrameIntoResourceProvider(
          std::move(media_video_frame), resource_provider,
          raster_context_provider, dest_rect, video_renderer)) {
    return external_texture;
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

  // Set plane for ExternalTexture
  external_texture_desc.plane0 = plane0;

  // Decide whether color space conversion could be skipped.
  // Try to workaround crbug.com/1407112 by using Dawn to do color space
  // conversion.
  // TODO(crbug.com/1407112): compare recyclable_canvas_resource_color_space
  // instead of src_color_space after fixing crbug.com/1407112.
  external_texture_desc.doYuvToRgbConversionOnly =
      IsSameGamutAndGamma(src_color_space, dst_color_space);

  // Set color space transformation metas for ExternalTexture
  ColorSpaceConversionConstants color_space_conversion_constants =
      GetColorSpaceConversionConstants(src_color_space, dst_color_space);

  external_texture_desc.gamutConversionMatrix =
      color_space_conversion_constants.gamut_conversion_matrix.data();
  external_texture_desc.srcTransferFunctionParameters =
      color_space_conversion_constants.src_transfer_constants.data();
  external_texture_desc.dstTransferFunctionParameters =
      color_space_conversion_constants.dst_transfer_constants.data();

  external_texture.wgpu_external_texture =
      device->GetProcs().deviceCreateExternalTexture(device->GetHandle(),
                                                     &external_texture_desc);

  // The texture view will be referenced during external texture creation, so by
  // calling release here we ensure this texture view will be destructed when
  // the external texture is destructed.
  device->GetProcs().textureViewRelease(plane0);
  external_texture.mailbox_texture = std::move(mailbox_texture);

  return external_texture;
}

}  // namespace blink
