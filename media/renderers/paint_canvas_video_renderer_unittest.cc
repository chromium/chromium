// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/paint_canvas_video_renderer.h"

#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/byte_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
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
#include "media/renderers/shared_image_video_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gfx/color_space.h"
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

  PaintCanvasVideoRendererTest(const PaintCanvasVideoRendererTest&) = delete;
  PaintCanvasVideoRendererTest& operator=(const PaintCanvasVideoRendererTest&) =
      delete;

  ~PaintCanvasVideoRendererTest() override;

  // Paints to |canvas| using |renderer_| without any frame data.
  void PaintWithoutFrame(cc::PaintCanvas* canvas);

  // Set `video_frame` to `color`.
  void FillFrameWithColor(scoped_refptr<VideoFrame> video_frame, Color color);

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
  base::test::TaskEnvironment task_environment_;
};

static SkBitmap AllocBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(width, height, kPremul_SkAlphaType));
  bitmap.eraseColor(0);
  return bitmap;
}

static scoped_refptr<VideoFrame> CreateCroppedFrame() {
  scoped_refptr<VideoFrame> cropped_frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, gfx::Size(16, 16), gfx::Rect(6, 6, 8, 6),
      gfx::Size(8, 6), base::Milliseconds(4));
  // Make sure the cropped video frame's aspect ratio matches the output device.
  // Update cropped_frame_'s crop dimensions if this is not the case.
  EXPECT_EQ(cropped_frame->visible_rect().width() * kHeight,
            cropped_frame->visible_rect().height() * kWidth);

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
                   cropped_frame->writable_data(VideoFrame::Plane::kY),
                   cropped_frame->stride(VideoFrame::Plane::kY),
                   cropped_frame->writable_data(VideoFrame::Plane::kU),
                   cropped_frame->stride(VideoFrame::Plane::kU),
                   cropped_frame->writable_data(VideoFrame::Plane::kV),
                   cropped_frame->stride(VideoFrame::Plane::kV), 16, 16);

  return cropped_frame;
}

PaintCanvasVideoRendererTest::PaintCanvasVideoRendererTest()
    : natural_frame_(VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight))),
      larger_frame_(
          VideoFrame::CreateBlackFrame(gfx::Size(kWidth * 2, kHeight * 2))),
      smaller_frame_(
          VideoFrame::CreateBlackFrame(gfx::Size(kWidth / 2, kHeight / 2))),
      cropped_frame_(CreateCroppedFrame()),
      bitmap_(AllocBitmap(kWidth, kHeight)),
      target_canvas_(bitmap_) {
  // Give each frame a unique timestamp.
  natural_frame_->set_timestamp(base::Milliseconds(1));
  larger_frame_->set_timestamp(base::Milliseconds(2));
  smaller_frame_->set_timestamp(base::Milliseconds(3));
}

PaintCanvasVideoRendererTest::~PaintCanvasVideoRendererTest() = default;

void PaintCanvasVideoRendererTest::FillFrameWithColor(
    scoped_refptr<VideoFrame> video_frame,
    Color color) {
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
}

void PaintCanvasVideoRendererTest::PaintWithoutFrame(cc::PaintCanvas* canvas) {
  cc::PaintFlags flags;
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
  PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect = kNaturalRect;
  renderer_.Paint(nullptr, canvas, flags, params, nullptr);
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
  FillFrameWithColor(video_frame, color);
  cc::PaintFlags flags;
  flags.setBlendMode(mode);
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
  PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect = dest_rect;
  params.transformation = video_transformation;
  renderer_.Paint(std::move(video_frame), canvas, flags, params, nullptr);
}

void PaintCanvasVideoRendererTest::Copy(scoped_refptr<VideoFrame> video_frame,
                                        cc::PaintCanvas* canvas) {
  renderer_.Copy(std::move(video_frame), canvas, nullptr);
}

