// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_yuv_mailboxes_holder.h"

#include <GLES3/gl3.h>

#include "base/logging.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace media {

namespace {

viz::SharedImageFormat PlaneSharedImageFormat(int num_channels,
                                              bool supports_red) {
  switch (num_channels) {
    case 1:
      return supports_red ? viz::SinglePlaneFormat::kR_8
                          : viz::SinglePlaneFormat::kLUMINANCE_8;
    case 2:
      return viz::SinglePlaneFormat::kRG_88;
    case 3:
      return viz::SinglePlaneFormat::kRGBX_8888;
    case 4:
      return viz::SinglePlaneFormat::kRGBA_8888;
  }
  NOTREACHED_NORETURN();
}

GLenum PlaneGLFormat(int num_channels,
                     viz::RasterContextProvider* context_provider) {
  return context_provider->GetGrGLTextureFormat(PlaneSharedImageFormat(
      num_channels, context_provider->ContextCapabilities().texture_rg));
}

}  // namespace

VideoFrameYUVMailboxesHolder::VideoFrameYUVMailboxesHolder() = default;

VideoFrameYUVMailboxesHolder::~VideoFrameYUVMailboxesHolder() {
  ReleaseCachedData();
}

void VideoFrameYUVMailboxesHolder::ReleaseCachedData() {
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

void VideoFrameYUVMailboxesHolder::VideoFrameToMailboxes(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes]) {
  yuva_info_ = VideoFrameGetSkYUVAInfo(video_frame);
  num_planes_ = yuva_info_.planeDimensions(plane_sizes_);

  // If we have cached shared images but the provider or video has changed we
  // need to release shared images created on the old context and recreate them.
  if (created_shared_images_ &&
      (provider_.get() != raster_context_provider ||
       video_frame->coded_size() != cached_video_size_ ||
       video_frame->ColorSpace() != cached_video_color_space_)) {
    ReleaseCachedData();
  }
  provider_ = raster_context_provider;
  DCHECK(provider_);
  auto* ri = provider_->RasterInterface();
  DCHECK(ri);

  if (video_frame->HasTextures()) {
    DCHECK_EQ(num_planes_, video_frame->NumTextures());
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
    return;
  }

  // Create a shared image to upload the data to, if one doesn't exist already.
  if (!created_shared_images_) {
    auto* sii = provider_->SharedImageInterface();
    DCHECK(sii);
    uint32_t mailbox_usage;
    auto& caps = provider_->ContextCapabilities();
    if (caps.supports_oop_raster) {
      mailbox_usage = gpu::SHARED_IMAGE_USAGE_RASTER |
                      gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
    } else {
      mailbox_usage = gpu::SHARED_IMAGE_USAGE_GLES2;
    }
    for (size_t plane = 0; plane < num_planes_; ++plane) {
      gfx::Size tex_size = {plane_sizes_[plane].width(),
                            plane_sizes_[plane].height()};
      int num_channels = yuva_info_.numChannelsInPlane(plane);
      viz::SharedImageFormat format =
          PlaneSharedImageFormat(num_channels, caps.texture_rg);
      holders_[plane].mailbox = sii->CreateSharedImage(
          format, tex_size, video_frame->ColorSpace(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType, mailbox_usage, "VideoFrameYUV",
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

  for (size_t plane = 0; plane < num_planes_; ++plane) {
    int num_channels = yuva_info_.numChannelsInPlane(plane);
    SkColorType color_type = SkYUVAPixmapInfo::DefaultColorTypeForDataType(
        SkYUVAPixmaps::DataType::kUnorm8, num_channels);
    SkImageInfo info = SkImageInfo::Make(plane_sizes_[plane], color_type,
                                         kUnknown_SkAlphaType);
    ri->WritePixels(
        holders_[plane].mailbox, /*dst_x_offset=*/0,
        /*dst_y_offset=*/0, /*dst_plane_index=*/0, GL_TEXTURE_2D,
        SkPixmap(info, video_frame->data(plane), video_frame->stride(plane)));
    mailboxes[plane] = holders_[plane].mailbox;
  }
}

GrYUVABackendTextures VideoFrameYUVMailboxesHolder::VideoFrameToSkiaTextures(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    bool for_surface) {
  gpu::Mailbox mailboxes[kMaxPlanes];
  VideoFrameToMailboxes(video_frame, raster_context_provider, mailboxes);
  ImportTextures(for_surface);
  GrBackendTexture backend_textures[SkYUVAInfo::kMaxPlanes];
  for (size_t plane = 0; plane < num_planes_; ++plane) {
    backend_textures[plane] = {plane_sizes_[plane].width(),
                               plane_sizes_[plane].height(), GrMipmapped::kNo,
                               textures_[plane].texture};
  }
  return GrYUVABackendTextures(yuva_info_, backend_textures,
                               kTopLeft_GrSurfaceOrigin);
}

sk_sp<SkImage> VideoFrameYUVMailboxesHolder::VideoFrameToSkImage(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    sk_sp<SkColorSpace> reinterpret_color_space) {
  GrDirectContext* gr_context = raster_context_provider->GrContext();
  DCHECK(gr_context);

  GrYUVABackendTextures yuva_backend_textures = VideoFrameToSkiaTextures(
      video_frame, raster_context_provider, /*for_surface=*/false);
  auto rgb_color_space =
      reinterpret_color_space
          ? reinterpret_color_space
          : video_frame->ColorSpace().GetAsFullRangeRGB().ToSkColorSpace();

  DCHECK(yuva_backend_textures.isValid());
  auto result = SkImages::TextureFromYUVATextures(
      gr_context, yuva_backend_textures, rgb_color_space);
  DCHECK(result);
  return result;
}

bool VideoFrameYUVMailboxesHolder::VideoFrameToPlaneSkSurfaces(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    sk_sp<SkSurface> surfaces[SkYUVAInfo::kMaxPlanes]) {
  for (size_t plane = 0; plane < SkYUVAInfo::kMaxPlanes; ++plane)
    surfaces[plane] = nullptr;

  if (!video_frame->HasTextures()) {
    // The below call to VideoFrameToSkiaTextures would blit |video_frame| into
    // a temporary SharedImage, which would be exposed as a SkSurface. That is
    // probably undesirable (it has no current use cases), so just return an
    // error.
    DLOG(ERROR) << "VideoFrameToPlaneSkSurfaces requires texture backing.";
    return false;
  }

  GrDirectContext* gr_context = raster_context_provider->GrContext();
  DCHECK(gr_context);
  GrYUVABackendTextures yuva_backend_textures = VideoFrameToSkiaTextures(
      video_frame, raster_context_provider, /*for_surface=*/true);

  bool result = true;
  for (size_t plane = 0; plane < num_planes_; ++plane) {
    const int num_channels = yuva_info_.numChannelsInPlane(plane);
    SkColorType color_type = SkYUVAPixmapInfo::DefaultColorTypeForDataType(
        SkYUVAPixmaps::DataType::kUnorm8, num_channels);
    // Gray is not renderable.
    if (color_type == kGray_8_SkColorType)
      color_type = kAlpha_8_SkColorType;

    auto surface = SkSurfaces::WrapBackendTexture(
        gr_context, yuva_backend_textures.texture(plane),
        kTopLeft_GrSurfaceOrigin, /*sampleCnt=*/1, color_type,
        SkColorSpace::MakeSRGB(), nullptr);
    if (!surface) {
      DLOG(ERROR)
          << "VideoFrameToPlaneSkSurfaces failed to make surface for plane "
          << plane << " of " << num_planes_ << ".";
      result = false;
    }
    surfaces[plane] = surface;
  }
  return result;
}

void VideoFrameYUVMailboxesHolder::ImportTextures(bool for_surface) {
  DCHECK(!imported_textures_)
      << "Textures should always be released after converting video frame. "
         "Call ReleaseTextures() for each call to VideoFrameToSkiaTextures()";

  auto* ri = provider_->RasterInterface();
  for (size_t plane = 0; plane < num_planes_; ++plane) {
    textures_[plane].texture.fID =
        ri->CreateAndConsumeForGpuRaster(holders_[plane].mailbox);
    if (holders_[plane].mailbox.IsSharedImage()) {
      textures_[plane].is_shared_image = true;
      ri->BeginSharedImageAccessDirectCHROMIUM(
          textures_[plane].texture.fID,
          for_surface ? GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM
                      : GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    } else {
      textures_[plane].is_shared_image = false;
    }

    int num_channels = yuva_info_.numChannelsInPlane(plane);
    textures_[plane].texture.fTarget = holders_[plane].texture_target;
    textures_[plane].texture.fFormat =
        PlaneGLFormat(num_channels, provider_.get());
  }

  imported_textures_ = true;
}

void VideoFrameYUVMailboxesHolder::ReleaseTextures() {
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

// static
std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
VideoFrameYUVMailboxesHolder::VideoPixelFormatToSkiaValues(
    VideoPixelFormat video_format) {
  // To expand support for additional VideoFormats expand this switch. Note that
  // we do assume 8 bit formats. With that exception, anything else should work.
  switch (video_format) {
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_P016LE:
      return {SkYUVAInfo::PlaneConfig::kY_UV, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_NV12A:
      return {SkYUVAInfo::PlaneConfig::kY_UV_A, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_I420:
      return {SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k420};
    case PIXEL_FORMAT_I420A:
      return {SkYUVAInfo::PlaneConfig::kY_U_V_A, SkYUVAInfo::Subsampling::k420};
    default:
      return {SkYUVAInfo::PlaneConfig::kUnknown,
              SkYUVAInfo::Subsampling::kUnknown};
  }
}

// static
SkYUVAInfo VideoFrameYUVMailboxesHolder::VideoFrameGetSkYUVAInfo(
    const VideoFrame* video_frame) {
  SkISize video_size{video_frame->coded_size().width(),
                     video_frame->coded_size().height()};
  auto plane_config = SkYUVAInfo::PlaneConfig::kUnknown;
  auto subsampling = SkYUVAInfo::Subsampling::kUnknown;
  std::tie(plane_config, subsampling) =
      VideoPixelFormatToSkiaValues(video_frame->format());

  // TODO(crbug.com/828599): This should really default to rec709.
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(video_frame->BitDepth(),
                                              &color_space);
  return SkYUVAInfo(video_size, plane_config, subsampling, color_space);
}

}  // namespace media
