// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_yuv_converter.h"

#include <GLES3/gl3.h>

#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/GrYUVABackendTextures.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace media {

namespace {

SkYUVColorSpace ColorSpaceToSkYUVColorSpace(
    const gfx::ColorSpace& color_space) {
  // TODO(crbug.com/828599): This should really default to rec709.
  SkYUVColorSpace sk_color_space = kRec601_SkYUVColorSpace;
  color_space.ToSkYUVColorSpace(&sk_color_space);
  return sk_color_space;
}

viz::ResourceFormat PlaneResourceFormat(int num_channels) {
  switch (num_channels) {
    case 1:
      return viz::LUMINANCE_8;
    case 2:
      return viz::RG_88;
    case 3:
      return viz::RGBX_8888;
    case 4:
      return viz::RGBA_8888;
  }
  NOTREACHED();
  return viz::RGBA_8888;
}

GLenum PlaneGLFormat(int num_channels) {
  return viz::TextureStorageFormat(PlaneResourceFormat(num_channels));
}

std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
VideoPixelFormatToSkiaValues(VideoPixelFormat video_format) {
  // To expand support for additional VideoFormats expand this switch. Note that
  // we do assume 8 bit formats. With that exception, anything else should work.
  switch (video_format) {
    case PIXEL_FORMAT_NV12:
      return {SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_I420:
      return {SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_I420A:
      return {SkYUVAInfo::PlaneConfig::kY_U_V_A, SkYUVAInfo::Subsampling::k420};
    default:
      return {SkYUVAInfo::PlaneConfig::kUnknown,
              SkYUVAInfo::Subsampling::kUnknown};
  }
}

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
      NOTREACHED();
      return kUnknown_SkColorType;
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

bool YUVGrBackendTexturesToSkSurface(
    GrDirectContext* gr_context,
    const VideoFrame* video_frame,
    const GrYUVABackendTextures& yuva_backend_textures,
    sk_sp<SkSurface> surface,
    bool use_visible_rect) {
  auto image = SkImage::MakeFromYUVATextures(gr_context, yuva_backend_textures,
                                             SkColorSpace::MakeSRGB());

  if (!image) {
    return false;
  }

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

  surface->flushAndSubmit();
  return true;
}

}  // namespace

class VideoFrameYUVConverter::VideoFrameYUVMailboxesHolder {
 public:
  VideoFrameYUVMailboxesHolder() = default;
  ~VideoFrameYUVMailboxesHolder() { ReleaseCachedData(); }

  void ReleaseCachedData();
  void ReleaseTextures();

  // Extracts shared image information if |video_frame| is texture backed or
  // creates new shared images and uploads YUV data to GPU if |video_frame| is
  // mappable. This function can be called repeatedly to re-use shared images in
  // the case of CPU backed VideoFrames. The planes are returned in |mailboxes|.
  void VideoFrameToMailboxes(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider,
      gpu::Mailbox mailboxes[]);

  // Like VideoFrameToMailboxes but imports the textures from the mailboxes and
  // returns the planes as a set of YUVA GrBackendTextures.
  GrYUVABackendTextures VideoFrameToSkiaTextures(
      const VideoFrame* video_frame,
      viz::RasterContextProvider* raster_context_provider);

  SkYUVAInfo::PlaneConfig plane_config() const { return plane_config_; }

  SkYUVAInfo::Subsampling subsampling() const { return subsampling_; }

 private:
  static constexpr size_t kMaxPlanes =
      static_cast<size_t>(SkYUVAInfo::kMaxPlanes);

  void ImportTextures();
  size_t NumPlanes() {
    return static_cast<size_t>(SkYUVAInfo::NumPlanes(plane_config_));
  }

  scoped_refptr<viz::RasterContextProvider> provider_;
  bool imported_textures_ = false;
  SkYUVAInfo::PlaneConfig plane_config_ = SkYUVAInfo::PlaneConfig::kUnknown;
  SkYUVAInfo::Subsampling subsampling_ = SkYUVAInfo::Subsampling::kUnknown;
  bool created_shared_images_ = false;
  gfx::Size cached_video_size_;
  gfx::ColorSpace cached_video_color_space_;
  std::array<gpu::MailboxHolder, kMaxPlanes> holders_;

  struct YUVPlaneTextureInfo {
    GrGLTextureInfo texture = {0, 0};
    bool is_shared_image = false;
  };
  std::array<YUVPlaneTextureInfo, kMaxPlanes> textures_;
};

void VideoFrameYUVConverter::VideoFrameYUVMailboxesHolder::ReleaseCachedData() {
  if (holders_[0].mailbox.IsZero())
    return;

  ReleaseTextures();

  // Don't destroy shared images we don't own.
  if (!created_shared_images_)
    return;

  auto* ri = provider_->RasterInterface();
  DCHECK(ri);
  gpu::SyncToken token;
  ri->GenUnverifiedSyncTokenCHROMIUM(token.GetData());

  auto* sii = provider_->SharedImageInterface();
  DCHECK(sii);
  for (auto& mailbox_holder : holders_) {
    if (!mailbox_holder.mailbox.IsZero())
      sii->DestroySharedImage(token, mailbox_holder.mailbox);
    mailbox_holder.mailbox.SetZero();
  }

  created_shared_images_ = false;
}

void VideoFrameYUVConverter::VideoFrameYUVMailboxesHolder::
    VideoFrameToMailboxes(const VideoFrame* video_frame,
                          viz::RasterContextProvider* raster_context_provider,
                          gpu::Mailbox mailboxes[]) {
  std::tie(plane_config_, subsampling_) =
      VideoPixelFormatToSkiaValues(video_frame->format());
  DCHECK_NE(plane_config_, SkYUVAInfo::PlaneConfig::kUnknown);

  // If we have cached shared images but the provider or video has changed we
  // need to release shared images created on the old context and recreate them.
  if (created_shared_images_ &&
      (provider_.get() != raster_context_provider ||
       video_frame->coded_size() != cached_video_size_ ||
       video_frame->ColorSpace() != cached_video_color_space_))
    ReleaseCachedData();
  provider_ = raster_context_provider;
  DCHECK(provider_);
  auto* ri = provider_->RasterInterface();
  DCHECK(ri);

  gfx::Size video_size = video_frame->coded_size();
  SkISize plane_sizes[SkYUVAInfo::kMaxPlanes];
  size_t num_planes = SkYUVAInfo::PlaneDimensions(
      {video_size.width(), video_size.height()}, plane_config_, subsampling_,
      kTopLeft_SkEncodedOrigin, plane_sizes);

  if (video_frame->HasTextures()) {
    DCHECK_EQ(num_planes, video_frame->NumTextures());
    for (size_t plane = 0; plane < video_frame->NumTextures(); ++plane) {
      holders_[plane] = video_frame->mailbox_holder(plane);
      DCHECK(holders_[plane].texture_target == GL_TEXTURE_2D ||
             holders_[plane].texture_target == GL_TEXTURE_EXTERNAL_OES ||
             holders_[plane].texture_target == GL_TEXTURE_RECTANGLE_ARB)
          << "Unsupported texture target " << std::hex << std::showbase
          << holders_[plane].texture_target;
      ri->WaitSyncTokenCHROMIUM(holders_[plane].sync_token.GetConstData());
      mailboxes[plane] = holders_[plane].mailbox;
    }
  } else {
    if (!created_shared_images_) {
      auto* sii = provider_->SharedImageInterface();
      DCHECK(sii);
      uint32_t mailbox_usage;
      if (provider_->ContextCapabilities().supports_oop_raster) {
        mailbox_usage = gpu::SHARED_IMAGE_USAGE_RASTER |
                        gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
      } else {
        mailbox_usage = gpu::SHARED_IMAGE_USAGE_GLES2;
      }
      for (size_t plane = 0; plane < num_planes; ++plane) {
        gfx::Size tex_size = {plane_sizes[plane].width(),
                              plane_sizes[plane].height()};
        int num_channels = SkYUVAInfo::NumChannelsInPlane(plane_config_, plane);
        viz::ResourceFormat format = PlaneResourceFormat(num_channels);
        holders_[plane].mailbox = sii->CreateSharedImage(
            format, tex_size, video_frame->ColorSpace(),
            kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, mailbox_usage,
            gpu::kNullSurfaceHandle);
        holders_[plane].texture_target = GL_TEXTURE_2D;
      }

      // Split up shared image creation from upload so we only have to wait on
      // one sync token.
      ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

      cached_video_size_ = video_frame->coded_size();
      cached_video_color_space_ = video_frame->ColorSpace();
      created_shared_images_ = true;
    }

    // If we have cached shared images that have been imported release them to
    // prevent writing to a shared image for which we're holding read access.
    ReleaseTextures();

    for (size_t plane = 0; plane < num_planes; ++plane) {
      int num_channels = SkYUVAInfo::NumChannelsInPlane(plane_config_, plane);
      SkColorType color_type = SkYUVAPixmapInfo::DefaultColorTypeForDataType(
          SkYUVAPixmaps::DataType::kUnorm8, num_channels);
      SkImageInfo info = SkImageInfo::Make(plane_sizes[plane], color_type,
                                           kUnknown_SkAlphaType);
      ri->WritePixels(holders_[plane].mailbox, 0, 0, GL_TEXTURE_2D,
                      video_frame->stride(plane), info,
                      video_frame->data(plane));
      mailboxes[plane] = holders_[plane].mailbox;
    }
  }
}

GrYUVABackendTextures
VideoFrameYUVConverter::VideoFrameYUVMailboxesHolder::VideoFrameToSkiaTextures(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider) {
  gpu::Mailbox mailboxes[kMaxPlanes];
  VideoFrameToMailboxes(video_frame, raster_context_provider, mailboxes);
  ImportTextures();
  SkISize video_size{video_frame->coded_size().width(),
                     video_frame->coded_size().height()};
  SkYUVAInfo yuva_info(video_size, plane_config_, subsampling_,
                       ColorSpaceToSkYUVColorSpace(video_frame->ColorSpace()));
  GrBackendTexture backend_textures[SkYUVAInfo::kMaxPlanes];
  SkISize plane_sizes[SkYUVAInfo::kMaxPlanes];
  int num_planes = yuva_info.planeDimensions(plane_sizes);
  for (int i = 0; i < num_planes; ++i) {
    backend_textures[i] = {plane_sizes[i].width(), plane_sizes[i].height(),
                           GrMipmapped::kNo, textures_[i].texture};
  }
  return GrYUVABackendTextures(yuva_info, backend_textures,
                               kTopLeft_GrSurfaceOrigin);
}

void VideoFrameYUVConverter::VideoFrameYUVMailboxesHolder::ImportTextures() {
  DCHECK(!imported_textures_)
      << "Textures should always be released after converting video frame. "
         "Call ReleaseTextures() for each call to VideoFrameToSkiaTextures()";

  auto* ri = provider_->RasterInterface();
  for (size_t plane = 0; plane < NumPlanes(); ++plane) {
    textures_[plane].texture.fID =
        ri->CreateAndConsumeForGpuRaster(holders_[plane].mailbox);
    if (holders_[plane].mailbox.IsSharedImage()) {
      textures_[plane].is_shared_image = true;
      ri->BeginSharedImageAccessDirectCHROMIUM(
          textures_[plane].texture.fID,
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    }

    int num_channels = SkYUVAInfo::NumChannelsInPlane(plane_config_, plane);
    textures_[plane].texture.fTarget = holders_[plane].texture_target;
    textures_[plane].texture.fFormat = PlaneGLFormat(num_channels);
  }

  imported_textures_ = true;
}

void VideoFrameYUVConverter::VideoFrameYUVMailboxesHolder::ReleaseTextures() {
  if (!imported_textures_)
    return;

  auto* ri = provider_->RasterInterface();
  DCHECK(ri);
  for (auto& tex_info : textures_) {
    if (!tex_info.texture.fID)
      continue;

    if (tex_info.is_shared_image)
      ri->EndSharedImageAccessDirectCHROMIUM(tex_info.texture.fID);
    ri->DeleteGpuRasterTexture(tex_info.texture.fID);

    tex_info.texture.fID = 0;
  }

  imported_textures_ = false;
}

VideoFrameYUVConverter::VideoFrameYUVConverter() = default;
VideoFrameYUVConverter::~VideoFrameYUVConverter() = default;

bool VideoFrameYUVConverter::IsVideoFrameFormatSupported(
    const VideoFrame& video_frame) {
  return std::get<0>(VideoPixelFormatToSkiaValues(video_frame.format())) !=
         SkYUVAInfo::PlaneConfig::kUnknown;
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrameNoCaching(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder) {
  VideoFrameYUVConverter converter;
  return converter.ConvertYUVVideoFrame(video_frame, raster_context_provider,
                                        dest_mailbox_holder);
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrame(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect) {
  DCHECK(video_frame);
  DCHECK(IsVideoFrameFormatSupported(*video_frame))
      << "VideoFrame has an unsupported YUV format " << video_frame->format();
  DCHECK(!video_frame->coded_size().IsEmpty())
      << "|video_frame| must have an area > 0";
  DCHECK(raster_context_provider);

  if (!holder_)
    holder_ = std::make_unique<VideoFrameYUVMailboxesHolder>();

  if (raster_context_provider->GrContext()) {
    return ConvertFromVideoFrameYUVWithGrContext(
        video_frame, raster_context_provider, dest_mailbox_holder,
        internal_format, type, flip_y, use_visible_rect);
  }

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());
  SkYUVColorSpace color_space =
      ColorSpaceToSkYUVColorSpace(video_frame->ColorSpace());

  gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes]{};
  holder_->VideoFrameToMailboxes(video_frame, raster_context_provider,
                                 mailboxes);
  ri->ConvertYUVAMailboxesToRGB(dest_mailbox_holder.mailbox, color_space,
                                holder_->plane_config(), holder_->subsampling(),
                                mailboxes);
  return true;
}

bool VideoFrameYUVConverter::ConvertYUVVideoFrameToDstTextureNoCaching(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect) {
  VideoFrameYUVConverter converter;
  return converter.ConvertYUVVideoFrame(video_frame, raster_context_provider,
                                        dest_mailbox_holder, internal_format,
                                        type, flip_y, use_visible_rect);
}

void VideoFrameYUVConverter::ReleaseCachedData() {
  holder_.reset();
}

bool VideoFrameYUVConverter::ConvertFromVideoFrameYUVWithGrContext(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect) {
  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());
  GLuint dest_tex_id =
      ri->CreateAndConsumeForGpuRaster(dest_mailbox_holder.mailbox);
  if (dest_mailbox_holder.mailbox.IsSharedImage()) {
    ri->BeginSharedImageAccessDirectCHROMIUM(
        dest_tex_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  }

  bool result = ConvertFromVideoFrameYUVSkia(
      video_frame, raster_context_provider, dest_mailbox_holder.texture_target,
      dest_tex_id, internal_format, type, flip_y, use_visible_rect);

  if (dest_mailbox_holder.mailbox.IsSharedImage())
    ri->EndSharedImageAccessDirectCHROMIUM(dest_tex_id);
  ri->DeleteGpuRasterTexture(dest_tex_id);

  return result;
}

bool VideoFrameYUVConverter::ConvertFromVideoFrameYUVSkia(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_target,
    unsigned int texture_id,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect) {
  // Rendering YUV textures to SkSurface by dst texture
  GrDirectContext* gr_context = raster_context_provider->GrContext();
  DCHECK(gr_context);
  // TODO(crbug.com/674185): We should compare the DCHECK vs when
  // UpdateLastImage calls this function.
  DCHECK(IsVideoFrameFormatSupported(*video_frame));

  GrYUVABackendTextures yuva_backend_textures =
      holder_->VideoFrameToSkiaTextures(video_frame, raster_context_provider);
  DCHECK(yuva_backend_textures.isValid());

  GrGLTextureInfo result_gl_texture_info{};
  result_gl_texture_info.fID = texture_id;
  result_gl_texture_info.fTarget = texture_target;
  result_gl_texture_info.fFormat = GetSurfaceColorFormat(internal_format, type);

  int result_width = use_visible_rect ? video_frame->visible_rect().width()
                                      : video_frame->coded_size().width();
  int result_height = use_visible_rect ? video_frame->visible_rect().height()
                                       : video_frame->coded_size().height();

  GrBackendTexture result_texture(result_width, result_height, GrMipMapped::kNo,
                                  result_gl_texture_info);

  // Use dst texture as SkSurface back resource.
  auto surface = SkSurface::MakeFromBackendTexture(
      gr_context, result_texture,
      flip_y ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin, 1,
      GetCompatibleSurfaceColorType(result_gl_texture_info.fFormat),
      SkColorSpace::MakeSRGB(), nullptr);

  // Terminate if surface cannot be created.
  if (!surface) {
    return false;
  }

  bool result = YUVGrBackendTexturesToSkSurface(gr_context, video_frame,
                                                yuva_backend_textures, surface,
                                                use_visible_rect);

  // Release textures to guarantee |holder_| doesn't hold read access on
  // textures it doesn't own.
  holder_->ReleaseTextures();

  return result;
}

}  // namespace media
