// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/shared_image_video_frame_test_utils.h"

#include "base/logging.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"

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
void DestroySharedImages(scoped_refptr<viz::ContextProvider> context_provider,
                         std::vector<gpu::Mailbox> mailboxes,
                         base::OnceClosure callback,
                         const gpu::SyncToken& sync_token) {
  auto* sii = context_provider->SharedImageInterface();
  for (const auto& mailbox : mailboxes)
    sii->DestroySharedImage(sync_token, mailbox);
  std::move(callback).Run();
}

}  // namespace

scoped_refptr<VideoFrame> CreateSharedImageFrame(
    scoped_refptr<viz::ContextProvider> context_provider,
    VideoPixelFormat format,
    std::vector<gpu::Mailbox> mailboxes,
    const gpu::SyncToken& sync_token,
    GLenum texture_target,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    base::OnceClosure destroyed_callback) {
  gpu::MailboxHolder mailboxes_for_frame[VideoFrame::kMaxPlanes] = {};
  size_t i = 0;
  for (const auto& mailbox : mailboxes) {
    mailboxes_for_frame[i++] =
        gpu::MailboxHolder(mailbox, sync_token, texture_target);
  }
  auto callback =
      base::BindOnce(&DestroySharedImages, std::move(context_provider),
                     std::move(mailboxes), std::move(destroyed_callback));
  return VideoFrame::WrapNativeTextures(format, mailboxes_for_frame,
                                        std::move(callback), coded_size,
                                        visible_rect, natural_size, timestamp);
}

scoped_refptr<VideoFrame> CreateSharedImageRGBAFrame(
    scoped_refptr<viz::ContextProvider> context_provider,
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

  auto* sii = context_provider->SharedImageInterface();
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      viz::SinglePlaneFormat::kRGBA_8888, coded_size, gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_GLES2, pixels);

  return CreateSharedImageFrame(
      std::move(context_provider), VideoPixelFormat::PIXEL_FORMAT_ABGR,
      {mailbox}, {}, GL_TEXTURE_2D, coded_size, visible_rect,
      visible_rect.size(), base::Seconds(1), std::move(destroyed_callback));
}

scoped_refptr<VideoFrame> CreateSharedImageI420Frame(
    scoped_refptr<viz::ContextProvider> context_provider,
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

  auto plane_format = context_provider->ContextCapabilities().texture_rg
                          ? viz::SinglePlaneFormat::kR_8
                          : viz::SinglePlaneFormat::kLUMINANCE_8;
  auto* sii = context_provider->SharedImageInterface();
  gpu::Mailbox y_mailbox = sii->CreateSharedImage(
      plane_format, coded_size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, gpu::SHARED_IMAGE_USAGE_GLES2, y_pixels);
  gpu::Mailbox u_mailbox = sii->CreateSharedImage(
      plane_format, uv_size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, gpu::SHARED_IMAGE_USAGE_GLES2, u_pixels);
  gpu::Mailbox v_mailbox = sii->CreateSharedImage(
      plane_format, uv_size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, gpu::SHARED_IMAGE_USAGE_GLES2, v_pixels);

  return CreateSharedImageFrame(
      std::move(context_provider), VideoPixelFormat::PIXEL_FORMAT_I420,
      {y_mailbox, u_mailbox, v_mailbox}, {}, GL_TEXTURE_2D, coded_size,
      visible_rect, visible_rect.size(), base::Seconds(1),
      std::move(destroyed_callback));
}

scoped_refptr<VideoFrame> CreateSharedImageNV12Frame(
    scoped_refptr<viz::ContextProvider> context_provider,
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
  gpu::Mailbox y_mailbox = sii->CreateSharedImage(
      viz::SinglePlaneFormat::kR_8, coded_size, gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_GLES2, y_pixels);
  gpu::Mailbox uv_mailbox = sii->CreateSharedImage(
      viz::SinglePlaneFormat::kRG_88, uv_size, gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_GLES2, uv_pixels);
  return CreateSharedImageFrame(
      std::move(context_provider), VideoPixelFormat::PIXEL_FORMAT_NV12,
      {y_mailbox, uv_mailbox}, {}, GL_TEXTURE_2D, coded_size, visible_rect,
      visible_rect.size(), base::Seconds(1), std::move(destroyed_callback));
}

}  // namespace media