TEST_F(PaintCanvasVideoRendererTest, NoFrame) {
  // Test that black gets painted over canvas.
  target_canvas()->clear(SkColors::kRed);
  PaintWithoutFrame(target_canvas());
  EXPECT_EQ(SK_ColorBLACK, bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, TransparentFrame) {
  target_canvas()->clear(SkColors::kRed);
  PaintRotated(
      VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
      target_canvas(), kNaturalRect, kNone, SkBlendMode::kSrcOver,
      kNoTransformation);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorRED), bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, TransparentFrameSrcMode) {
  target_canvas()->clear(SkColors::kRed);
  // SRC mode completely overwrites the buffer.
  PaintRotated(
      VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
      target_canvas(), kNaturalRect, kNone, SkBlendMode::kSrc,
      kNoTransformation);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, TransparentFrameSrcMode1x1) {
  target_canvas()->clear(SkColors::kRed);
  // SRC mode completely overwrites the buffer.
  auto frame = VideoFrame::CreateTransparentFrame(gfx::Size(1, 1));
  PaintRotated(frame.get(), target_canvas(), gfx::RectF(1, 1), kNone,
               SkBlendMode::kSrc, kNoTransformation);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, CopyTransparentFrame) {
  target_canvas()->clear(SkColors::kRed);
  Copy(VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
       target_canvas());
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
            bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, ReinterpretAsSRGB) {
  FillFrameWithColor(natural_frame(), kRed);
  natural_frame()->set_color_space(gfx::ColorSpace::CreateHDR10());

  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrcOver);
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);

  PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect = kNaturalRect;
  renderer_.Paint(natural_frame(), target_canvas(), flags, params, nullptr);
  EXPECT_NE(SK_ColorRED, bitmap()->getColor(0, 0));

  params.reinterpret_as_srgb = true;
  renderer_.Paint(natural_frame(), target_canvas(), flags, params, nullptr);
  EXPECT_EQ(SK_ColorRED, bitmap()->getColor(0, 0));
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

uint32_t MaybeConvertABGRToARGB(uint32_t abgr) {
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
  return abgr;
#else
  return (base::ByteSwap(abgr & 0x00FFFFFF) >> 8) | (abgr & 0xFF000000);
#endif
}

