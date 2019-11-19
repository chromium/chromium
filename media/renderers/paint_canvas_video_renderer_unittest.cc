// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/test/task_environment.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

using media::VideoFrame;

namespace media {

static const int kWidth = 320;
static const int kHeight = 240;
static const gfx::RectF kNaturalRect(kWidth, kHeight);

// Generate frame pixels to provided |external_memory| and wrap it as frame.
scoped_refptr<VideoFrame> CreateTestY16Frame(const gfx::Size& coded_size,
                                             const gfx::Rect& visible_rect,
                                             void* external_memory,
                                             base::TimeDelta timestamp) {
  const int offset_x = visible_rect.x();
  const int offset_y = visible_rect.y();
  const int stride = coded_size.width();
  const size_t byte_size = stride * coded_size.height() * 2;

  // In the visible rect, fill upper byte with [0-255] and lower with [255-0].
  uint16_t* data = static_cast<uint16_t*>(external_memory);
  for (int j = 0; j < visible_rect.height(); j++) {
    for (int i = 0; i < visible_rect.width(); i++) {
      const int value = i + j * visible_rect.width();
      data[(stride * (j + offset_y)) + i + offset_x] =
          ((value & 0xFF) << 8) | (~value & 0xFF);
    }
  }

  return media::VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_Y16, coded_size, visible_rect, visible_rect.size(),
      static_cast<uint8_t*>(external_memory), byte_size, timestamp);
}

// Destroys a list of shared images after a sync token is passed. Also runs
// |callback|.
static void DestroySharedImages(
    scoped_refptr<viz::ContextProvider> context_provider,
    std::vector<gpu::Mailbox> mailboxes,
    base::OnceClosure callback,
    const gpu::SyncToken& sync_token) {
  auto* sii = context_provider->SharedImageInterface();
  for (const auto& mailbox : mailboxes)
    sii->DestroySharedImage(sync_token, mailbox);
  std::move(callback).Run();
}

// Creates a video frame from a set of shared images with a common texture
// target and sync token.
static scoped_refptr<VideoFrame> CreateSharedImageFrame(
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

// Upload pixels to a shared image using GL.
static void UploadPixels(gpu::gles2::GLES2Interface* gl,
                         const gpu::Mailbox& mailbox,
                         const gfx::Size& size,
                         GLenum format,
                         GLenum type,
                         const uint8_t* data) {
  GLuint texture = gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
  gl->BeginSharedImageAccessDirectCHROMIUM(
      texture, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  gl->BindTexture(GL_TEXTURE_2D, texture);
  gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width(), size.height(), format,
                    type, data);
  gl->EndSharedImageAccessDirectCHROMIUM(texture);
  gl->DeleteTextures(1, &texture);
}

// Creates a shared image backed frame in RGBA format, with colors on the shared
// image mapped as follow.
// Bk | R | G | Y
// ---+---+---+---
// Bl | M | C | W
static scoped_refptr<VideoFrame> CreateSharedImageRGBAFrame(
    scoped_refptr<viz::ContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback) {
  DCHECK_EQ(coded_size.width() % 4, 0);
  DCHECK_EQ(coded_size.height() % 2, 0);
  size_t pixels_size = coded_size.GetArea() * 4;
  auto pixels = std::make_unique<uint8_t[]>(pixels_size);
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
  gpu::Mailbox mailbox =
      sii->CreateSharedImage(viz::ResourceFormat::RGBA_8888, coded_size,
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);
  auto* gl = context_provider->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  UploadPixels(gl, mailbox, coded_size, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels.get());
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  return CreateSharedImageFrame(
      std::move(context_provider), VideoPixelFormat::PIXEL_FORMAT_ABGR,
      {mailbox}, sync_token, GL_TEXTURE_2D, coded_size, visible_rect,
      visible_rect.size(), base::TimeDelta::FromSeconds(1),
      std::move(destroyed_callback));
}

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

// Creates a shared image backed frame in I420 format, with colors mapped
// exactly like CreateSharedImageRGBAFrame above, noting that subsamples may get
// interpolated leading to inconsistent colors around the "seams".
static scoped_refptr<VideoFrame> CreateSharedImageI420Frame(
    scoped_refptr<viz::ContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback) {
  DCHECK_EQ(coded_size.width() % 8, 0);
  DCHECK_EQ(coded_size.height() % 4, 0);
  gfx::Size uv_size(coded_size.width() / 2, coded_size.height() / 2);
  size_t y_pixels_size = coded_size.GetArea();
  size_t uv_pixels_size = uv_size.GetArea();
  auto y_pixels = std::make_unique<uint8_t[]>(y_pixels_size);
  auto u_pixels = std::make_unique<uint8_t[]>(uv_pixels_size);
  auto v_pixels = std::make_unique<uint8_t[]>(uv_pixels_size);
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
  gpu::Mailbox y_mailbox =
      sii->CreateSharedImage(viz::ResourceFormat::LUMINANCE_8, coded_size,
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);
  gpu::Mailbox u_mailbox =
      sii->CreateSharedImage(viz::ResourceFormat::LUMINANCE_8, uv_size,
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);
  gpu::Mailbox v_mailbox =
      sii->CreateSharedImage(viz::ResourceFormat::LUMINANCE_8, uv_size,
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);
  auto* gl = context_provider->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  UploadPixels(gl, y_mailbox, coded_size, GL_LUMINANCE, GL_UNSIGNED_BYTE,
               y_pixels.get());
  UploadPixels(gl, u_mailbox, uv_size, GL_LUMINANCE, GL_UNSIGNED_BYTE,
               u_pixels.get());
  UploadPixels(gl, v_mailbox, uv_size, GL_LUMINANCE, GL_UNSIGNED_BYTE,
               v_pixels.get());
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  return CreateSharedImageFrame(
      std::move(context_provider), VideoPixelFormat::PIXEL_FORMAT_I420,
      {y_mailbox, u_mailbox, v_mailbox}, sync_token, GL_TEXTURE_2D, coded_size,
      visible_rect, visible_rect.size(), base::TimeDelta::FromSeconds(1),
      std::move(destroyed_callback));
}

// Creates a shared image backed frame in NV12 format, with colors mapped
// exactly like CreateSharedImageRGBAFrame above.
// This will return nullptr if the necessary extension is not available for NV12
// support.
static scoped_refptr<VideoFrame> CreateSharedImageNV12Frame(
    scoped_refptr<viz::ContextProvider> context_provider,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    base::OnceClosure destroyed_callback) {
  DCHECK_EQ(coded_size.width() % 8, 0);
  DCHECK_EQ(coded_size.height() % 4, 0);
  if (!context_provider->ContextCapabilities().texture_rg)
    return {};
  gfx::Size uv_size(coded_size.width() / 2, coded_size.height() / 2);
  size_t y_pixels_size = coded_size.GetArea();
  size_t uv_pixels_size = uv_size.GetArea() * 2;
  auto y_pixels = std::make_unique<uint8_t[]>(y_pixels_size);
  auto uv_pixels = std::make_unique<uint8_t[]>(uv_pixels_size);
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
  gpu::Mailbox y_mailbox =
      sii->CreateSharedImage(viz::ResourceFormat::LUMINANCE_8, coded_size,
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);
  gpu::Mailbox uv_mailbox =
      sii->CreateSharedImage(viz::ResourceFormat::RG_88, uv_size,
                             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_GLES2);
  auto* gl = context_provider->ContextGL();
  gl->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
  UploadPixels(gl, y_mailbox, coded_size, GL_LUMINANCE, GL_UNSIGNED_BYTE,
               y_pixels.get());
  UploadPixels(gl, uv_mailbox, uv_size, GL_RG, GL_UNSIGNED_BYTE,
               uv_pixels.get());
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  return CreateSharedImageFrame(
      std::move(context_provider), VideoPixelFormat::PIXEL_FORMAT_NV12,
      {y_mailbox, uv_mailbox}, sync_token, GL_TEXTURE_2D, coded_size,
      visible_rect, visible_rect.size(), base::TimeDelta::FromSeconds(1),
      std::move(destroyed_callback));
}

// Readback the contents of a RGBA texture into an array of RGBA values.
static std::unique_ptr<uint8_t[]> ReadbackTexture(
    gpu::gles2::GLES2Interface* gl,
    GLuint texture,
    const gfx::Size& size) {
  size_t pixel_count = size.width() * size.height();
  GLuint fbo = 0;
  gl->GenFramebuffers(1, &fbo);
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texture, 0);
  auto pixels = std::make_unique<uint8_t[]>(pixel_count * 4);
  uint8_t* raw_pixels = pixels.get();
  gl->ReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                 raw_pixels);
  gl->DeleteFramebuffers(1, &fbo);
  return pixels;
}

