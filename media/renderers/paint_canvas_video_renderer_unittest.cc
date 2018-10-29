// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES3/gl3.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "base/message_loop/message_loop.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "gpu/command_buffer/common/capabilities.h"
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
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/geometry/rect_f.h"

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
  void Paint(const scoped_refptr<VideoFrame>& video_frame,
             cc::PaintCanvas* canvas,
             Color color);
  void PaintRotated(const scoped_refptr<VideoFrame>& video_frame,
                    cc::PaintCanvas* canvas,
                    const gfx::RectF& dest_rect,
                    Color color,
                    SkBlendMode mode,
                    VideoRotation video_rotation);

  void Copy(const scoped_refptr<VideoFrame>& video_frame,
            cc::PaintCanvas* canvas);

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
  base::MessageLoop message_loop_;

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
  renderer_.Paint(nullptr, canvas, kNaturalRect, flags, VIDEO_ROTATION_0,
                  Context3D(), nullptr);
}

void PaintCanvasVideoRendererTest::Paint(
    const scoped_refptr<VideoFrame>& video_frame,
    cc::PaintCanvas* canvas,
    Color color) {
  PaintRotated(video_frame, canvas, kNaturalRect, color, SkBlendMode::kSrcOver,
               VIDEO_ROTATION_0);
}

void PaintCanvasVideoRendererTest::PaintRotated(
    const scoped_refptr<VideoFrame>& video_frame,
    cc::PaintCanvas* canvas,
    const gfx::RectF& dest_rect,
    Color color,
    SkBlendMode mode,
    VideoRotation video_rotation) {
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
  renderer_.Paint(video_frame, canvas, dest_rect, flags, video_rotation,
                  Context3D(), nullptr);
}

void PaintCanvasVideoRendererTest::Copy(
    const scoped_refptr<VideoFrame>& video_frame,
    cc::PaintCanvas* canvas) {
  renderer_.Copy(video_frame, canvas, Context3D(), nullptr);
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
      VIDEO_ROTATION_0);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorRED), bitmap()->getColor(0, 0));
}

TEST_F(PaintCanvasVideoRendererTest, TransparentFrameSrcMode) {
  target_canvas()->clear(SK_ColorRED);
  // SRC mode completely overwrites the buffer.
  PaintRotated(
      VideoFrame::CreateTransparentFrame(gfx::Size(kWidth, kHeight)).get(),
      target_canvas(), kNaturalRect, kNone, SkBlendMode::kSrc,
      VIDEO_ROTATION_0);
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
               SkBlendMode::kSrcOver, VIDEO_ROTATION_90);
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
               SkBlendMode::kSrcOver, VIDEO_ROTATION_180);
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
               SkBlendMode::kSrcOver, VIDEO_ROTATION_270);
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
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_0);
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
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_90);
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
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_180);
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
               kNone, SkBlendMode::kSrcOver, VIDEO_ROTATION_270);
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
  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestY16Frame(gfx::Size(stride, offset_y + bitmap.height()), rect,
                         memory.get(), cropped_frame()->timestamp());

  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setFilterQuality(kNone_SkFilterQuality);
  renderer_.Paint(video_frame, &canvas,
                  gfx::RectF(bitmap.width(), bitmap.height()), flags,
                  VIDEO_ROTATION_0, Context3D(), nullptr);
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

// Test that PaintCanvasVideoRendererTest::Paint doesn't crash when GrContext is
// abandoned.
TEST_F(PaintCanvasVideoRendererTest, ContextLost) {
  sk_sp<const GrGLInterface> null_interface(GrGLCreateNullInterface());
  sk_sp<GrContext> gr_context = GrContext::MakeGL(std::move(null_interface));
  gr_context->abandonContext();

  cc::SkiaPaintCanvas canvas(AllocBitmap(kWidth, kHeight));

  TestGLES2Interface gles2;
  Context3D context_3d(&gles2, gr_context.get());
  gfx::Size size(kWidth, kHeight);
  gpu::MailboxHolder holders[VideoFrame::kMaxPlanes] = {gpu::MailboxHolder(
      gpu::Mailbox::Generate(), gpu::SyncToken(), GL_TEXTURE_RECTANGLE_ARB)};
  auto video_frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_UYVY, holders, base::Bind(MailboxHoldersReleased), size,
      gfx::Rect(size), size, kNoTimestamp);

  cc::PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  renderer_.Paint(video_frame, &canvas, kNaturalRect, flags, VIDEO_ROTATION_90,
                  context_3d, nullptr);
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
  renderer_.Paint(video_frame, &canvas, visible_rect, flags, VIDEO_ROTATION_0,
                  Context3D(), nullptr);

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
  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestY16Frame(gfx::Size(stride, offset_y + height), rect,
                         memory.get(), cropped_frame()->timestamp());

  TestGLES2Interface gles2;
  // Bind the texImage2D callback to verify the uint16 to float32 conversion.
  gles2.teximage2d_callback_ =
      base::Bind([](GLenum target, GLint level, GLint internalformat,
                    GLsizei width, GLsizei height, GLint border, GLenum format,
                    GLenum type, const void* pixels) {
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
  scoped_refptr<media::VideoFrame> video_frame =
      CreateTestY16Frame(gfx::Size(stride, offset_y + height), rect,
                         memory.get(), cropped_frame()->timestamp());

  TestGLES2Interface gles2;
  // Bind the texImage2D callback to verify the uint16 to float32 conversion.
  gles2.texsubimage2d_callback_ =
      base::Bind([](GLenum target, GLint level, GLint xoffset, GLint yoffset,
                    GLsizei width, GLsizei height, GLenum format, GLenum type,
                    const void* pixels) {
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

}  // namespace media
