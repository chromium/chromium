// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_util.h"

#include <stdint.h>

#include <cmath>
#include <memory>

#include "base/macros.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Initialize a plane's visible rect with value circularly from 0 to 255.
void FillPlaneWithPattern(uint8_t* data,
                          int stride,
                          const gfx::Size& visible_size) {
  DCHECK(data && visible_size.width() <= stride);

  uint32_t val = 0;
  uint8_t* src = data;
  for (int i = 0; i < visible_size.height(); ++i, src += stride) {
    for (int j = 0; j < visible_size.width(); ++j, ++val)
      src[j] = val & 0xff;
  }
}

// Create a VideoFrame and initialize the visible rect using
// |FillPlaneWithPattern()|. For testing purpose, the VideoFrame should be
// filled with varying values, which is different from
// |VideoFrame::CreateColorFrame()| where the entrire VideoFrame is filled
// with a given color.
scoped_refptr<media::VideoFrame> CreateFrameWithPatternFilled(
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  scoped_refptr<media::VideoFrame> frame(media::VideoFrame::CreateFrame(
      format, coded_size, visible_rect, natural_size, timestamp));

  FillPlaneWithPattern(frame->data(media::VideoFrame::kYPlane),
                       frame->stride(media::VideoFrame::kYPlane),
                       frame->visible_rect().size());
  FillPlaneWithPattern(
      frame->data(media::VideoFrame::kUPlane),
      frame->stride(media::VideoFrame::kUPlane),
      media::VideoFrame::PlaneSize(format, media::VideoFrame::kUPlane,
                                   frame->visible_rect().size()));
  FillPlaneWithPattern(
      frame->data(media::VideoFrame::kVPlane),
      frame->stride(media::VideoFrame::kVPlane),
      media::VideoFrame::PlaneSize(format, media::VideoFrame::kVPlane,
                                   frame->visible_rect().size()));
  return frame;
}

// Helper function used to verify the data in the coded region after copying the
// visible region and padding the remaining area.
bool VerifyPlanCopyWithPadding(const uint8_t* src,
                               size_t src_stride,
                               // Size of visible region.
                               const gfx::Size& src_size,
                               const uint8_t* dst,
                               size_t dst_stride,
                               // Coded size of |dst|.
                               const gfx::Size& dst_size) {
  if (!src || !dst)
    return false;

  const size_t src_width = src_size.width();
  const size_t src_height = src_size.height();
  const size_t dst_width = dst_size.width();
  const size_t dst_height = dst_size.height();
  if (src_width > dst_width || src_width > src_stride ||
      src_height > dst_height || src_size.IsEmpty() || dst_size.IsEmpty())
    return false;

  const uint8_t *src_ptr = src, *dst_ptr = dst;
  for (size_t i = 0; i < src_height;
       ++i, src_ptr += src_stride, dst_ptr += dst_stride) {
    if (memcmp(src_ptr, dst_ptr, src_width))
      return false;
    for (size_t j = src_width; j < dst_width; ++j) {
      if (src_ptr[src_width - 1] != dst_ptr[j])
        return false;
    }
  }
  if (src_height < dst_height) {
    src_ptr = dst + (src_height - 1) * dst_stride;
    if (memcmp(src_ptr, dst_ptr, dst_width))
      return false;
  }
  return true;
}