// Returns a functor that retrieves a SkColor for a given pixel, from raw RGBA
// data.
static auto ColorGetter(uint8_t* pixels, const gfx::Size& size) {
  return [pixels, size](size_t x, size_t y) {
    uint8_t* p = pixels + (size.width() * y + x) * 4;
    return SkColorSetARGB(p[3], p[0], p[1], p[2]);
  };
}

class PaintCanvasVideoRendererTest : public testing::Test {
 public:
  enum Color {
    kNone,
    kRed,
    kGreen,
    kBlue,
  };

  PaintCanvasVideoRendererTest();
  ~PaintCanvasVideoRendererTest() override;

  // Paints to |canvas| using |renderer_| without any frame data.
  void PaintWithoutFrame(cc::PaintCanvas* canvas);

  // Paints the |video_frame| to the |canvas| using |renderer_|, setting the
  // color of |video_frame| to |color| first.
  void Paint(scoped_refptr<VideoFrame> video_frame,
             cc::PaintCanvas* canvas,
             Color color);
  void PaintRotated(scoped_refptr<VideoFrame> video_frame,
                    cc::PaintCanvas* canvas,
                    const gfx::RectF& dest_rect,
                    Color color,
                    SkBlendMode mode,
                    VideoTransformation video_transformation);

  void Copy(scoped_refptr<VideoFrame> video_frame, cc::PaintCanvas* canvas);

  // Getters for various frame sizes.
  scoped_refptr<VideoFrame> natural_frame() { return natural_frame_; }
  scoped_refptr<VideoFrame> larger_frame() { return larger_frame_; }
  scoped_refptr<VideoFrame> smaller_frame() { return smaller_frame_; }
  scoped_refptr<VideoFrame> cropped_frame() { return cropped_frame_; }

  // Standard canvas.
  cc::PaintCanvas* target_canvas() { return &target_canvas_; }
  SkBitmap* bitmap() { return &bitmap_; }

 protected:
  PaintCanvasVideoRenderer renderer_;

  scoped_refptr<VideoFrame> natural_frame_;
  scoped_refptr<VideoFrame> larger_frame_;
  scoped_refptr<VideoFrame> smaller_frame_;
  scoped_refptr<VideoFrame> cropped_frame_;

  SkBitmap bitmap_;
  cc::SkiaPaintCanvas target_canvas_;
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(PaintCanvasVideoRendererTest);
};

static SkBitmap AllocBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType));
  bitmap.eraseColor(0);
  return bitmap;
}