TEST_F(PaintCanvasVideoRendererTest, CroppedFrameToRGBParallel) {
  // We need a test frame large enough to trigger parallel conversion. So we use
  // cropped_frame() as a base and scale it up. Note: Visible rect and natural
  // size must be even.
  auto test_frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, gfx::Size(3840, 2160), gfx::Rect(1440, 810, 1920, 810),
      gfx::Size(1920, 810), base::TimeDelta());

  // Fill in the frame with the same data as the cropped frame.
  libyuv::I420Scale(cropped_frame()->data(0), cropped_frame()->stride(0),
                    cropped_frame()->data(1), cropped_frame()->stride(1),
                    cropped_frame()->data(2), cropped_frame()->stride(2),
                    cropped_frame()->coded_size().width(),
                    cropped_frame()->coded_size().height(),
                    test_frame->writable_data(0), test_frame->stride(0),
                    test_frame->writable_data(1), test_frame->stride(1),
                    test_frame->writable_data(2), test_frame->stride(2),
                    test_frame->coded_size().width(),
                    test_frame->coded_size().height(), libyuv::kFilterNone);

  const gfx::Size visible_size = test_frame->visible_rect().size();
  const size_t row_bytes = visible_size.width() * sizeof(SkColor);
  const size_t allocation_size = row_bytes * visible_size.height();

  std::unique_ptr<uint8_t, base::AlignedFreeDeleter> memory(
      static_cast<uint8_t*>(base::AlignedAlloc(
          allocation_size, media::VideoFrame::kFrameAddressAlignment)));
  memset(memory.get(), 0, allocation_size);

  PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
      test_frame.get(), memory.get(), row_bytes);

  const uint32_t* rgb_pixels = reinterpret_cast<uint32_t*>(memory.get());

  // Check the corners; this is sufficient to reveal https://crbug.com/1027442.
  EXPECT_EQ(SK_ColorBLACK, rgb_pixels[0]);
  EXPECT_EQ(MaybeConvertABGRToARGB(SK_ColorRED),
            rgb_pixels[visible_size.width() - 1]);
  EXPECT_EQ(SK_ColorGREEN,
            rgb_pixels[visible_size.width() * (visible_size.height() - 1)]);
  EXPECT_EQ(MaybeConvertABGRToARGB(SK_ColorBLUE),
            rgb_pixels[(visible_size.width() - 1) * visible_size.height()]);
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
  canvas.clear(SkColors::kMagenta);

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
  canvas.clear(SkColors::kMagenta);

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
  canvas.clear(SkColors::kMagenta);

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
  canvas.clear(SkColors::kMagenta);

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
    for (int plane = VideoFrame::Plane::kY; plane <= VideoFrame::Plane::kV;
         ++plane) {
      int width = cropped_frame()->row_bytes(plane);
      uint16_t* dst = reinterpret_cast<uint16_t*>(frame->writable_data(plane));
      const uint8_t* src = cropped_frame()->data(plane);
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
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kNone);
  PaintCanvasVideoRenderer::PaintParams paint_params;
  paint_params.dest_rect = gfx::RectF(bitmap.width(), bitmap.height());
  renderer_.Paint(std::move(video_frame), &canvas, flags, paint_params,
                  nullptr);
  for (int j = 0; j < bitmap.height(); j++) {
    for (int i = 0; i < bitmap.width(); i++) {
      const int value = i + j * bitmap.width();
      EXPECT_EQ(SkColorSetRGB(value, value, value), bitmap.getColor(i, j));
    }
  }
}

// A reproducer test case for crbug.com/1230409 if run with ASAN enabled.
TEST_F(PaintCanvasVideoRendererTest, Yuv420P12OddWidth) {
  // Allocate the Y, U, V planes for a 3x3 12-bit YUV 4:2:0 image. Note that
  // there are no padding bytes after each row.
  constexpr int kImgWidth = 3;
  constexpr int kImgHeight = 3;
  constexpr int kUvWidth = (kImgWidth + 1) / 2;
  constexpr int kUvHeight = (kImgHeight + 1) / 2;
  std::unique_ptr<uint16_t[]> y_plane =
      std::make_unique<uint16_t[]>(kImgWidth * kImgHeight);
  std::unique_ptr<uint16_t[]> u_plane =
      std::make_unique<uint16_t[]>(kUvWidth * kUvHeight);
  std::unique_ptr<uint16_t[]> v_plane =
      std::make_unique<uint16_t[]>(kUvWidth * kUvHeight);
  // Set all pixels to white.
  for (int i = 0; i < kImgHeight; ++i) {
    for (int j = 0; j < kImgWidth; ++j) {
      y_plane[i * kImgWidth + j] = 4095;
    }
  }
  for (int i = 0; i < kUvHeight; ++i) {
    for (int j = 0; j < kUvWidth; ++j) {
      u_plane[i * kUvWidth + j] = 2048;
      v_plane[i * kUvWidth + j] = 2048;
    }
  }
  const int32_t y_stride = sizeof(uint16_t) * kImgWidth;
  const int32_t uv_stride = sizeof(uint16_t) * kUvWidth;
  uint8_t* const y_data = reinterpret_cast<uint8_t*>(y_plane.get());
  uint8_t* const u_data = reinterpret_cast<uint8_t*>(u_plane.get());
  uint8_t* const v_data = reinterpret_cast<uint8_t*>(v_plane.get());

  auto size = gfx::Size(kImgWidth, kImgHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_YUV420P12, size, gfx::Rect(size), size, y_stride, uv_stride,
      uv_stride, y_data, u_data, v_data, base::TimeDelta());

  std::unique_ptr<uint32_t[]> rgba =
      std::make_unique<uint32_t[]>(kImgWidth * kImgHeight);
  PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
      frame.get(), rgba.get(), frame->visible_rect().width() * 4,
      /*premultiply_alpha=*/true);
  for (int i = 0; i < kImgHeight; ++i) {
    for (int j = 0; j < kImgWidth; ++j) {
      EXPECT_EQ(rgba[i * kImgWidth + j], 0xffffffff);
    }
  }
}

