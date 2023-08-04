// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_yuv_converter.h"

#include <GLES3/gl3.h>

#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "media/renderers/video_frame_yuv_mailboxes_holder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace media {

namespace {

SkColorType GetCompatibleSurfaceColorType(GrGLenum format) {
  switch (format) {
    case GL_RGBA8:
      return kRGBA_8888_SkColorType;
    case GL_RGB565:
      return kRGB_565_SkColorType;
    case GL_RGBA16F:
      return kRGBA_F16_SkColorType;
    case GL_RGB8:
      return kRGB_888x_SkColorType;
    case GL_RGB10_A2:
      return kRGBA_1010102_SkColorType;
    case GL_RGBA4:
      return kARGB_4444_SkColorType;
    case GL_SRGB8_ALPHA8:
      return kRGBA_8888_SkColorType;
    default:
      NOTREACHED_NORETURN();
  }
}

GrGLenum GetSurfaceColorFormat(GrGLenum format, GrGLenum type) {
  if (format == GL_RGBA) {
    if (type == GL_UNSIGNED_BYTE)
      return GL_RGBA8;
    if (type == GL_UNSIGNED_SHORT_4_4_4_4)
      return GL_RGBA4;
  }
  if (format == GL_RGB) {
    if (type == GL_UNSIGNED_BYTE)
      return GL_RGB8;
    if (type == GL_UNSIGNED_SHORT_5_6_5)
      return GL_RGB565;
  }
  return format;
}

void DrawYUVImageToSkSurface(const VideoFrame* video_frame,
                             sk_sp<SkImage> image,
                             sk_sp<SkSurface> surface,
                             bool use_visible_rect,
                             GrDirectContext* gr_context) {
  if (!use_visible_rect) {
    surface->getCanvas()->drawImage(image, 0, 0);
  } else {
    // Draw the planar SkImage to the SkSurface wrapping the WebGL texture.
    // Using drawImageRect to draw visible rect from video frame to dst texture.
    const gfx::Rect& visible_rect = video_frame->visible_rect();
    const SkRect src_rect =
        SkRect::MakeXYWH(visible_rect.x(), visible_rect.y(),
                         visible_rect.width(), visible_rect.height());
    const SkRect dst_rect =
        SkRect::MakeWH(visible_rect.width(), visible_rect.height());
    surface->getCanvas()->drawImageRect(image, src_rect, dst_rect,
                                        SkSamplingOptions(), nullptr,
                                        SkCanvas::kStrict_SrcRectConstraint);
  }

  gr_context->flushAndSubmit(surface);
}

}  // namespace

VideoFrameYUVConverter::VideoFrameYUVConverter() = default;
VideoFrameYUVConverter::~VideoFrameYUVConverter() = default;

bool VideoFrameYUVConverter::IsVideoFrameFormatSupported(
    const VideoFrame& video_frame) {
  return std::get<0>(VideoFrameYUVMailboxesHolder::VideoPixelFormatToSkiaValues(
             video_frame.format())) != SkYUVAInfo::PlaneConfig::kUnknown;
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    absl::optional<GrParams> gr_params) {
  VideoFrameYUVConverter converter;
  return converter.ConvertYUVVideoFrame(video_frame, raster_context_provider,
                                        dest_mailbox_holder, gr_params);
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrame(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    absl::optional<GrParams> gr_params) {
  DCHECK(video_frame);
  DCHECK(IsVideoFrameFormatSupported(*video_frame))
      << "VideoFrame has an unsupported YUV format " << video_frame->format();
  DCHECK(!video_frame->coded_size().IsEmpty())
      << "|video_frame| must have an area > 0";
  DCHECK(raster_context_provider);

  if (!holder_)
    holder_ = std::make_unique<VideoFrameYUVMailboxesHolder>();

  if (raster_context_provider->GrContext() &&
      !(raster_context_provider->ContextCapabilities()
            .supports_yuv_rgb_conversion &&
        dest_mailbox_holder.mailbox.IsSharedImage())) {
    return ConvertFromVideoFrameYUVWithGrContext(
        video_frame, raster_context_provider, dest_mailbox_holder,
        gr_params.value_or(GrParams()));
  }

  // The RasterInterface path does not support flip_y or use_visible_rect.
  if (gr_params) {
    DCHECK(!gr_params->flip_y);
    DCHECK(!gr_params->use_visible_rect);
  }

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());

  // TODO(hitawala): Add support for software video decode.
  if (video_frame->shared_image_format_type() !=
          SharedImageFormatType::kLegacy &&
      video_frame->HasTextures()) {
    gpu::Mailbox src_mailbox = video_frame->mailbox_holder(0).mailbox;
    ri->CopySharedImage(src_mailbox, dest_mailbox_holder.mailbox, GL_TEXTURE_2D,
                        0, 0, 0, 0, video_frame->coded_size().width(),
                        video_frame->coded_size().height(),
                        /*unpack_flip_y=*/false,
                        /*unpack_premultiply_alpha=*/false);
  } else {
    gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes]{};
    holder_->VideoFrameToMailboxes(video_frame, raster_context_provider,
                                   mailboxes);
    ri->ConvertYUVAMailboxesToRGB(
        dest_mailbox_holder.mailbox, holder_->yuva_info().yuvColorSpace(),
        nullptr, holder_->yuva_info().planeConfig(),
        holder_->yuva_info().subsampling(), mailboxes);
  }
  return true;
}