PaintCanvasVideoRendererTest::PaintCanvasVideoRendererTest()
    : natural_frame_(VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight))),
      larger_frame_(
          VideoFrame::CreateBlackFrame(gfx::Size(kWidth * 2, kHeight * 2))),
      smaller_frame_(
          VideoFrame::CreateBlackFrame(gfx::Size(kWidth / 2, kHeight / 2))),
      cropped_frame_(
          VideoFrame::CreateFrame(PIXEL_FORMAT_I420,
                                  gfx::Size(16, 16),
                                  gfx::Rect(6, 6, 8, 6),
                                  gfx::Size(8, 6),
                                  base::TimeDelta::FromMilliseconds(4))),
      bitmap_(AllocBitmap(kWidth, kHeight)),
      target_canvas_(bitmap_) {
  // Give each frame a unique timestamp.
  natural_frame_->set_timestamp(base::TimeDelta::FromMilliseconds(1));
  larger_frame_->set_timestamp(base::TimeDelta::FromMilliseconds(2));
  smaller_frame_->set_timestamp(base::TimeDelta::FromMilliseconds(3));

  // Make sure the cropped video frame's aspect ratio matches the output device.
  // Update cropped_frame_'s crop dimensions if this is not the case.
  EXPECT_EQ(cropped_frame()->visible_rect().width() * kHeight,
            cropped_frame()->visible_rect().height() * kWidth);

  // Fill in the cropped frame's entire data with colors:
  //
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   Bl Bl Bl Bl Bl Bl Bl Bl R  R  R  R  R  R  R  R
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //   G  G  G  G  G  G  G  G  B  B  B  B  B  B  B  B
  //
  // The visible crop of the frame (as set by its visible_rect_) has contents:
  //
  //   Bl Bl R  R  R  R  R  R
  //   Bl Bl R  R  R  R  R  R
  //   G  G  B  B  B  B  B  B
  //   G  G  B  B  B  B  B  B
  //   G  G  B  B  B  B  B  B
  //   G  G  B  B  B  B  B  B
  //
  // Each color region in the cropped frame is on a 2x2 block granularity, to
  // avoid sharing UV samples between regions.

  static const uint8_t cropped_y_plane[] = {
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      0,   0,   0,   0,   0,   0,   0,   0,   76, 76, 76, 76, 76, 76, 76, 76,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
      149, 149, 149, 149, 149, 149, 149, 149, 29, 29, 29, 29, 29, 29, 29, 29,
  };

  static const uint8_t cropped_u_plane[] = {
      128, 128, 128, 128, 84,  84,  84,  84,  128, 128, 128, 128, 84,
      84,  84,  84,  128, 128, 128, 128, 84,  84,  84,  84,  128, 128,
      128, 128, 84,  84,  84,  84,  43,  43,  43,  43,  255, 255, 255,
      255, 43,  43,  43,  43,  255, 255, 255, 255, 43,  43,  43,  43,
      255, 255, 255, 255, 43,  43,  43,  43,  255, 255, 255, 255,
  };
  static const uint8_t cropped_v_plane[] = {
      128, 128, 128, 128, 255, 255, 255, 255, 128, 128, 128, 128, 255,
      255, 255, 255, 128, 128, 128, 128, 255, 255, 255, 255, 128, 128,
      128, 128, 255, 255, 255, 255, 21,  21,  21,  21,  107, 107, 107,
      107, 21,  21,  21,  21,  107, 107, 107, 107, 21,  21,  21,  21,
      107, 107, 107, 107, 21,  21,  21,  21,  107, 107, 107, 107,
  };

  libyuv::I420Copy(cropped_y_plane, 16, cropped_u_plane, 8, cropped_v_plane, 8,
                   cropped_frame()->data(VideoFrame::kYPlane),
                   cropped_frame()->stride(VideoFrame::kYPlane),
                   cropped_frame()->data(VideoFrame::kUPlane),
                   cropped_frame()->stride(VideoFrame::kUPlane),
                   cropped_frame()->data(VideoFrame::kVPlane),
                   cropped_frame()->stride(VideoFrame::kVPlane), 16, 16);
}

PaintCanvasVideoRendererTest::~PaintCanvasVideoRendererTest() = default;

void PaintCanvasVideoRendererTest::PaintWithoutFrame(cc::PaintCanvas* canvas) {
  cc::PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  renderer_.Paint(nullptr, canvas, kNaturalRect, flags, kNoTransformation,
                  nullptr);
}

void PaintCanvasVideoRendererTest::Paint(scoped_refptr<VideoFrame> video_frame,
                                         cc::PaintCanvas* canvas,
                                         Color color) {
  PaintRotated(std::move(video_frame), canvas, kNaturalRect, color,
               SkBlendMode::kSrcOver, kNoTransformation);
}

void PaintCanvasVideoRendererTest::PaintRotated(
    scoped_refptr<VideoFrame> video_frame,
    cc::PaintCanvas* canvas,
    const gfx::RectF& dest_rect,
    Color color,
    SkBlendMode mode,
    VideoTransformation video_transformation) {
  switch (color) {
    case kNone:
      break;
    case kRed:
      media::FillYUV(video_frame.get(), 76, 84, 255);
      break;
    case kGreen:
      media::FillYUV(video_frame.get(), 149, 43, 21);
      break;
    case kBlue:
      media::FillYUV(video_frame.get(), 29, 255, 107);
      break;
  }
  cc::PaintFlags flags;
  flags.setBlendMode(mode);
  flags.setFilterQuality(kLow_SkFilterQuality);
  renderer_.Paint(std::move(video_frame), canvas, dest_rect, flags,
                  video_transformation, nullptr);
}

void PaintCanvasVideoRendererTest::Copy(scoped_refptr<VideoFrame> video_frame,
                                        cc::PaintCanvas* canvas) {
  renderer_.Copy(std::move(video_frame), canvas, nullptr);
}

TEST_F(PaintCanvasVideoRendererTest, NoFrame) {
  // Test that black gets painted over canvas.
  target_canvas()->clear(SK_ColorRED);
  PaintWithoutFrame(target_canvas());
  EXPECT_EQ(SK_ColorBLACK, bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, TransparentFrame) {
  target_canvas()->clear(SK_ColorRED);
  PaintRotated(
      VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
      target_canvas(), kNaturalRect, kNone, SkBlendMode::kSrcOver,
      kNoTransformation);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorRED), bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, TransparentFrameSrcMode) {
  target_canvas()->clear(SK_ColorRED);
  // SRC mode completely overwrites the buffer.
  PaintRotated(
      VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
      target_canvas(), kNaturalRect, kNone, SkBlendMode::kSrc,
      kNoTransformation);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, CopyTransparentFrame) {
  target_canvas()->clear(SK_ColorRED);
  Copy(VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
       target_canvas());
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, Natural) {
  Paint(natural_frame(), target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, Larger) {
  Paint(natural_frame(), target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, bitmap()->getColor(0, 0));

  Paint(larger_frame(), target_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, Smaller) {
  Paint(natural_frame(), target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, bitmap()->getColor(0, 0));

  Paint(smaller_frame(), target_canvas(), kBlue);
  EXPECT_EQ(SK_ColorBLUE, bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, NoTimestamp) {
  VideoFrame* video_frame = natural_frame().get();
  video_frame->set_timestamp(media::kNoTimestamp);
  Paint(video_frame, target_canvas(), kRed);
  EXPECT_EQ(SK_ColorRED, bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, CroppedFrame) {
  Paint(cropped_frame(), target_canvas(), kNone);
  // Check the corners.
  EXPECT_EQ(SK_ColorBLACK, bitmap()->getColor(0, 0));
  EXPECT_EQ(SK_ColorRED, bitmap()->getColor(kWidth - 1, 0));
  EXPECT_EQ(SK_ColorGREEN, bitmap()->getColor(0, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap()->getColor(kWidth - 1, kHeight - 1));
  // Check the interior along the border between color regions.  Note that we're
  // bilinearly upscaling, so we'll need to take care to pick sample points that
  // are just outside the "zone of resampling".
  EXPECT_EQ(SK_ColorBLACK,
            bitmap()->getColor(kWidth * 1 / 8 - 1, kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorRED,
            bitmap()->getColor(kWidth * 3 / 8, kHeight * 1 / 6 - 1));
  EXPECT_EQ(SK_ColorGREEN,
            bitmap()->getColor(kWidth * 1 / 8 - 1, kHeight * 3 / 6));
  EXPECT_EQ(SK_ColorBLUE, bitmap()->getColor(kWidth * 3 / 8, kHeight * 3 / 6));
}

TEST_F(PaintCanvasVideoRendererTest, CroppedFrame_NoScaling) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  const gfx::Rect crop_rect = cropped_frame()->visible_rect();

  // Force painting to a non-zero position on the destination bitmap, to check
  // if the coordinates are calculated properly.
  const int offset_x = 10;
  const int offset_y = 15;
  canvas.translate(offset_x, offset_y);

  // Create a destination canvas with dimensions and scale which would not
  // cause scaling.
  canvas.scale(static_cast<SkScalar>(crop_rect.width()) / kWidth,
               static_cast<SkScalar>(crop_rect.height()) / kHeight);

  Paint(cropped_frame(), &canvas, kNone);

  // Check the corners.
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(offset_x, offset_y));
  EXPECT_EQ(SK_ColorRED,
            bitmap.getColor(offset_x + crop_rect.width() - 1, offset_y));
  EXPECT_EQ(SK_ColorGREEN,
            bitmap.getColor(offset_x, offset_y + crop_rect.height() - 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(offset_x + crop_rect.width() - 1,
                                          offset_y + crop_rect.height() - 1));
}

TEST_F(PaintCanvasVideoRendererTest, Video_Rotation_90) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  PaintRotated(cropped_frame(), &canvas, kNaturalRect, kNone,
               SkBlendMode::kSrcOver, VideoTransformation(VIDEO_ROTATION_90));
  // Check the corners.
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(kWidth - 1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, kHeight - 1));
}

TEST_F(PaintCanvasVideoRendererTest, Video_Rotation_180) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  PaintRotated(cropped_frame(), &canvas, kNaturalRect, kNone,
               SkBlendMode::kSrcOver, VideoTransformation(VIDEO_ROTATION_180));
  // Check the corners.
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(kWidth - 1, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(0, kHeight - 1));
}

TEST_F(PaintCanvasVideoRendererTest, Video_Rotation_270) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  PaintRotated(cropped_frame(), &canvas, kNaturalRect, kNone,
               SkBlendMode::kSrcOver, VideoTransformation(VIDEO_ROTATION_270));
  // Check the corners.
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(kWidth - 1, 0));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(0, kHeight - 1));
}