bool VerifyCopyWithPadding(const media::VideoFrame& src_frame,
                           const media::VideoFrame& dst_frame) {
  if (!src_frame.IsMappable() || !dst_frame.IsMappable() ||
      src_frame.visible_rect().size() != dst_frame.visible_rect().size())
    return false;

  if (!VerifyPlanCopyWithPadding(
          src_frame.visible_data(media::VideoFrame::kYPlane),
          src_frame.stride(media::VideoFrame::kYPlane),
          src_frame.visible_rect().size(),
          dst_frame.data(media::VideoFrame::kYPlane),
          dst_frame.stride(media::VideoFrame::kYPlane), dst_frame.coded_size()))
    return false;
  if (!VerifyPlanCopyWithPadding(
          src_frame.visible_data(media::VideoFrame::kUPlane),
          src_frame.stride(media::VideoFrame::kUPlane),
          media::VideoFrame::PlaneSize(media::PIXEL_FORMAT_I420,
                                       media::VideoFrame::kUPlane,
                                       src_frame.visible_rect().size()),
          dst_frame.data(media::VideoFrame::kUPlane),
          dst_frame.stride(media::VideoFrame::kUPlane),
          media::VideoFrame::PlaneSize(media::PIXEL_FORMAT_I420,
                                       media::VideoFrame::kUPlane,
                                       dst_frame.coded_size())))
    return false;
  if (!VerifyPlanCopyWithPadding(
          src_frame.visible_data(media::VideoFrame::kVPlane),
          src_frame.stride(media::VideoFrame::kVPlane),
          media::VideoFrame::PlaneSize(media::PIXEL_FORMAT_I420,
                                       media::VideoFrame::kVPlane,
                                       src_frame.visible_rect().size()),
          dst_frame.data(media::VideoFrame::kVPlane),
          dst_frame.stride(media::VideoFrame::kVPlane),
          media::VideoFrame::PlaneSize(media::PIXEL_FORMAT_I420,
                                       media::VideoFrame::kVPlane,
                                       dst_frame.coded_size())))
    return false;

  return true;
}

}  // namespace

namespace media {

class VideoUtilTest : public testing::Test {
 public:
  VideoUtilTest()
      : height_(0),
        y_stride_(0),
        u_stride_(0),
        v_stride_(0) {
  }

  ~VideoUtilTest() override = default;

  void CreateSourceFrame(int width, int height,
                         int y_stride, int u_stride, int v_stride) {
    EXPECT_GE(y_stride, width);
    EXPECT_GE(u_stride, width / 2);
    EXPECT_GE(v_stride, width / 2);

    height_ = height;
    y_stride_ = y_stride;
    u_stride_ = u_stride;
    v_stride_ = v_stride;

    y_plane_.reset(new uint8_t[y_stride * height]);
    u_plane_.reset(new uint8_t[u_stride * height / 2]);
    v_plane_.reset(new uint8_t[v_stride * height / 2]);
  }

  void CreateDestinationFrame(int width, int height) {
    gfx::Size size(width, height);
    destination_frame_ = VideoFrame::CreateFrame(
        PIXEL_FORMAT_I420, size, gfx::Rect(size), size, base::TimeDelta());
  }

 private:
  std::unique_ptr<uint8_t[]> y_plane_;
  std::unique_ptr<uint8_t[]> u_plane_;
  std::unique_ptr<uint8_t[]> v_plane_;

  int height_;
  int y_stride_;
  int u_stride_;
  int v_stride_;

  scoped_refptr<VideoFrame> destination_frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoUtilTest);
};

TEST_F(VideoUtilTest, GetPixelAspectRatio) {
  gfx::Rect visible_rect(320, 240);

  // Test empty or invalid combinations.
  EXPECT_TRUE(std::isnan(GetPixelAspectRatio(gfx::Rect(), gfx::Size())));
  EXPECT_TRUE(std::isnan(GetPixelAspectRatio(gfx::Rect(1, 1), gfx::Size())));
  EXPECT_TRUE(std::isnan(GetPixelAspectRatio(gfx::Rect(), gfx::Size(1, 1))));
  EXPECT_TRUE(
      std::isinf(GetPixelAspectRatio(gfx::Rect(1, 1), gfx::Size(1, 0))));
  EXPECT_EQ(0.0, GetPixelAspectRatio(gfx::Rect(1, 1), gfx::Size(0, 1)));
  EXPECT_EQ(0.0, GetPixelAspectRatio(gfx::Rect(1, 0), gfx::Size(1, 1)));
  EXPECT_TRUE(
      std::isinf(GetPixelAspectRatio(gfx::Rect(0, 1), gfx::Size(1, 1))));

  // Some normal ratios.
  EXPECT_DOUBLE_EQ(1.0, GetPixelAspectRatio(visible_rect, gfx::Size(320, 240)));
  EXPECT_DOUBLE_EQ(2.0, GetPixelAspectRatio(visible_rect, gfx::Size(640, 240)));
  EXPECT_DOUBLE_EQ(0.5, GetPixelAspectRatio(visible_rect, gfx::Size(320, 480)));
}