void VideoFrameYUVConverter::ReleaseCachedData() {
  holder_.reset();
}

bool VideoFrameYUVConverter::ConvertFromVideoFrameYUVWithGrContext(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    const GrParams& gr_params) {
  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());
  GLuint dest_tex_id =
      ri->CreateAndConsumeForGpuRaster(dest_mailbox_holder.mailbox);
  if (dest_mailbox_holder.mailbox.IsSharedImage()) {
    ri->BeginSharedImageAccessDirectCHROMIUM(
        dest_tex_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  }

  // Rendering YUV textures to SkSurface by dst texture
  GrDirectContext* gr_context = raster_context_provider->GrContext();
  DCHECK(gr_context);
  // TODO(crbug.com/674185): We should compare the DCHECK vs when
  // UpdateLastImage calls this function.
  DCHECK(IsVideoFrameFormatSupported(*video_frame));

  // Create SkSurface with dst texture.
  GrGLTextureInfo result_gl_texture_info{};
  result_gl_texture_info.fID = dest_tex_id;
  result_gl_texture_info.fTarget = dest_mailbox_holder.texture_target;
  result_gl_texture_info.fFormat =
      GetSurfaceColorFormat(gr_params.internal_format, gr_params.type);

  int result_width = gr_params.use_visible_rect
                         ? video_frame->visible_rect().width()
                         : video_frame->coded_size().width();
  int result_height = gr_params.use_visible_rect
                          ? video_frame->visible_rect().height()
                          : video_frame->coded_size().height();

  auto result_texture =
      GrBackendTextures::MakeGL(result_width, result_height,
                                skgpu::Mipmapped::kNo, result_gl_texture_info);

  // Use the same SkColorSpace for the surface and image, so that no color space
  // conversion is performed.
  auto source_and_dest_color_space = SkColorSpace::MakeSRGB();

  // Use dst texture as SkSurface back resource.
  auto surface = SkSurfaces::WrapBackendTexture(
      gr_context, result_texture,
      gr_params.flip_y ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin,
      1, GetCompatibleSurfaceColorType(result_gl_texture_info.fFormat),
      source_and_dest_color_space, nullptr);

  // Terminate if surface cannot be created.
  bool result = false;
  if (surface) {
    auto image = holder_->VideoFrameToSkImage(
        video_frame, raster_context_provider, source_and_dest_color_space);
    if (image) {
      result = true;
      DrawYUVImageToSkSurface(video_frame, image, surface,
                              gr_params.use_visible_rect, gr_context);
    } else {
      DLOG(ERROR) << "Failed to create YUV SkImage";
    }
  } else {
    DLOG(ERROR) << "Failed to create SkSurface";
  }

  // Release textures to guarantee |holder_| doesn't hold read access on
  // textures it doesn't own.
  holder_->ReleaseTextures();

  if (dest_mailbox_holder.mailbox.IsSharedImage())
    ri->EndSharedImageAccessDirectCHROMIUM(dest_tex_id);
  ri->DeleteGpuRasterTexture(dest_tex_id);

  return result;
}

}  // namespace media