TEST_F(PaintCanvasVideoRendererTest, Video_Translate) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  canvas.clear(SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver, kNoTransformation);
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor((kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            bitmap.getColor((kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(kWidth / 2, kHeight - 1));
}

TEST_F(PaintCanvasVideoRendererTest, Video_Translate_Rotation_90) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  canvas.clear(SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver,
               VideoTransformation(VIDEO_ROTATION_90));
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor((kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            bitmap.getColor((kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(kWidth / 2, kHeight - 1));
}

TEST_F(PaintCanvasVideoRendererTest, Video_Translate_Rotation_180) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  canvas.clear(SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver,
               VideoTransformation(VIDEO_ROTATION_180));
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor((kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            bitmap.getColor((kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(kWidth / 2, kHeight - 1));
}

TEST_F(PaintCanvasVideoRendererTest, Video_Translate_Rotation_270) {
  SkBitmap bitmap = AllocBitmap(kWidth, kHeight);
  cc::SkiaPaintCanvas canvas(bitmap);
  canvas.clear(SK_ColorMAGENTA);

  PaintRotated(cropped_frame(), &canvas,
               gfx::RectF(kWidth / 2, kHeight / 2, kWidth / 2, kHeight / 2),
               kNone, SkBlendMode::kSrcOver,
               VideoTransformation(VIDEO_ROTATION_270));
  // Check the corners of quadrant 2 and 4.
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, 0));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor((kWidth / 2) - 1, 0));
  EXPECT_EQ(SK_ColorMAGENTA,
            bitmap.getColor((kWidth / 2) - 1, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorMAGENTA, bitmap.getColor(0, (kHeight / 2) - 1));
  EXPECT_EQ(SK_ColorRED, bitmap.getColor(kWidth / 2, kHeight / 2));
  EXPECT_EQ(SK_ColorBLUE, bitmap.getColor(kWidth - 1, kHeight / 2));
  EXPECT_EQ(SK_ColorGREEN, bitmap.getColor(kWidth - 1, kHeight - 1));
  EXPECT_EQ(SK_ColorBLACK, bitmap.getColor(kWidth / 2, kHeight - 1));
}

TEST_F(PaintCanvasVideoRendererTest, HighBitDepth) {
  struct params {
    int bit_depth;
    VideoPixelFormat format;
  } kBitDepthAndFormats[] = {{9, PIXEL_FORMAT_YUV420P9},
                             {10, PIXEL_FORMAT_YUV420P10},
                             {12, PIXEL_FORMAT_YUV420P12}};
  for (const auto param : kBitDepthAndFormats) {
    // Copy cropped_frame into a highbit frame.
    scoped_refptr<VideoFrame> frame(VideoFrame::CreateFrame(
        param.format, cropped_frame()->coded_size(),
        cropped_frame()->visible_rect(), cropped_frame()->natural_size(),
        cropped_frame()->timestamp()));
    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      int width = cropped_frame()->row_bytes(plane);
      uint16_t* dst = reinterpret_cast<uint16_t*>(frame->data(plane));
      uint8_t* src = cropped_frame()->data(plane);
      for (int row = 0; row < cropped_frame()->rows(plane); row++) {
        for (int col = 0; col < width; col++) {
          dst[col] = src[col] << (param.bit_depth - 8);
        }
        src += cropped_frame()->stride(plane);
        dst += frame->stride(plane) / 2;
      }
    }

    Paint(frame, target_canvas(), kNone);
    // Check the corners.
    EXPECT_EQ(SK_ColorBLACK, bitmap()->getColor(0, 0));
    EXPECT_EQ(SK_ColorRED, bitmap()->getColor(kWidth - 1, 0));
    EXPECT_EQ(SK_ColorGREEN, bitmap()->getColor(0, kHeight - 1));
    EXPECT_EQ(SK_ColorBLUE, bitmap()->getColor(kWidth - 1, kHeight - 1));
    // Check the interior along the border between color regions.  Note that
    // we're bilinearly upscaling, so we'll need to take care to pick sample
    // points that are just outside the "zone of resampling".
    EXPECT_EQ(SK_ColorBLACK,
              bitmap()->getColor(kWidth * 1 / 8 - 1, kHeight * 1 / 6 - 1));
    EXPECT_EQ(SK_ColorRED,
              bitmap()->getColor(kWidth * 3 / 8, kHeight * 1 / 6 - 1));
    EXPECT_EQ(SK_ColorGREEN,
              bitmap()->getColor(kWidth * 1 / 8 - 1, kHeight * 3 / 6));
    EXPECT_EQ(SK_ColorBLUE,
              bitmap()->getColor(kWidth * 3 / 8, kHeight * 3 / 6));
  }
}

TEST_F(PaintCanvasVideoRendererTest, Y16) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(16, 16, kPremul_SkAlphaType));

  // |offset_x| and |offset_y| define visible rect's offset to coded rect.
  const int offset_x = 3;
  const int offset_y = 5;
  const int stride = bitmap.width() + offset_x;
  const size_t byte_size = stride * (bitmap.height() + offset_y) * 2;
  std::unique_ptr<unsigned char, base::AlignedFreeDeleter> memory(
      static_cast<unsigned char*>(base::AlignedAlloc(
          byte_size, media::VideoFrame::kFrameAddressAlignment)));
  const gfx::Rect rect(offset_x, offset_y, bitmap.width(), bitmap.height());
  auto video_frame =
      CreateTestY16Frame(gfx::Size(stride, offset_y + bitmap.height()), rect,
                         memory.get(), cropped_frame()->timestamp());

  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setFilterQuality(kNone_SkFilterQuality);
  renderer_.Paint(std::move(video_frame), &canvas,
                  gfx::RectF(bitmap.width(), bitmap.height()), flags,
                  kNoTransformation, nullptr);
  for (int j = 0; j < bitmap.height(); j++) {
    for (int i = 0; i < bitmap.width(); i++) {
      const int value = i + j * bitmap.width();
      EXPECT_EQ(SkColorSetRGB(value, value, value), bitmap.getColor(i, j));
    }
  }
}

namespace {
class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  void GenTextures(GLsizei n, GLuint* textures) override {
    DCHECK_EQ(1, n);
    *textures = 1;
  }

  void TexImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) override {
    if (!teximage2d_callback_.is_null()) {
      teximage2d_callback_.Run(target, level, internalformat, width, height,
                               border, format, type, pixels);
    }
  }

  void TexSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override {
    if (!texsubimage2d_callback_.is_null()) {
      texsubimage2d_callback_.Run(target, level, xoffset, yoffset, width,
                                  height, format, type, pixels);
    }
  }

  base::Callback<void(GLenum target,
                      GLint level,
                      GLint internalformat,
                      GLsizei width,
                      GLsizei height,
                      GLint border,
                      GLenum format,
                      GLenum type,
                      const void* pixels)>
      teximage2d_callback_;

  base::Callback<void(GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLint yoffset,
                      GLsizei width,
                      GLsizei height,
                      GLenum format,
                      GLenum type,
                      const void* pixels)>
      texsubimage2d_callback_;
};

