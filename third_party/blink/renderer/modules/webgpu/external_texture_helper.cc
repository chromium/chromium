// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webgpu/external_texture_helper.h"

#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "media/renderers/paint_canvas_video_renderer.h"
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
wgpu::ExternalTextureRotation FromVideoRotation(media::VideoRotation rotation) {
  switch (rotation) {
    case media::VIDEO_ROTATION_0:
      return wgpu::ExternalTextureRotation::Rotate0Degrees;
    case media::VIDEO_ROTATION_90:
      return wgpu::ExternalTextureRotation::Rotate90Degrees;
    case media::VIDEO_ROTATION_180:
      return wgpu::ExternalTextureRotation::Rotate180Degrees;
    case media::VIDEO_ROTATION_270:
      return wgpu::ExternalTextureRotation::Rotate270Degrees;
  }
  NOTREACHED_IN_MIGRATION();
}

// TODO(crbug.com/40227105): Support HDR color space and color range in
// generated wgsl shader to enable all color space for zero-copy path.
bool DstColorSpaceSupportedByZeroCopy(
    PredefinedColorSpace dst_predefined_color_space) {
  switch (dst_predefined_color_space) {
    case PredefinedColorSpace::kSRGB:
    case PredefinedColorSpace::kP3:
      return true;
    default:
      break;
  }
  return false;
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
    PredefinedColorSpace dst_predefined_color_space,
    scoped_refptr<media::VideoFrame> media_video_frame,
    media::PaintCanvasVideoRenderer* video_renderer) {
  DCHECK(media_video_frame);
  gfx::ColorSpace src_color_space = media_video_frame->ColorSpace();
  gfx::ColorSpace dst_color_space =
      PredefinedColorSpaceToGfxColorSpace(dst_predefined_color_space);

  // It should be very rare that a frame didn't get a valid colorspace through
  // the guessing process:
  // https://source.chromium.org/chromium/chromium/src/+/main:media/base/video_color_space.cc;l=69;drc=6c9cfff09be8397270b376a4e4407328694e97fa
  // The historical rule for this was to use BT.601 for SD content and BT.709
  // for HD content:
  // https://source.chromium.org/chromium/chromium/src/+/main:media/ffmpeg/ffmpeg_common.cc;l=683;drc=1946212ac0100668f14eb9e2843bdd846e510a1e)
  // We prefer always using BT.709 since SD content in practice is down-scaled
  // HD content, not NTSC broadcast content.
  if (!src_color_space.IsValid()) {
    src_color_space = gfx::ColorSpace::CreateREC709();
  }

  ExternalTexture external_texture = {};

  // TODO(crbug.com/1306753): Use SharedImageProducer and CompositeSharedImage
  // rather than check 'is_webgpu_compatible'.
  bool device_support_zero_copy =
      device->adapter()->SupportsMultiPlanarFormats();

  wgpu::ExternalTextureDescriptor external_texture_desc = {};

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

  // Set ExternalTexture rotation and mirrored state.
  const media::VideoFrameMetadata& metadata = media_video_frame->metadata();
  if (metadata.transformation) {
    external_texture_desc.rotation =
        FromVideoRotation(metadata.transformation->rotation);
    external_texture_desc.mirrored = metadata.transformation->mirrored;
  }

  const bool zero_copy =
      (media_video_frame->HasSharedImage() &&
       (media_video_frame->format() == media::PIXEL_FORMAT_NV12) &&
       device_support_zero_copy &&
       media_video_frame->metadata().is_webgpu_compatible &&
       DstColorSpaceSupportedByZeroCopy(dst_predefined_color_space));

  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webgpu"),
                       "CreateExternalTexture", TRACE_EVENT_SCOPE_THREAD,
                       "zero_copy", !!zero_copy, "video_frame",
                       media_video_frame->AsHumanReadableString());
  if (zero_copy) {
    scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
        WebGPUMailboxTexture::FromVideoFrame(
            device->GetDawnControlClient(), device->GetHandle(),
            wgpu::TextureUsage::TextureBinding, media_video_frame);
    if (!mailbox_texture) {
      return {};
    }

    wgpu::TextureViewDescriptor view_desc = {
        .format = wgpu::TextureFormat::R8Unorm,
        .aspect = wgpu::TextureAspect::Plane0Only};
    wgpu::TextureView plane0 =
        mailbox_texture->GetTexture().CreateView(&view_desc);
    view_desc.format = wgpu::TextureFormat::RG8Unorm;
    view_desc.aspect = wgpu::TextureAspect::Plane1Only;
    wgpu::TextureView plane1 =
        mailbox_texture->GetTexture().CreateView(&view_desc);

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

    external_texture.wgpu_external_texture =
        device->GetHandle().CreateExternalTexture(&external_texture_desc);

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
  external_texture_desc.visibleOrigin = {};

  std::unique_ptr<media::PaintCanvasVideoRenderer> local_video_renderer;
  if (!video_renderer) {
    local_video_renderer = std::make_unique<media::PaintCanvasVideoRenderer>();
    video_renderer = local_video_renderer.get();
  }

  // Using CopyVideoFrameToSharedImage() is an optional one copy upload path.
  // However, the formats this path supports are quite limited. Check whether
  // the current video frame could be uploaded through this one copy upload
  // path. If not, fallback to DrawVideoFrameIntoResourceProvider().
  // TODO(crbug.com/327270287): Expand CopyVideoFrameToSharedImage() to
  // support all valid video frame formats and remove the draw path.
  bool use_copy_to_shared_image =
      video_renderer->CanUseCopyVideoFrameToSharedImage(*media_video_frame);

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  // The recyclable resource's color space is the same as source color space
  // with the YUV to RGB transform stripped out since that's handled by the
  // PaintCanvasVideoRenderer.
  gfx::ColorSpace resource_color_space = src_color_space.GetAsRGB();

  // Using DrawVideoFrameIntoResourceProvider() for uploading. Need to
  // workaround issue crbug.com/1407112. It requires no color space
  // conversion when drawing video frame to resource provider.
  // Leverage Dawn to do the color space conversion.
  // TODO(crbug.com/1407112): Don't use compatRgbColorSpace but the
  // exact color space after fixing this issue.
  if (!use_copy_to_shared_image) {
    resource_color_space = media_video_frame->CompatRGBColorSpace();
  }

  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      device->GetDawnControlClient()->GetOrCreateCanvasResource(
          SkImageInfo::MakeN32Premul(visible_rect.width(),
                                     visible_rect.height(),
                                     resource_color_space.ToSkColorSpace()));
  if (!recyclable_canvas_resource) {
    return external_texture;
  }

  CanvasResourceProvider* resource_provider =
      recyclable_canvas_resource->resource_provider();
  DCHECK(resource_provider);

  viz::RasterContextProvider* raster_context_provider = nullptr;
  if (auto* context_provider = context_provider_wrapper->ContextProvider())
    raster_context_provider = context_provider->RasterContextProvider();

  if (use_copy_to_shared_image) {
    // We don't need to specify a sync token since both CanvasResourceProvider
    // and PaintCanvasVideoRenderer use the SharedGpuContext.
    auto client_si =
        resource_provider->GetBackingClientSharedImageForOverwrite();
    gpu::MailboxHolder dst_mailbox(
        client_si ? client_si->mailbox() : gpu::Mailbox(), gpu::SyncToken(),
        client_si ? client_si->GetTextureTarget() : GL_TEXTURE_2D);

    // The returned sync token is from the SharedGpuContext - it's ok to drop it
    // here since WebGPUMailboxTexture::FromCanvasResource will generate a new
    // sync token from the SharedContextState and wait on it anyway.
    std::ignore = video_renderer->CopyVideoFrameToSharedImage(
        raster_context_provider, std::move(media_video_frame), dst_mailbox,
        /*use_visible_rect=*/true);
  } else {
    const gfx::Rect dest_rect = media_video_frame->visible_rect();
    // Delegate video transformation to Dawn.
    if (!DrawVideoFrameIntoResourceProvider(
            std::move(media_video_frame), resource_provider,
            raster_context_provider, dest_rect, video_renderer,
            /* ignore_video_transformation */ true)) {
      return {};
    }
  }

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromCanvasResource(
          device->GetDawnControlClient(), device->GetHandle(),
          wgpu::TextureUsage::TextureBinding,
          std::move(recyclable_canvas_resource));
  if (!mailbox_texture) {
    return {};
  }

  wgpu::TextureViewDescriptor view_desc = {};
  wgpu::TextureView plane0 =
      mailbox_texture->GetTexture().CreateView(&view_desc);

  // Set plane for ExternalTexture
  external_texture_desc.plane0 = plane0;

  // Decide whether color space conversion could be skipped.
  external_texture_desc.doYuvToRgbConversionOnly =
      IsSameGamutAndGamma(resource_color_space, dst_color_space);

  // Set color space transformation metas for ExternalTexture
  ColorSpaceConversionConstants color_space_conversion_constants =
      GetColorSpaceConversionConstants(resource_color_space, dst_color_space);

  external_texture_desc.gamutConversionMatrix =
      color_space_conversion_constants.gamut_conversion_matrix.data();
  external_texture_desc.srcTransferFunctionParameters =
      color_space_conversion_constants.src_transfer_constants.data();
  external_texture_desc.dstTransferFunctionParameters =
      color_space_conversion_constants.dst_transfer_constants.data();

  external_texture.wgpu_external_texture =
      device->GetHandle().CreateExternalTexture(&external_texture_desc);
  external_texture.mailbox_texture = std::move(mailbox_texture);

  return external_texture;
}

}  // namespace blink