TEST_F(VideoUtilTest, GetNaturalSize_Double) {
  gfx::Rect visible_rect(320, 240);

  // Test 0 sizes.
  EXPECT_EQ(gfx::Size(0, 0), GetNaturalSize(gfx::Rect(0, 0), 1.0));
  EXPECT_EQ(gfx::Size(0, 1), GetNaturalSize(gfx::Rect(0, 1), 1.0));
  EXPECT_EQ(gfx::Size(1, 0), GetNaturalSize(gfx::Rect(1, 0), 1.0));

  // Test abnormal ratios.
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_rect, NAN));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_rect, 0.0));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_rect, INFINITY));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_rect, -INFINITY));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_rect, -1.0));

  // Test normal sizes and ratios.
  EXPECT_EQ(gfx::Size(320, 240), GetNaturalSize(visible_rect, 1.0 / 1.0));
  EXPECT_EQ(gfx::Size(640, 240), GetNaturalSize(visible_rect, 2.0 / 1.0));
  EXPECT_EQ(gfx::Size(320, 480), GetNaturalSize(visible_rect, 1.0 / 2.0));
  EXPECT_EQ(gfx::Size(427, 240), GetNaturalSize(visible_rect, 4.0 / 3.0));
  EXPECT_EQ(gfx::Size(320, 320), GetNaturalSize(visible_rect, 3.0 / 4.0));
  EXPECT_EQ(gfx::Size(569, 240), GetNaturalSize(visible_rect, 16.0 / 9.0));
  EXPECT_EQ(gfx::Size(320, 427), GetNaturalSize(visible_rect, 9.0 / 16.0));

  // Test some random ratios.
  EXPECT_EQ(gfx::Size(495, 240), GetNaturalSize(visible_rect, 17.0 / 11.0));
  EXPECT_EQ(gfx::Size(320, 371), GetNaturalSize(visible_rect, 11.0 / 17.0));
}

TEST_F(VideoUtilTest, GetNaturalSize_Fraction) {
  gfx::Size visible_size(320, 240);

  // Test 0 sizes.
  EXPECT_EQ(gfx::Size(0, 0), GetNaturalSize(gfx::Size(0, 0), 1, 1));
  EXPECT_EQ(gfx::Size(0, 1), GetNaturalSize(gfx::Size(0, 1), 1, 1));
  EXPECT_EQ(gfx::Size(1, 0), GetNaturalSize(gfx::Size(1, 0), 1, 1));

  // Test abnormal ratios.
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_size, 0, 0));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_size, 0, 1));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_size, 1, 0));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_size, 1, -1));
  EXPECT_EQ(gfx::Size(), GetNaturalSize(visible_size, -1, 1));

  // Test normal sizes and ratios.
  EXPECT_EQ(gfx::Size(320, 240), GetNaturalSize(visible_size, 1, 1));
  EXPECT_EQ(gfx::Size(640, 240), GetNaturalSize(visible_size, 2, 1));
  EXPECT_EQ(gfx::Size(320, 480), GetNaturalSize(visible_size, 1, 2));
  EXPECT_EQ(gfx::Size(427, 240), GetNaturalSize(visible_size, 4, 3));
  EXPECT_EQ(gfx::Size(320, 320), GetNaturalSize(visible_size, 3, 4));
  EXPECT_EQ(gfx::Size(569, 240), GetNaturalSize(visible_size, 16, 9));
  EXPECT_EQ(gfx::Size(320, 427), GetNaturalSize(visible_size, 9, 16));

  // Test some random ratios.
  EXPECT_EQ(gfx::Size(495, 240), GetNaturalSize(visible_size, 17, 11));
  EXPECT_EQ(gfx::Size(320, 371), GetNaturalSize(visible_size, 11, 17));
}