void MailboxHoldersReleased(const gpu::SyncToken& sync_token) {}
}  // namespace

// Test that PaintCanvasVideoRenderer::Paint doesn't crash when GrContext is
// unable to wrap a video frame texture (eg due to being abandoned).
TEST_F(PaintCanvasVideoRendererTest, ContextLost) {
  auto context_provider = viz::TestContextProvider::Create();
  context_provider->BindToCurrentThread();
  context_provider->GrContext()->abandonContext();

  cc::SkiaPaintCanvas canvas(AllocBitmap(kWidth, kHeight));

  gfx::Size size(kWidth, kHeight);
  gpu::MailboxHolder holders[VideoFrame::kMaxPlanes] = {gpu::MailboxHolder(
      gpu::Mailbox::Generate(), gpu::SyncToken(), GL_TEXTURE_RECTANGLE_ARB)};
  auto video_frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_NV12, holders, base::BindOnce(MailboxHoldersReleased), size,
      gfx::Rect(size), size, kNoTimestamp);

  cc::PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  renderer_.Paint(std::move(video_frame), &canvas, kNaturalRect, flags,
                  kNoTransformation, context_provider.get());
}

void EmptyCallback(const gpu::SyncToken& sync_token) {}

TEST_F(PaintCanvasVideoRendererTest, CorrectFrameSizeToVisibleRect) {
  constexpr int fWidth{16}, fHeight{16};
  SkImageInfo imInfo =
      SkImageInfo::MakeN32(fWidth, fHeight, kOpaque_SkAlphaType);

  cc::SkiaPaintCanvas canvas(AllocBitmap(kWidth, kHeight));

  gfx::Size coded_size(fWidth, fHeight);
  gfx::Size visible_size(fWidth / 2, fHeight / 2);

  uint8_t memory[fWidth * fHeight * 2] = {0};

  auto video_frame = media::VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_Y16, coded_size, gfx::Rect(visible_size),
      visible_size, &memory[0], fWidth * fHeight * 2,
      base::TimeDelta::FromMilliseconds(4));

  gfx::RectF visible_rect(visible_size.width(), visible_size.height());
  cc::PaintFlags flags;
  renderer_.Paint(std::move(video_frame), &canvas, visible_rect, flags,
                  kNoTransformation, nullptr);

  EXPECT_EQ(fWidth / 2, renderer_.LastImageDimensionsForTesting().width());
  EXPECT_EQ(fWidth / 2, renderer_.LastImageDimensionsForTesting().height());
}

TEST_F(PaintCanvasVideoRendererTest, TexImage2D_Y16_RGBA32F) {
  // Create test frame.
  // |offset_x| and |offset_y| define visible rect's offset to coded rect.
  const int offset_x = 3;
  const int offset_y = 5;
  const int width = 16;
  const int height = 16;
  const int stride = width + offset_x;
  const size_t byte_size = stride * (height + offset_y) * 2;
  std::unique_ptr<unsigned char, base::AlignedFreeDeleter> memory(
      static_cast<unsigned char*>(base::AlignedAlloc(
          byte_size, media::VideoFrame::kFrameAddressAlignment)));
  const gfx::Rect rect(offset_x, offset_y, width, height);
  auto video_frame =
      CreateTestY16Frame(gfx::Size(stride, offset_y + height), rect,
                         memory.get(), cropped_frame()->timestamp());

  TestGLES2Interface gles2;
  // Bind the texImage2D callback to verify the uint16 to float32 conversion.
  gles2.teximage2d_callback_ =
      base::BindRepeating([](GLenum target, GLint level, GLint internalformat,
                             GLsizei width, GLsizei height, GLint border,
                             GLenum format, GLenum type, const void* pixels) {
        EXPECT_EQ(static_cast<unsigned>(GL_FLOAT), type);
        EXPECT_EQ(static_cast<unsigned>(GL_RGBA), format);
        EXPECT_EQ(GL_RGBA, internalformat);
        EXPECT_EQ(0, border);
        EXPECT_EQ(16, width);
        EXPECT_EQ(16, height);
        EXPECT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
        const float* data = static_cast<const float*>(pixels);
        for (int j = 0; j < height; j++) {
          for (int i = 0; i < width; i++) {
            const int value = i + (height - j - 1) * width;  // flip_y is true.
            float expected_value =
                (((value & 0xFF) << 8) | (~value & 0xFF)) / 65535.f;
            EXPECT_EQ(expected_value, data[(i + j * width) * 4]);
            EXPECT_EQ(expected_value, data[(i + j * width) * 4 + 1]);
            EXPECT_EQ(expected_value, data[(i + j * width) * 4 + 2]);
            EXPECT_EQ(1.0f, data[(i + j * width) * 4 + 3]);
          }
        }
      });
  PaintCanvasVideoRenderer::TexImage2D(
      GL_TEXTURE_2D, 0, &gles2, gpu::Capabilities(), video_frame.get(), 0,
      GL_RGBA, GL_RGBA, GL_FLOAT, true /*flip_y*/, true);
}

