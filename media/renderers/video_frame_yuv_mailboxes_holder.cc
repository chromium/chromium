// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_frame_yuv_mailboxes_holder.h"

#include <GLES3/gl3.h>

#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace media {

namespace {

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

GrYUVABackendTextures VideoFrameYUVMailboxesHolder::VideoFrameToSkiaTextures(
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

SkYUVAPixmaps VideoFrameYUVMailboxesHolder::VideoFrameToSkiaPixmaps(
    const VideoFrame* video_frame) {
  std::tie(plane_config_, subsampling_) =
      VideoPixelFormatToSkiaValues(video_frame->format());
  DCHECK_NE(plane_config_, SkYUVAInfo::PlaneConfig::kUnknown);

  SkISize video_size{video_frame->coded_size().width(),
                     video_frame->coded_size().height()};
  SkYUVAInfo yuva_info(video_size, plane_config_, subsampling_,
                       ColorSpaceToSkYUVColorSpace(video_frame->ColorSpace()));
  SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes];
  SkISize plane_sizes[SkYUVAInfo::kMaxPlanes];
  int num_planes = yuva_info.planeDimensions(plane_sizes);

  // Create SkImageInfos with the appropriate color types for 8 bit unorm data
  // based on plane config.
  size_t row_bytes[kMaxPlanes];
  for (int i = 0; i < num_planes; ++i) {
    row_bytes[i] =
        VideoFrame::RowBytes(i, video_frame->format(), plane_sizes[i].width());
  }

  SkYUVAPixmapInfo pixmaps_infos(yuva_info, SkYUVAPixmaps::DataType::kUnorm8,
                                 row_bytes);

  for (int i = 0; i < num_planes; ++i) {
    pixmaps[i].reset(pixmaps_infos.planeInfo(i), video_frame->data(i),
                     pixmaps_infos.rowBytes(i));
  }

  return SkYUVAPixmaps::FromExternalPixmaps(yuva_info, pixmaps);
}

void VideoFrameYUVMailboxesHolder::ImportTextures() {
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
SkYUVColorSpace VideoFrameYUVMailboxesHolder::ColorSpaceToSkYUVColorSpace(
    const gfx::ColorSpace& color_space) {
  // TODO(crbug.com/828599): This should really default to rec709.
  SkYUVColorSpace sk_color_space = kRec601_SkYUVColorSpace;
  color_space.ToSkYUVColorSpace(&sk_color_space);
  return sk_color_space;
}

// static
std::tuple<SkYUVAInfo::PlaneConfig, SkYUVAInfo::Subsampling>
VideoFrameYUVMailboxesHolder::VideoPixelFormatToSkiaValues(
    VideoPixelFormat video_format) {
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

}  // namespace media