namespace {

uint8_t src6x4[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};

// Target images, name pattern target_rotation_flipV_flipH.
uint8_t* target6x4_0_n_n = src6x4;

uint8_t target6x4_0_n_y[] = {5,  4,  3,  2,  1,  0,  11, 10, 9,  8,  7,  6,
                             17, 16, 15, 14, 13, 12, 23, 22, 21, 20, 19, 18};

uint8_t target6x4_0_y_n[] = {18, 19, 20, 21, 22, 23, 12, 13, 14, 15, 16, 17,
                             6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  4,  5};

uint8_t target6x4_0_y_y[] = {23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12,
                             11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};

uint8_t target6x4_90_n_n[] = {255, 19, 13, 7, 1, 255, 255, 20, 14, 8,  2, 255,
                              255, 21, 15, 9, 3, 255, 255, 22, 16, 10, 4, 255};

uint8_t target6x4_90_n_y[] = {255, 1, 7, 13, 19, 255, 255, 2, 8,  14, 20, 255,
                              255, 3, 9, 15, 21, 255, 255, 4, 10, 16, 22, 255};

uint8_t target6x4_90_y_n[] = {255, 22, 16, 10, 4, 255, 255, 21, 15, 9, 3, 255,
                              255, 20, 14, 8,  2, 255, 255, 19, 13, 7, 1, 255};

uint8_t target6x4_90_y_y[] = {255, 4, 10, 16, 22, 255, 255, 3, 9, 15, 21, 255,
                              255, 2, 8,  14, 20, 255, 255, 1, 7, 13, 19, 255};

uint8_t* target6x4_180_n_n = target6x4_0_y_y;
uint8_t* target6x4_180_n_y = target6x4_0_y_n;
uint8_t* target6x4_180_y_n = target6x4_0_n_y;
uint8_t* target6x4_180_y_y = target6x4_0_n_n;

uint8_t* target6x4_270_n_n = target6x4_90_y_y;
uint8_t* target6x4_270_n_y = target6x4_90_y_n;
uint8_t* target6x4_270_y_n = target6x4_90_n_y;
uint8_t* target6x4_270_y_y = target6x4_90_n_n;

uint8_t src4x6[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23};

uint8_t* target4x6_0_n_n = src4x6;

uint8_t target4x6_0_n_y[] = {3,  2,  1,  0,  7,  6,  5,  4,  11, 10, 9,  8,
                             15, 14, 13, 12, 19, 18, 17, 16, 23, 22, 21, 20};

uint8_t target4x6_0_y_n[] = {20, 21, 22, 23, 16, 17, 18, 19, 12, 13, 14, 15,
                             8,  9,  10, 11, 4,  5,  6,  7,  0,  1,  2,  3};

uint8_t target4x6_0_y_y[] = {23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12,
                             11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};

uint8_t target4x6_90_n_n[] = {255, 255, 255, 255, 16,  12,  8,   4,
                              17,  13,  9,   5,   18,  14,  10,  6,
                              19,  15,  11,  7,   255, 255, 255, 255};

uint8_t target4x6_90_n_y[] = {255, 255, 255, 255, 4,   8,   12,  16,
                              5,   9,   13,  17,  6,   10,  14,  18,
                              7,   11,  15,  19,  255, 255, 255, 255};

uint8_t target4x6_90_y_n[] = {255, 255, 255, 255, 19,  15,  11,  7,
                              18,  14,  10,  6,   17,  13,  9,   5,
                              16,  12,  8,   4,   255, 255, 255, 255};

uint8_t target4x6_90_y_y[] = {255, 255, 255, 255, 7,   11,  15,  19,
                              6,   10,  14,  18,  5,   9,   13,  17,
                              4,   8,   12,  16,  255, 255, 255, 255};

uint8_t* target4x6_180_n_n = target4x6_0_y_y;
uint8_t* target4x6_180_n_y = target4x6_0_y_n;
uint8_t* target4x6_180_y_n = target4x6_0_n_y;
uint8_t* target4x6_180_y_y = target4x6_0_n_n;

uint8_t* target4x6_270_n_n = target4x6_90_y_y;
uint8_t* target4x6_270_n_y = target4x6_90_y_n;
uint8_t* target4x6_270_y_n = target4x6_90_n_y;
uint8_t* target4x6_270_y_y = target4x6_90_n_n;

struct VideoRotationTestData {
  uint8_t* src;
  uint8_t* target;
  int width;
  int height;
  int rotation;
  bool flip_vert;
  bool flip_horiz;
};

const VideoRotationTestData kVideoRotationTestData[] = {
  { src6x4, target6x4_0_n_n, 6, 4, 0, false, false },
  { src6x4, target6x4_0_n_y, 6, 4, 0, false, true },
  { src6x4, target6x4_0_y_n, 6, 4, 0, true, false },
  { src6x4, target6x4_0_y_y, 6, 4, 0, true, true },

  { src6x4, target6x4_90_n_n, 6, 4, 90, false, false },
  { src6x4, target6x4_90_n_y, 6, 4, 90, false, true },
  { src6x4, target6x4_90_y_n, 6, 4, 90, true, false },
  { src6x4, target6x4_90_y_y, 6, 4, 90, true, true },

  { src6x4, target6x4_180_n_n, 6, 4, 180, false, false },
  { src6x4, target6x4_180_n_y, 6, 4, 180, false, true },
  { src6x4, target6x4_180_y_n, 6, 4, 180, true, false },
  { src6x4, target6x4_180_y_y, 6, 4, 180, true, true },

  { src6x4, target6x4_270_n_n, 6, 4, 270, false, false },
  { src6x4, target6x4_270_n_y, 6, 4, 270, false, true },
  { src6x4, target6x4_270_y_n, 6, 4, 270, true, false },
  { src6x4, target6x4_270_y_y, 6, 4, 270, true, true },

  { src4x6, target4x6_0_n_n, 4, 6, 0, false, false },
  { src4x6, target4x6_0_n_y, 4, 6, 0, false, true },
  { src4x6, target4x6_0_y_n, 4, 6, 0, true, false },
  { src4x6, target4x6_0_y_y, 4, 6, 0, true, true },

  { src4x6, target4x6_90_n_n, 4, 6, 90, false, false },
  { src4x6, target4x6_90_n_y, 4, 6, 90, false, true },
  { src4x6, target4x6_90_y_n, 4, 6, 90, true, false },
  { src4x6, target4x6_90_y_y, 4, 6, 90, true, true },

  { src4x6, target4x6_180_n_n, 4, 6, 180, false, false },
  { src4x6, target4x6_180_n_y, 4, 6, 180, false, true },
  { src4x6, target4x6_180_y_n, 4, 6, 180, true, false },
  { src4x6, target4x6_180_y_y, 4, 6, 180, true, true },

  { src4x6, target4x6_270_n_n, 4, 6, 270, false, false },
  { src4x6, target4x6_270_n_y, 4, 6, 270, false, true },
  { src4x6, target4x6_270_y_n, 4, 6, 270, true, false },
  { src4x6, target4x6_270_y_y, 4, 6, 270, true, true }
};

}  // namespace