TEST_F(PaintCanvasVideoRendererTest, TexSubImage2D_Y16_R32F) {
  // Create test frame.
  // |offset_x| and |offset_y| define visible rect's offset to coded rect.
  const int offset_x = 3;
  const int offset_y = 5;
  const int width = 16;
  const int height = 16;
  const int stride = width + offset_x;
  const size_t byte_size = stride * (height + offset_y) * 2;
  std::unique_ptr<unsigned char, base::AlignedFreeDeleter> memory(
      static_cast<unsigned char*>(base::AlignedAlloc(
          byte_size, media::VideoFrame::kFrameAddressAlignment)));
  const gfx::Rect rect(offset_x, offset_y, width, height);
  auto video_frame =
      CreateTestY16Frame(gfx::Size(stride, offset_y + height), rect,
                         memory.get(), cropped_frame()->timestamp());

  TestGLES2Interface gles2;
  // Bind the texImage2D callback to verify the uint16 to float32 conversion.
  gles2.texsubimage2d_callback_ =
      base::BindRepeating([](GLenum target, GLint level, GLint xoffset,
                             GLint yoffset, GLsizei width, GLsizei height,
                             GLenum format, GLenum type, const void* pixels) {
        EXPECT_EQ(static_cast<unsigned>(GL_FLOAT), type);
        EXPECT_EQ(static_cast<unsigned>(GL_RED), format);
        EXPECT_EQ(2, xoffset);
        EXPECT_EQ(1, yoffset);
        EXPECT_EQ(16, width);
        EXPECT_EQ(16, height);
        EXPECT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
        const float* data = static_cast<const float*>(pixels);
        for (int j = 0; j < height; j++) {
          for (int i = 0; i < width; i++) {
            const int value = i + j * width;  // flip_y is false.
            float expected_value =
                (((value & 0xFF) << 8) | (~value & 0xFF)) / 65535.f;
            EXPECT_EQ(expected_value, data[(i + j * width)]);
          }
        }
      });
  PaintCanvasVideoRenderer::TexSubImage2D(
      GL_TEXTURE_2D, &gles2, video_frame.get(), 0, GL_RED, GL_FLOAT,
      2 /*xoffset*/, 1 /*yoffset*/, false /*flip_y*/, true);
}

// Fixture for tests that require a GL context.
class PaintCanvasVideoRendererWithGLTest : public PaintCanvasVideoRendererTest {
 public:
  using GetColorCallback = base::RepeatingCallback<SkColor(int, int)>;

  void SetUp() override {
    gl::GLSurfaceTestSupport::InitializeOneOff();
    enable_pixels_.emplace();
    media_context_ = base::MakeRefCounted<viz::TestInProcessContextProvider>(
        false /* enable_oop_rasterization */, false /* support_locking */);
    gpu::ContextResult result = media_context_->BindToCurrentThread();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);

