// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/shared_image_video_frame_test_utils.h"

#include "base/logging.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"

namespace media {

namespace {

static constexpr const uint8_t kYuvColors[8][3] = {
    {0x00, 0x80, 0x80},  // Black
    {0x4c, 0x54, 0xff},  // Red
    {0x95, 0x2b, 0x15},  // Green
    {0xe1, 0x00, 0x94},  // Yellow
    {0x1d, 0xff, 0x6b},  // Blue
    {0x69, 0xd3, 0xec},  // Magenta
    {0xb3, 0xaa, 0x00},  // Cyan
    {0xff, 0x80, 0x80},  // White
};

// Destroys a list of shared images after a sync token is passed. Also runs
// |callback|.
void DestroySharedImage(scoped_refptr<gpu::ClientSharedImage> shared_image,
                        base::OnceClosure callback,
                        const gpu::SyncToken& sync_token) {
  shared_image->UpdateDestructionSyncToken(sync_token);
  std::move(callback).Run();
}

scoped_refptr<VideoFrame> CreateSharedImageFrame(
    VideoPixelFormat format,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    const gpu::SyncToken& sync_token,
    GLenum texture_target,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    base::OnceClosure destroyed_callback) {
  auto callback = base::BindOnce(&DestroySharedImage, shared_image,
                                 std::move(destroyed_callback));
  auto frame = VideoFrame::WrapSharedImage(
      format, std::move(shared_image), sync_token, std::move(callback),
      coded_size, visible_rect, natural_size, timestamp);
  // Set the format type to take new code path with single multiplanar shared
  // image.
  frame->set_shared_image_format_type(
      SharedImageFormatType::kSharedImageFormat);
  return frame;
}

}  // namespace

scoped_refptr<VideoFrame> CreateSharedImageRGBAFrame(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback) {
  DCHECK_EQ(coded_size.width() % 4, 0);
  DCHECK_EQ(coded_size.height() % 2, 0);
  size_t pixels_size = coded_size.GetArea() * 4;
  std::vector<uint8_t> pixels(pixels_size);
  size_t i = 0;
  for (size_t block_y = 0; block_y < 2u; ++block_y) {
    for (int y = 0; y < coded_size.height() / 2; ++y) {
      for (size_t block_x = 0; block_x < 4u; ++block_x) {
        for (int x = 0; x < coded_size.width() / 4; ++x) {
          pixels[i++] = 0xffu * (block_x % 2);  // R
          pixels[i++] = 0xffu * (block_x / 2);  // G
          pixels[i++] = 0xffu * block_y;        // B
          pixels[i++] = 0xffu;                  // A
        }
      }
    }
  }
  DCHECK_EQ(i, pixels_size);

  // This SharedImage will be read by the raster interface to create
  // intermediate copies in copy to canvas and 2-copy upload to WebGL. It may
  // also be read by the GLES2 interface if the code creating the intermediate
  // SharedImage decides that the VideoFrame can be wrapped directly as a GL
  // texture and/or if raster is going over GLES2 in the context of the test.
  constexpr auto kUsages =
      gpu::SHARED_IMAGE_USAGE_RASTER_READ | gpu::SHARED_IMAGE_USAGE_GLES2_READ;
  auto* sii = context_provider->SharedImageInterface();
  auto shared_image =
      sii->CreateSharedImage({viz::SinglePlaneFormat::kRGBA_8888, coded_size,
                              gfx::ColorSpace(), kUsages, "RGBAVideoFrame"},
                             pixels);

  return CreateSharedImageFrame(
      VideoPixelFormat::PIXEL_FORMAT_ABGR, shared_image, {}, GL_TEXTURE_2D,
      coded_size, visible_rect, visible_rect.size(), base::Seconds(1),
      std::move(destroyed_callback));
}

scoped_refptr<VideoFrame> CreateSharedImageI420Frame(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback) {
  DCHECK_EQ(coded_size.width() % 8, 0);
  DCHECK_EQ(coded_size.height() % 4, 0);
  gfx::Size uv_size(coded_size.width() / 2, coded_size.height() / 2);
  size_t y_pixels_size = coded_size.GetArea();
  size_t uv_pixels_size = uv_size.GetArea();
  std::vector<uint8_t> y_pixels(y_pixels_size);
  std::vector<uint8_t> u_pixels(uv_pixels_size);
  std::vector<uint8_t> v_pixels(uv_pixels_size);
  size_t y_i = 0;
  size_t uv_i = 0;
  for (size_t block_y = 0; block_y < 2u; ++block_y) {
    for (int y = 0; y < coded_size.height() / 2; ++y) {
      for (size_t block_x = 0; block_x < 4u; ++block_x) {
        size_t color_index = block_x + block_y * 4;
        const uint8_t* yuv = kYuvColors[color_index];
        for (int x = 0; x < coded_size.width() / 4; ++x) {
          y_pixels[y_i++] = yuv[0];
          if ((x % 2) && (y % 2)) {
            u_pixels[uv_i] = yuv[1];
            v_pixels[uv_i++] = yuv[2];
          }
        }
      }
    }
  }
  DCHECK_EQ(y_i, y_pixels_size);
  DCHECK_EQ(uv_i, uv_pixels_size);

  auto* sii = context_provider->SharedImageInterface();
  auto* ri = context_provider->RasterInterface();
  // These SharedImages will be read by the raster interface to create
  // intermediate copies in copy to canvas and 2-copy upload to WebGL.
  // In the context of the tests using these SharedImages, GPU rasterization is
  // always used.
  auto usages = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
#if !BUILDFLAG(IS_ANDROID)
  // These SharedImages may be read by the GLES2 interface for 1-copy upload to
  // WebGL (not supported on Android).
  usages |= gpu::SHARED_IMAGE_USAGE_GLES2_READ;
#endif

  // Instead of creating shared image per plane, create a single multiplanar
  // shared image and upload pixels to it.
  auto shared_image =
      sii->CreateSharedImage({viz::MultiPlaneFormat::kI420, coded_size,
                              gfx::ColorSpace(), usages, "I420Frame"},
                             gpu::kNullSurfaceHandle);
  ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes] = {};
  // SkColorType is always Alpha8 for I420 8 bit video frames.
  auto color_type = kAlpha_8_SkColorType;
  SkImageInfo y_info = SkImageInfo::Make(
      coded_size.width(), coded_size.height(), color_type, kPremul_SkAlphaType);
  pixmaps[0] = SkPixmap(y_info, y_pixels.data(), y_info.minRowBytes());
  SkImageInfo u_info = SkImageInfo::Make(uv_size.width(), uv_size.height(),
                                         color_type, kPremul_SkAlphaType);
  pixmaps[1] = SkPixmap(u_info, u_pixels.data(), u_info.minRowBytes());
  SkImageInfo v_info = SkImageInfo::Make(uv_size.width(), uv_size.height(),
                                         color_type, kPremul_SkAlphaType);
  pixmaps[2] = SkPixmap(v_info, v_pixels.data(), v_info.minRowBytes());
  SkYUVAInfo info =
      SkYUVAInfo({coded_size.width(), coded_size.height()},
                 SkYUVAInfo::PlaneConfig::kY_U_V, SkYUVAInfo::Subsampling::k420,
                 kIdentity_SkYUVColorSpace);
  SkYUVAPixmaps yuv_pixmap = SkYUVAPixmaps::FromExternalPixmaps(info, pixmaps);
  ri->WritePixelsYUV(shared_image->mailbox(), yuv_pixmap);

  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  return CreateSharedImageFrame(
      VideoPixelFormat::PIXEL_FORMAT_I420, shared_image, sync_token,
      GL_TEXTURE_2D, coded_size, visible_rect, visible_rect.size(),
      base::Seconds(1), std::move(destroyed_callback));
}