class VideoUtilRotationTest
    : public testing::TestWithParam<VideoRotationTestData> {
 public:
  VideoUtilRotationTest() {
    dest_.reset(new uint8_t[GetParam().width * GetParam().height]);
  }

  virtual ~VideoUtilRotationTest() = default;

  uint8_t* dest_plane() { return dest_.get(); }

 private:
  std::unique_ptr<uint8_t[]> dest_;

  DISALLOW_COPY_AND_ASSIGN(VideoUtilRotationTest);
};

TEST_P(VideoUtilRotationTest, Rotate) {
  int rotation = GetParam().rotation;
  EXPECT_TRUE((rotation >= 0) && (rotation < 360) && (rotation % 90 == 0));

  int size = GetParam().width * GetParam().height;
  uint8_t* dest = dest_plane();
  memset(dest, 255, size);

  RotatePlaneByPixels(GetParam().src, dest, GetParam().width,
                      GetParam().height, rotation,
                      GetParam().flip_vert, GetParam().flip_horiz);

  EXPECT_EQ(memcmp(dest, GetParam().target, size), 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoUtilRotationTest,
                         testing::ValuesIn(kVideoRotationTestData));

// Tests the ComputeLetterboxRegion function.  Also, because of shared code
// internally, this also tests ScaleSizeToFitWithinTarget().
TEST_F(VideoUtilTest, ComputeLetterboxRegion) {
  EXPECT_EQ(gfx::Rect(166, 0, 667, 500),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 1000, 500),
                                   gfx::Size(640, 480)));
  EXPECT_EQ(gfx::Rect(0, 312, 500, 375),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 500, 1000),
                                   gfx::Size(640, 480)));
  EXPECT_EQ(gfx::Rect(55, 0, 889, 500),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 1000, 500),
                                   gfx::Size(1920, 1080)));
  EXPECT_EQ(gfx::Rect(0, 12, 100, 75),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 100, 100),
                                   gfx::Size(400, 300)));
  EXPECT_EQ(gfx::Rect(0, 250000000, 2000000000, 1500000000),
            ComputeLetterboxRegion(gfx::Rect(0, 0, 2000000000, 2000000000),
                                   gfx::Size(40000, 30000)));
  EXPECT_TRUE(ComputeLetterboxRegion(gfx::Rect(0, 0, 2000000000, 2000000000),
                                     gfx::Size(0, 0)).IsEmpty());

  // Some operations in the internal ScaleSizeToTarget() use rounded division
  // and might lose some precision, this expectation codifies that.
  EXPECT_EQ(
      gfx::Rect(0, 0, 1279, 720),
      ComputeLetterboxRegion(gfx::Rect(0, 0, 1280, 720), gfx::Size(1057, 595)));
}