    destination_context_ =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            false /* enable_oop_rasterization */, false /* support_locking */);
    result = destination_context_->BindToCurrentThread();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
  }

  void TearDown() override {
    renderer_.ResetCache();
    destination_context_.reset();
    media_context_.reset();
    enable_pixels_.reset();
    viz::TestGpuServiceHolder::ResetInstance();
    gl::GLSurfaceTestSupport::ShutdownGL();
  }

  // Uses CopyVideoFrameTexturesToGLTexture to copy |frame| into a GL texture,
  // reads back its contents, and runs |check_pixels| to validate it.
  template <class CheckPixels>
  void CopyVideoFrameTexturesAndCheckPixels(scoped_refptr<VideoFrame> frame,
                                            CheckPixels check_pixels) {
    auto* destination_gl = destination_context_->ContextGL();
    DCHECK(destination_gl);
    GLenum target = GL_TEXTURE_2D;
    GLuint texture = 0;
    destination_gl->GenTextures(1, &texture);
    destination_gl->BindTexture(target, texture);

    renderer_.CopyVideoFrameTexturesToGLTexture(
        media_context_.get(), destination_gl, frame, target, texture, GL_RGBA,
        GL_RGBA, GL_UNSIGNED_BYTE, 0, false /* premultiply_alpha */,
        false /* flip_y */);

    gfx::Size expected_size = frame->visible_rect().size();

    std::unique_ptr<uint8_t[]> pixels =
        ReadbackTexture(destination_gl, texture, expected_size);
    destination_gl->DeleteTextures(1, &texture);

    auto get_color = base::BindRepeating(
        [](uint8_t* pixels, const gfx::Size& size, int x, int y) {
          uint8_t* p = pixels + (size.width() * y + x) * 4;
          return SkColorSetARGB(p[3], p[0], p[1], p[2]);
        },
        pixels.get(), expected_size);
    check_pixels(get_color);
  }

  // Uses Copy to paint |frame| into a bitmap-backed canvas, then
  // runs |check_pixels| to validate the contents of the canvas.
  template <class CheckPixels>
  void PaintVideoFrameAndCheckPixels(scoped_refptr<VideoFrame> frame,
                                     CheckPixels check_pixels) {
    gfx::Size expected_size = frame->visible_rect().size();
    SkBitmap bitmap =
        AllocBitmap(expected_size.width(), expected_size.height());
    cc::SkiaPaintCanvas canvas(bitmap);
    canvas.clear(SK_ColorGRAY);
    renderer_.Copy(frame, &canvas, media_context_.get());

    auto get_color = base::BindRepeating(
        [](SkBitmap* bitmap, int x, int y) { return bitmap->getColor(x, y); },
        &bitmap);
    check_pixels(get_color);
  }

  // Creates a cropped RGBA VideoFrame. |closure| is run once the shared images
  // backing the VideoFrame have been destroyed.
  scoped_refptr<VideoFrame> CreateTestRGBAFrame(base::OnceClosure closure) {
    return CreateSharedImageRGBAFrame(media_context_, gfx::Size(16, 8),
                                      gfx::Rect(3, 3, 12, 4),
                                      std::move(closure));
  }

  // Checks that the contents of a texture/canvas match the expectations for the
  // cropped RGBA frame above. |get_color| is a callback that returns the actual
  // color at a given pixel location.
  static void CheckRGBAFramePixels(GetColorCallback get_color) {
    EXPECT_EQ(SK_ColorBLACK, get_color.Run(0, 0));
    EXPECT_EQ(SK_ColorRED, get_color.Run(1, 0));
    EXPECT_EQ(SK_ColorRED, get_color.Run(4, 0));
    EXPECT_EQ(SK_ColorGREEN, get_color.Run(5, 0));
    EXPECT_EQ(SK_ColorYELLOW, get_color.Run(9, 0));
    EXPECT_EQ(SK_ColorYELLOW, get_color.Run(11, 0));
    EXPECT_EQ(SK_ColorBLUE, get_color.Run(0, 1));
    EXPECT_EQ(SK_ColorBLUE, get_color.Run(0, 3));
    EXPECT_EQ(SK_ColorMAGENTA, get_color.Run(1, 1));
    EXPECT_EQ(SK_ColorMAGENTA, get_color.Run(4, 1));
    EXPECT_EQ(SK_ColorMAGENTA, get_color.Run(1, 3));
    EXPECT_EQ(SK_ColorMAGENTA, get_color.Run(4, 3));
    EXPECT_EQ(SK_ColorCYAN, get_color.Run(5, 1));
    EXPECT_EQ(SK_ColorCYAN, get_color.Run(5, 3));
    EXPECT_EQ(SK_ColorWHITE, get_color.Run(9, 1));
    EXPECT_EQ(SK_ColorWHITE, get_color.Run(11, 1));
    EXPECT_EQ(SK_ColorWHITE, get_color.Run(9, 3));
    EXPECT_EQ(SK_ColorWHITE, get_color.Run(11, 3));
  }

  // Creates a cropped I420 VideoFrame. |closure| is run once the shared images
  // backing the VideoFrame have been destroyed.
  scoped_refptr<VideoFrame> CreateTestI420Frame(base::OnceClosure closure) {
    return CreateSharedImageI420Frame(media_context_, gfx::Size(16, 8),
                                      gfx::Rect(2, 2, 12, 4),
                                      std::move(closure));
  }
  // Creates a cropped I420 VideoFrame. |closure| is run once the shared images
  // backing the VideoFrame have been destroyed.
  scoped_refptr<VideoFrame> CreateTestI420FrameNotSubset(
      base::OnceClosure closure) {
    return CreateSharedImageI420Frame(media_context_, gfx::Size(16, 8),
                                      gfx::Rect(0, 0, 16, 8),
                                      std::move(closure));
  }

  // Checks that the contents of a texture/canvas match the expectations for the
  // cropped I420 frame above. |get_color| is a callback that returns the actual
  // color at a given pixel location.
  static void CheckI420FramePixels(GetColorCallback get_color) {
    // Avoid checking around the "seams" where subsamples may be interpolated.
    EXPECT_EQ(SK_ColorBLACK, get_color.Run(0, 0));
    EXPECT_EQ(SK_ColorRED, get_color.Run(3, 0));
    EXPECT_EQ(SK_ColorRED, get_color.Run(4, 0));
    EXPECT_EQ(SK_ColorGREEN, get_color.Run(7, 0));
    EXPECT_EQ(SK_ColorGREEN, get_color.Run(8, 0));
    EXPECT_EQ(SK_ColorYELLOW, get_color.Run(11, 0));
    EXPECT_EQ(SK_ColorBLUE, get_color.Run(0, 3));
    EXPECT_EQ(SK_ColorMAGENTA, get_color.Run(3, 3));
    EXPECT_EQ(SK_ColorCYAN, get_color.Run(7, 3));
    EXPECT_EQ(SK_ColorWHITE, get_color.Run(11, 3));
  }

  // Checks that the contents of a texture/canvas match the expectations for the
  // cropped I420 frame above. |get_color| is a callback that returns the actual
  // color at a given pixel location.
  static void CheckI420FramePixelsNotSubset(GetColorCallback get_color) {
    // Avoid checking around the "seams" where subsamples may be interpolated.
    EXPECT_EQ(SK_ColorBLACK, get_color.Run(2, 2));
    EXPECT_EQ(SK_ColorRED, get_color.Run(5, 2));
    EXPECT_EQ(SK_ColorRED, get_color.Run(6, 2));
    EXPECT_EQ(SK_ColorGREEN, get_color.Run(9, 2));
    EXPECT_EQ(SK_ColorGREEN, get_color.Run(10, 2));
    EXPECT_EQ(SK_ColorYELLOW, get_color.Run(13, 2));
    EXPECT_EQ(SK_ColorBLUE, get_color.Run(2, 5));
    EXPECT_EQ(SK_ColorMAGENTA, get_color.Run(5, 5));
    EXPECT_EQ(SK_ColorCYAN, get_color.Run(9, 5));
    EXPECT_EQ(SK_ColorWHITE, get_color.Run(13, 5));
  }

  // Creates a cropped NV12 VideoFrame, or nullptr if the needed extension is
  // not available. |closure| is run once the shared images backing the
  // VideoFrame have been destroyed.
  scoped_refptr<VideoFrame> CreateTestNV12Frame(base::OnceClosure closure) {
    return CreateSharedImageNV12Frame(media_context_, gfx::Size(16, 8),
                                      gfx::Rect(2, 2, 12, 4),
                                      std::move(closure));
  }

  // Checks that the contents of a texture/canvas match the expectations for the
  // cropped NV12 frame above. |get_color| is a callback that returns the actual
  // color at a given pixel location. Note that the expectations are the same as
  // for the I420 frame.
  static void CheckNV12FramePixels(GetColorCallback get_color) {
    CheckI420FramePixels(std::move(get_color));
  }

 protected:
  base::Optional<gl::DisableNullDrawGLBindings> enable_pixels_;
  scoped_refptr<viz::TestInProcessContextProvider> media_context_;
  scoped_refptr<viz::TestInProcessContextProvider> destination_context_;
};