scoped_refptr<VideoFrame> CreateSharedImageNV12Frame(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback) {
  DCHECK_EQ(coded_size.width() % 8, 0);
  DCHECK_EQ(coded_size.height() % 4, 0);
  if (!context_provider->ContextCapabilities().texture_rg) {
    LOG(ERROR) << "GL_EXT_texture_rg not supported";
    return {};
  }
  gfx::Size uv_size(coded_size.width() / 2, coded_size.height() / 2);
  size_t y_pixels_size = coded_size.GetArea();
  size_t uv_pixels_size = uv_size.GetArea() * 2;
  std::vector<uint8_t> y_pixels(y_pixels_size);
  std::vector<uint8_t> uv_pixels(uv_pixels_size);
  size_t y_i = 0;
  size_t uv_i = 0;
  for (size_t block_y = 0; block_y < 2u; ++block_y) {
    for (int y = 0; y < coded_size.height() / 2; ++y) {
      for (size_t block_x = 0; block_x < 4u; ++block_x) {
        size_t color_index = block_x + block_y * 4;
        const uint8_t* yuv = kYuvColors[color_index];
        for (int x = 0; x < coded_size.width() / 4; ++x) {
          y_pixels[y_i++] = yuv[0];
          if ((x % 2) && (y % 2)) {
            uv_pixels[uv_i++] = yuv[1];
            uv_pixels[uv_i++] = yuv[2];
          }
        }
      }
    }
  }
  DCHECK_EQ(y_i, y_pixels_size);
  DCHECK_EQ(uv_i, uv_pixels_size);

  auto* sii = context_provider->SharedImageInterface();
  auto* ri = context_provider->RasterInterface();
  // These SharedImages will be read by the raster interface to create
  // intermediate copies in copy to canvas and 2-copy upload to WebGL.
  // In the context of the tests using these SharedImages, GPU rasterization is
  // always used.
  auto usages = gpu::SHARED_IMAGE_USAGE_RASTER_READ |
                gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
#if !BUILDFLAG(IS_ANDROID)
  // These SharedImages may be read by the GLES2 interface for 1-copy upload to
  // WebGL (not supported on Android).
  usages |= gpu::SHARED_IMAGE_USAGE_GLES2_READ;
#endif
  // Instead of creating shared image per plane, create a single multiplanar
  // shared image and upload pixels to it.
  auto shared_image =
      sii->CreateSharedImage({viz::MultiPlaneFormat::kNV12, coded_size,
                              gfx::ColorSpace(), usages, "NV12Frame"},
                             gpu::kNullSurfaceHandle);
  ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  SkPixmap pixmaps[SkYUVAInfo::kMaxPlanes] = {};
  SkImageInfo y_info =
      SkImageInfo::Make(coded_size.width(), coded_size.height(),
                        kAlpha_8_SkColorType, kPremul_SkAlphaType);
  pixmaps[0] = SkPixmap(y_info, y_pixels.data(), y_info.minRowBytes());
  SkImageInfo uv_info =
      SkImageInfo::Make(uv_size.width(), uv_size.height(),
                        kR8G8_unorm_SkColorType, kPremul_SkAlphaType);
  pixmaps[1] = SkPixmap(uv_info, uv_pixels.data(), uv_info.minRowBytes());

  SkYUVAInfo info = SkYUVAInfo(
      {coded_size.width(), coded_size.height()}, SkYUVAInfo::PlaneConfig::kY_UV,
      SkYUVAInfo::Subsampling::k420, kIdentity_SkYUVColorSpace);
  SkYUVAPixmaps yuv_pixmap = SkYUVAPixmaps::FromExternalPixmaps(info, pixmaps);
  ri->WritePixelsYUV(shared_image->mailbox(), yuv_pixmap);

  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  return CreateSharedImageFrame(
      VideoPixelFormat::PIXEL_FORMAT_NV12, shared_image, sync_token,
      GL_TEXTURE_2D, coded_size, visible_rect, visible_rect.size(),
      base::Seconds(1), std::move(destroyed_callback));
}

}  // namespace media