// Tests the ComputeLetterboxRegionForI420 function.
TEST_F(VideoUtilTest, ComputeLetterboxRegionForI420) {
  // Note: These are the same trials as in VideoUtilTest.ComputeLetterboxRegion
  // above, except that Rect coordinates are nudged into even-numbered values.
  EXPECT_EQ(gfx::Rect(166, 0, 666, 500),
            ComputeLetterboxRegionForI420(gfx::Rect(0, 0, 1000, 500),
                                          gfx::Size(640, 480)));
  EXPECT_EQ(gfx::Rect(0, 312, 500, 374),
            ComputeLetterboxRegionForI420(gfx::Rect(0, 0, 500, 1000),
                                          gfx::Size(640, 480)));
  EXPECT_EQ(gfx::Rect(54, 0, 890, 500),
            ComputeLetterboxRegionForI420(gfx::Rect(0, 0, 1000, 500),
                                          gfx::Size(1920, 1080)));
  EXPECT_EQ(gfx::Rect(0, 12, 100, 74),
            ComputeLetterboxRegionForI420(gfx::Rect(0, 0, 100, 100),
                                          gfx::Size(400, 300)));
  EXPECT_EQ(
      gfx::Rect(0, 250000000, 2000000000, 1500000000),
      ComputeLetterboxRegionForI420(gfx::Rect(0, 0, 2000000000, 2000000000),
                                    gfx::Size(40000, 30000)));
  EXPECT_TRUE(ComputeLetterboxRegionForI420(
                  gfx::Rect(0, 0, 2000000000, 2000000000), gfx::Size(0, 0))
                  .IsEmpty());
}