TEST_F(PaintCanvasVideoRendererWithGLTest, CopyVideoFrameYUVDataToGLTexture) {
  auto* destination_gl = destination_context_->ContextGL();
  DCHECK(destination_gl);
  GLenum target = GL_TEXTURE_2D;
  GLuint texture = 0;
  destination_gl->GenTextures(1, &texture);
  destination_gl->BindTexture(target, texture);

  renderer_.CopyVideoFrameYUVDataToGLTexture(
      media_context_.get(), destination_gl, *cropped_frame(), target, texture,
      GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, 0, false /* premultiply_alpha */,
      false /* flip_y */);

  gfx::Size expected_size = cropped_frame()->visible_rect().size();

  std::unique_ptr<uint8_t[]> pixels =
      ReadbackTexture(destination_gl, texture, expected_size);
  auto get_color = ColorGetter(pixels.get(), expected_size);

  // Avoid checking around the seams.
  EXPECT_EQ(SK_ColorBLACK, get_color(0, 0));
  EXPECT_EQ(SK_ColorRED, get_color(3, 0));
  EXPECT_EQ(SK_ColorRED, get_color(7, 0));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 3));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 5));
  EXPECT_EQ(SK_ColorBLUE, get_color(3, 3));
  EXPECT_EQ(SK_ColorBLUE, get_color(7, 5));

  destination_gl->DeleteTextures(1, &texture);
}

TEST_F(PaintCanvasVideoRendererWithGLTest,
       CopyVideoFrameYUVDataToGLTexture_FlipY) {
  auto* destination_gl = destination_context_->ContextGL();
  DCHECK(destination_gl);
  GLenum target = GL_TEXTURE_2D;
  GLuint texture = 0;
  destination_gl->GenTextures(1, &texture);
  destination_gl->BindTexture(target, texture);

  renderer_.CopyVideoFrameYUVDataToGLTexture(
      media_context_.get(), destination_gl, *cropped_frame(), target, texture,
      GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, 0, false /* premultiply_alpha */,
      true /* flip_y */);

  gfx::Size expected_size = cropped_frame()->visible_rect().size();

  std::unique_ptr<uint8_t[]> pixels =
      ReadbackTexture(destination_gl, texture, expected_size);
  auto get_color = ColorGetter(pixels.get(), expected_size);

  // Avoid checking around the seams.
  EXPECT_EQ(SK_ColorBLACK, get_color(0, 5));
  EXPECT_EQ(SK_ColorRED, get_color(3, 5));
  EXPECT_EQ(SK_ColorRED, get_color(7, 5));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 2));
  EXPECT_EQ(SK_ColorGREEN, get_color(0, 0));
  EXPECT_EQ(SK_ColorBLUE, get_color(3, 2));
  EXPECT_EQ(SK_ColorBLUE, get_color(7, 0));

  destination_gl->DeleteTextures(1, &texture);
}

// Checks that we correctly copy a RGBA shared image VideoFrame when using
// CopyVideoFrameYUVDataToGLTexture, including correct cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest,
       CopyVideoFrameTexturesToGLTextureRGBA) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame = CreateTestRGBAFrame(run_loop.QuitClosure());

  CopyVideoFrameTexturesAndCheckPixels(frame, &CheckRGBAFramePixels);

  frame.reset();
  run_loop.Run();
}

// Checks that we correctly copy a RGBA shared image VideoFrame that needs read
// lock fences, when using CopyVideoFrameYUVDataToGLTexture, including correct
// cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest,
       CopyVideoFrameTexturesToGLTextureRGBA_ReadLockFence) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame = CreateTestRGBAFrame(run_loop.QuitClosure());
  frame->metadata()->SetBoolean(VideoFrameMetadata::READ_LOCK_FENCES_ENABLED,
                                true);

  CopyVideoFrameTexturesAndCheckPixels(frame, &CheckRGBAFramePixels);

  frame.reset();
  run_loop.Run();
}

// Checks that we correctly paint a RGBA shared image VideoFrame, including
// correct cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest, PaintRGBA) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame = CreateTestRGBAFrame(run_loop.QuitClosure());

  PaintVideoFrameAndCheckPixels(frame, &CheckRGBAFramePixels);

  frame.reset();
  run_loop.Run();
}

// Checks that we correctly copy an I420 shared image VideoFrame when using
// CopyVideoFrameYUVDataToGLTexture, including correct cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest,
       CopyVideoFrameTexturesToGLTextureI420) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame = CreateTestI420Frame(run_loop.QuitClosure());

  CopyVideoFrameTexturesAndCheckPixels(frame, &CheckI420FramePixels);

  frame.reset();
  run_loop.Run();
}

// Checks that we correctly paint a I420 shared image VideoFrame, including
// correct cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest, PaintI420) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame = CreateTestI420Frame(run_loop.QuitClosure());

  PaintVideoFrameAndCheckPixels(frame, &CheckI420FramePixels);

  frame.reset();
  run_loop.Run();
}

// Checks that we correctly paint a I420 shared image VideoFrame, including
// correct cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest, PaintI420NotSubset) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame =
      CreateTestI420FrameNotSubset(run_loop.QuitClosure());

  PaintVideoFrameAndCheckPixels(frame, &CheckI420FramePixelsNotSubset);

  frame.reset();
  run_loop.Run();
}

// Checks that we correctly copy a NV12 shared image VideoFrame when using
// CopyVideoFrameYUVDataToGLTexture, including correct cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest,
       CopyVideoFrameTexturesToGLTextureNV12) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame = CreateTestNV12Frame(run_loop.QuitClosure());
  if (!frame) {
    LOG(ERROR) << "GL_EXT_texture_rg not supported, skipping NV12 test";
    return;
  }

  CopyVideoFrameTexturesAndCheckPixels(frame, &CheckNV12FramePixels);

  frame.reset();
  run_loop.Run();
}

// Checks that we correctly paint a NV12 shared image VideoFrame, including
// correct cropping.
TEST_F(PaintCanvasVideoRendererWithGLTest, PaintNV12) {
  base::RunLoop run_loop;
  scoped_refptr<VideoFrame> frame = CreateTestNV12Frame(run_loop.QuitClosure());
  if (!frame) {
    LOG(ERROR) << "GL_EXT_texture_rg not supported, skipping NV12 test";
    return;
  }

  PaintVideoFrameAndCheckPixels(frame, &CheckNV12FramePixels);

  frame.reset();
  run_loop.Run();
}

}  // namespace media