TEST_F(PaintCanvasVideoRendererTest, I420WithFilters) {
  // Allocate the Y, U, V planes for a 4x4 8-bit YUV 4:2:0 image. Note that
  // there are no padding bytes after each row.
  constexpr int kImgWidth = 4;
  constexpr int kImgHeight = 4;
  constexpr int kUvWidth = (kImgWidth + 1) / 2;
  constexpr int kUvHeight = (kImgHeight + 1) / 2;
  std::unique_ptr<uint8_t[]> y_plane =
      std::make_unique<uint8_t[]>(kImgWidth * kImgHeight);
  std::unique_ptr<uint8_t[]> u_plane =
      std::make_unique<uint8_t[]>(kUvWidth * kUvHeight);
  std::unique_ptr<uint8_t[]> v_plane =
      std::make_unique<uint8_t[]>(kUvWidth * kUvHeight);
  // In the JPEG color space (K_R = 0.299, K_B = 0.114, full range), red
  // (R = 255, G = 0, B = 0) is Y = 76, U = 85, V = 255.
  //
  // Set Y to 76 for all pixels.
  memset(y_plane.get(), 76, kImgWidth * kImgHeight);
  // Set U = 85 and V = 255 for the upperleft pixel. Then vary U and V with a
  // linear, diagonal slope over the UV planes with a step size of 4 and -4,
  // respectively.
  //
  // The full U plane is
  //  85  89  93  97
  //  89  93  97 101
  //  93  97 101 105
  //  97 101 105 109
  // The subsampled U plane is
  //    89      97
  //    97     105
  //
  // The full V plane is
  // 255 251 247 243
  // 251 247 243 239
  // 247 243 239 235
  // 243 239 235 231
  // The subsampled V plane is
  //   251     243
  //   243     235
  for (int i = 0; i < kUvHeight; ++i) {
    for (int j = 0; j < kUvWidth; ++j) {
      u_plane[i * kUvWidth + j] = 89 + 8 * i + 8 * j;
      v_plane[i * kUvWidth + j] = 251 - 8 * i - 8 * j;
    }
  }

  auto size = gfx::Size(kImgWidth, kImgHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalYuvData(
      PIXEL_FORMAT_I420, size, gfx::Rect(size), size, kImgWidth, kUvWidth,
      kUvWidth, y_plane.get(), u_plane.get(), v_plane.get(), base::TimeDelta());
  frame->set_color_space(gfx::ColorSpace::CreateJpeg());

  std::unique_ptr<uint32_t[]> rgba =
      std::make_unique<uint32_t[]>(kImgWidth * kImgHeight);

  // First convert with kFilterNone (nearest neighbor).
  PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
      frame.get(), rgba.get(), frame->visible_rect().width() * 4,
      /*premultiply_alpha=*/true);

  // The pixel at coordinates (1, 1) will have U = 89 and V = 251 if nearest
  // neighbor is used. (The correct values are U = 93 and V = 247.)
  int i = 1;
  int j = 1;
  uint32_t color = rgba[i * kImgWidth + j];
  EXPECT_EQ(SkGetPackedA32(color), 255u);
  EXPECT_EQ(SkGetPackedR32(color), 249u);
  EXPECT_EQ(SkGetPackedG32(color), 1u);
  EXPECT_EQ(SkGetPackedB32(color), 7u);
  // The pixel at coordinates (2, 2) will have U = 105 and V = 235 if nearest
  // neighbor is used. (The correct values are U = 101 and V = 239.)
  i = 2;
  j = 2;
  color = rgba[i * kImgWidth + j];
  EXPECT_EQ(SkGetPackedA32(color), 255u);
  EXPECT_EQ(SkGetPackedR32(color), 226u);
  EXPECT_EQ(SkGetPackedG32(color), 7u);
  EXPECT_EQ(SkGetPackedB32(color), 35u);

  // Then convert with kFilterBilinear (bilinear interpolation).
  PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
      frame.get(), rgba.get(), frame->visible_rect().width() * 4,
      /*premultiply_alpha=*/true, PaintCanvasVideoRenderer::kFilterBilinear);

  // The pixel at coordinates (1, 1) will have the correct values U = 93 and
  // V = 247 if bilinear interpolation is used.
  i = 1;
  j = 1;
  color = rgba[i * kImgWidth + j];
  EXPECT_EQ(SkGetPackedA32(color), 255u);
  EXPECT_EQ(SkGetPackedR32(color), 243u);
  EXPECT_EQ(SkGetPackedG32(color), 2u);
  EXPECT_EQ(SkGetPackedB32(color), 14u);
  // The pixel at coordinates (2, 2) will have the correct values U = 101 and
  // V = 239 if bilinear interpolation is used.
  i = 2;
  j = 2;
  color = rgba[i * kImgWidth + j];
  EXPECT_EQ(SkGetPackedA32(color), 255u);
  EXPECT_EQ(SkGetPackedR32(color), 232u);
  EXPECT_EQ(SkGetPackedG32(color), 5u);
  EXPECT_EQ(SkGetPackedB32(color), 28u);
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

  base::RepeatingCallback<void(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels)>
      teximage2d_callback_;

  base::RepeatingCallback<void(GLenum target,
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

#if !BUILDFLAG(IS_ANDROID)
void MailboxHoldersReleased(const gpu::SyncToken& sync_token) {}
#endif
}  // namespace