TEST_F(VideoUtilTest, ScaleSizeToEncompassTarget) {
  EXPECT_EQ(gfx::Size(1000, 750),
            ScaleSizeToEncompassTarget(gfx::Size(640, 480),
                                       gfx::Size(1000, 500)));
  EXPECT_EQ(gfx::Size(1333, 1000),
            ScaleSizeToEncompassTarget(gfx::Size(640, 480),
                                       gfx::Size(500, 1000)));
  EXPECT_EQ(gfx::Size(1000, 563),
            ScaleSizeToEncompassTarget(gfx::Size(1920, 1080),
                                       gfx::Size(1000, 500)));
  EXPECT_EQ(gfx::Size(133, 100),
            ScaleSizeToEncompassTarget(gfx::Size(400, 300),
                                       gfx::Size(100, 100)));
  EXPECT_EQ(gfx::Size(266666667, 200000000),
            ScaleSizeToEncompassTarget(gfx::Size(40000, 30000),
                                       gfx::Size(200000000, 200000000)));
  EXPECT_TRUE(ScaleSizeToEncompassTarget(
      gfx::Size(0, 0), gfx::Size(2000000000, 2000000000)).IsEmpty());
}

TEST_F(VideoUtilTest, PadToMatchAspectRatio) {
  EXPECT_EQ(gfx::Size(640, 480),
            PadToMatchAspectRatio(gfx::Size(640, 480), gfx::Size(640, 480)));
  EXPECT_EQ(gfx::Size(640, 480),
            PadToMatchAspectRatio(gfx::Size(640, 480), gfx::Size(4, 3)));
  EXPECT_EQ(gfx::Size(960, 480),
            PadToMatchAspectRatio(gfx::Size(640, 480), gfx::Size(1000, 500)));
  EXPECT_EQ(gfx::Size(640, 1280),
            PadToMatchAspectRatio(gfx::Size(640, 480), gfx::Size(500, 1000)));
  EXPECT_EQ(gfx::Size(2160, 1080),
            PadToMatchAspectRatio(gfx::Size(1920, 1080), gfx::Size(1000, 500)));
  EXPECT_EQ(gfx::Size(400, 400),
            PadToMatchAspectRatio(gfx::Size(400, 300), gfx::Size(100, 100)));
  EXPECT_EQ(gfx::Size(400, 400),
            PadToMatchAspectRatio(gfx::Size(300, 400), gfx::Size(100, 100)));
  EXPECT_EQ(gfx::Size(40000, 40000),
            PadToMatchAspectRatio(gfx::Size(40000, 30000),
                                  gfx::Size(2000000000, 2000000000)));
  EXPECT_TRUE(PadToMatchAspectRatio(
      gfx::Size(40000, 30000), gfx::Size(0, 0)).IsEmpty());
}

TEST_F(VideoUtilTest, LetterboxVideoFrame) {
  int width = 40;
  int height = 30;
  gfx::Size size(width, height);
  scoped_refptr<VideoFrame> frame(VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, size, gfx::Rect(size), size, base::TimeDelta()));

  for (int left_margin = 0; left_margin <= 10; left_margin += 10) {
    for (int right_margin = 0; right_margin <= 10; right_margin += 10) {
      for (int top_margin = 0; top_margin <= 10; top_margin += 10) {
        for (int bottom_margin = 0; bottom_margin <= 10; bottom_margin += 10) {
          gfx::Rect view_area(left_margin, top_margin,
                              width - left_margin - right_margin,
                              height - top_margin - bottom_margin);
          FillYUV(frame.get(), 0x1, 0x2, 0x3);
          LetterboxVideoFrame(frame.get(), view_area);
          for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
              bool inside = x >= view_area.x() &&
                  x < view_area.x() + view_area.width() &&
                  y >= view_area.y() &&
                  y < view_area.y() + view_area.height();
              EXPECT_EQ(frame->data(VideoFrame::kYPlane)[
                  y * frame->stride(VideoFrame::kYPlane) + x],
                        inside ? 0x01 : 0x00);
              EXPECT_EQ(frame->data(VideoFrame::kUPlane)[
                  (y / 2) * frame->stride(VideoFrame::kUPlane) + (x / 2)],
                        inside ? 0x02 : 0x80);
              EXPECT_EQ(frame->data(VideoFrame::kVPlane)[
                  (y / 2) * frame->stride(VideoFrame::kVPlane) + (x / 2)],
                        inside ? 0x03 : 0x80);
            }
          }
        }
      }
    }
  }
}