// NOTE: The below test tests behavior when PaintCanvasVideoRenderer is used
// without GPU raster. It is not relevant on Android, where GPU raster is
// always used.
#if !BUILDFLAG(IS_ANDROID)
// Test that PaintCanvasVideoRenderer::Paint doesn't crash when GrContext is
// unable to wrap a video frame texture (eg due to being abandoned).
TEST_F(PaintCanvasVideoRendererTest, ContextLost) {
  auto context_provider = viz::TestContextProvider::Create();
  CHECK(context_provider);
  context_provider->BindToCurrentSequence();
  CHECK(context_provider->GrContext());
  context_provider->GrContext()->abandonContext();

  cc::SkiaPaintCanvas canvas(AllocBitmap(kWidth, kHeight));

  gfx::Size size(kWidth, kHeight);
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu::ClientSharedImage::CreateForTesting();
  auto video_frame = VideoFrame::WrapSharedImage(
      PIXEL_FORMAT_NV12, shared_image, gpu::SyncToken(),
      base::BindOnce(MailboxHoldersReleased), size, gfx::Rect(size), size,
      kNoTimestamp);

  cc::PaintFlags flags;
  flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
  PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect = kNaturalRect;
  renderer_.Paint(std::move(video_frame), &canvas, flags, params,
                  context_provider.get());
}
#endif

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
      visible_size, &memory[0], fWidth * fHeight * 2, base::Milliseconds(4));

  cc::PaintFlags flags;
  PaintCanvasVideoRenderer::PaintParams params;
  params.dest_rect = gfx::RectF(visible_size.width(), visible_size.height());
  renderer_.Paint(std::move(video_frame), &canvas, flags, params, nullptr);

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

// Fixture for tests that require a GL context as destination.
class PaintCanvasVideoRendererWithGLTest : public testing::Test {
 public:
  using GetColorCallback = base::RepeatingCallback<SkColor(int, int)>;