TEST_F(VideoUtilTest, I420CopyWithPadding) {
  gfx::Size visible_size(40, 30);
  scoped_refptr<VideoFrame> src_frame = CreateFrameWithPatternFilled(
      PIXEL_FORMAT_I420, visible_size, gfx::Rect(visible_size), visible_size,
      base::TimeDelta());
  // Expect to return false when copying to an empty buffer.
  EXPECT_FALSE(I420CopyWithPadding(*src_frame, nullptr));

  scoped_refptr<VideoFrame> dst_frame = CreateFrameWithPatternFilled(
      PIXEL_FORMAT_I420, visible_size, gfx::Rect(visible_size), visible_size,
      base::TimeDelta());
  EXPECT_TRUE(I420CopyWithPadding(*src_frame, dst_frame.get()));
  EXPECT_TRUE(VerifyCopyWithPadding(*src_frame, *dst_frame));

  gfx::Size coded_size(60, 40);
  dst_frame = CreateFrameWithPatternFilled(PIXEL_FORMAT_I420, coded_size,
                                           gfx::Rect(visible_size), coded_size,
                                           base::TimeDelta());
  EXPECT_TRUE(I420CopyWithPadding(*src_frame, dst_frame.get()));
  EXPECT_TRUE(VerifyCopyWithPadding(*src_frame, *dst_frame));

  gfx::Size odd_size(39, 31);
  src_frame = CreateFrameWithPatternFilled(PIXEL_FORMAT_I420, odd_size,
                                           gfx::Rect(odd_size), odd_size,
                                           base::TimeDelta());
  dst_frame = CreateFrameWithPatternFilled(PIXEL_FORMAT_I420, coded_size,
                                           gfx::Rect(odd_size), coded_size,
                                           base::TimeDelta());
  EXPECT_TRUE(I420CopyWithPadding(*src_frame, dst_frame.get()));
  EXPECT_TRUE(VerifyCopyWithPadding(*src_frame, *dst_frame));
}

TEST_F(VideoUtilTest, WrapAsI420VideoFrame) {
  gfx::Size size(640, 480);
  scoped_refptr<VideoFrame> src_frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_I420A, size, gfx::Rect(size), size,
                              base::TimeDelta::FromDays(1));

  scoped_refptr<VideoFrame> dst_frame = WrapAsI420VideoFrame(src_frame);
  EXPECT_EQ(dst_frame->format(), PIXEL_FORMAT_I420);
  EXPECT_EQ(dst_frame->timestamp(), src_frame->timestamp());
  EXPECT_EQ(dst_frame->coded_size(), src_frame->coded_size());
  EXPECT_EQ(dst_frame->visible_rect(), src_frame->visible_rect());
  EXPECT_EQ(dst_frame->natural_size(), src_frame->natural_size());

  std::vector<size_t> planes = {VideoFrame::kYPlane, VideoFrame::kUPlane,
                                VideoFrame::kVPlane};
  for (auto plane : planes)
    EXPECT_EQ(dst_frame->data(plane), src_frame->data(plane));

  // Check that memory for planes is not released upon destruction of the
  // original frame pointer (new frame holds a reference). This check relies on
  // ASAN.
  src_frame.reset();
  for (auto plane : planes)
    memset(dst_frame->data(plane), 1, dst_frame->stride(plane));
}

}  // namespace media