  void SetUp() override {
    display_ = gl::GLSurfaceTestSupport::InitializeOneOff();
    enable_pixels_.emplace();
    media_context_ = base::MakeRefCounted<viz::TestInProcessContextProvider>(
        viz::TestContextType::kGpuRaster, /*support_locking=*/false);
    gpu::ContextResult result = media_context_->BindToCurrentSequence();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);

    raster_context_ = base::MakeRefCounted<viz::TestInProcessContextProvider>(
        viz::TestContextType::kGpuRaster, /*support_locking=*/false);
    result = raster_context_->BindToCurrentSequence();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);

    destination_context_ =
        base::MakeRefCounted<viz::TestInProcessContextProvider>(
            viz::TestContextType::kGLES2, /*support_locking=*/false);
    result = destination_context_->BindToCurrentSequence();
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    cropped_frame_ = CreateCroppedFrame();
  }

  void TearDown() override {
    renderer_.ResetCache();
    destination_context_.reset();
    raster_context_.reset();
    media_context_.reset();
    enable_pixels_.reset();
    viz::TestGpuServiceHolder::ResetInstance();
    gl::GLSurfaceTestSupport::ShutdownGL(display_);
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
    canvas.clear(SkColors::kGray);
    renderer_.Copy(frame, &canvas, media_context_.get());

    auto get_color = base::BindRepeating(
        [](SkBitmap* bitmap, int x, int y) { return bitmap->getColor(x, y); },
        &bitmap);
    check_pixels(get_color);
  }

  // Creates a cropped RGBA VideoFrame. |closure| is run once the shared images
  // backing the VideoFrame have been destroyed.
  scoped_refptr<VideoFrame> CreateTestRGBAFrame(base::OnceClosure closure) {
    return CreateSharedImageRGBAFrame(raster_context_, gfx::Size(16, 8),
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
    return CreateSharedImageI420Frame(raster_context_, gfx::Size(16, 8),
                                      gfx::Rect(2, 2, 12, 4),
                                      std::move(closure));
  }
  // Creates a cropped I420 VideoFrame. |closure| is run once the shared images
  // backing the VideoFrame have been destroyed.
  scoped_refptr<VideoFrame> CreateTestI420FrameNotSubset(
      base::OnceClosure closure) {
    return CreateSharedImageI420Frame(raster_context_, gfx::Size(16, 8),
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
    return CreateSharedImageNV12Frame(raster_context_, gfx::Size(16, 8),
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

  scoped_refptr<VideoFrame> cropped_frame() { return cropped_frame_; }

 protected:
  std::optional<gl::DisableNullDrawGLBindings> enable_pixels_;
  scoped_refptr<viz::TestInProcessContextProvider> media_context_;
  scoped_refptr<viz::TestInProcessContextProvider> raster_context_;
  scoped_refptr<viz::TestInProcessContextProvider> destination_context_;

  PaintCanvasVideoRenderer renderer_;
  scoped_refptr<VideoFrame> cropped_frame_;
  base::test::TaskEnvironment task_environment_;
  raw_ptr<gl::GLDisplay> display_ = nullptr;
};

TEST_F(PaintCanvasVideoRendererWithGLTest, CopyVideoFrameYUVDataToGLTexture) {
  auto* destination_gl = destination_context_->ContextGL();
  DCHECK(destination_gl);
  GLenum target = GL_TEXTURE_2D;
  GLuint texture = 0;
  destination_gl->GenTextures(1, &texture);
  destination_gl->BindTexture(target, texture);

  renderer_.CopyVideoFrameYUVDataToGLTexture(
      media_context_.get(), destination_gl, cropped_frame(), target, texture,
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
      media_context_.get(), destination_gl, cropped_frame(), target, texture,
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
  frame->metadata().read_lock_fences_enabled = true;

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
