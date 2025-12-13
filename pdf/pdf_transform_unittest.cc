// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_transform.h"

#include "pdf/pdf_rect.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

namespace {

constexpr float kDefaultWidth = 8.5 * printing::kPointsPerInch;
constexpr float kDefaultHeight = 11.0 * printing::kPointsPerInch;
constexpr float kDefaultRatio = kDefaultWidth / kDefaultHeight;
constexpr float kTolerance = 0.0001f;

void ExpectDefaultPortraitBox(const PdfRect& box) {
  EXPECT_FLOAT_EQ(0, box.left());
  EXPECT_FLOAT_EQ(0, box.bottom());
  EXPECT_FLOAT_EQ(kDefaultWidth, box.right());
  EXPECT_FLOAT_EQ(kDefaultHeight, box.top());
}

void ExpectDefaultLandscapeBox(const PdfRect& box) {
  EXPECT_FLOAT_EQ(0, box.left());
  EXPECT_FLOAT_EQ(0, box.bottom());
  EXPECT_FLOAT_EQ(kDefaultHeight, box.right());
  EXPECT_FLOAT_EQ(kDefaultWidth, box.top());
}

void ExpectBoxesAreEqual(const PdfRect& expected, const PdfRect& actual) {
  EXPECT_FLOAT_EQ(expected.left(), actual.left());
  EXPECT_FLOAT_EQ(expected.bottom(), actual.bottom());
  EXPECT_FLOAT_EQ(expected.right(), actual.right());
  EXPECT_FLOAT_EQ(expected.top(), actual.top());
}

void InitializeBoxToInvalidValues(PdfRect* box) {
  *box = PdfRect(-1, -1, -1, -1);
}

void InitializeBoxToDefaultPortraitValues(PdfRect* box) {
  *box = PdfRect(0, 0, kDefaultWidth, kDefaultHeight);
}

void InitializeBoxToDefaultLandscapeValue(PdfRect* box) {
  *box = PdfRect(0, 0, kDefaultHeight, kDefaultWidth);
}

}  // namespace

TEST(PdfTransformTest, CalculateScaleFactor) {
  static constexpr gfx::SizeF kSize(kDefaultWidth, kDefaultHeight);
  gfx::Rect rect(kDefaultWidth, kDefaultHeight);
  float scale;

  // 1:1
  scale = CalculateScaleFactor(rect, kSize, false);
  EXPECT_NEAR(1.0f, scale, kTolerance);
  scale = CalculateScaleFactor(rect, kSize, true);
  EXPECT_NEAR(kDefaultRatio, scale, kTolerance);

  // 1:2
  rect = gfx::Rect(kDefaultWidth / 2, kDefaultHeight / 2);
  scale = CalculateScaleFactor(rect, kSize, false);
  EXPECT_NEAR(0.5f, scale, kTolerance);
  scale = CalculateScaleFactor(rect, kSize, true);
  EXPECT_NEAR(kDefaultRatio / 2, scale, kTolerance);

  // 3:1
  rect = gfx::Rect(kDefaultWidth * 3, kDefaultHeight * 3);
  scale = CalculateScaleFactor(rect, kSize, false);
  EXPECT_NEAR(3.0f, scale, kTolerance);
  scale = CalculateScaleFactor(rect, kSize, true);
  EXPECT_NEAR(kDefaultRatio * 3, scale, kTolerance);

  // 3:1, rotated.
  rect = gfx::Rect(kDefaultHeight * 3, kDefaultWidth * 3);
  scale = CalculateScaleFactor(rect, kSize, false);
  EXPECT_NEAR(kDefaultRatio * 3, scale, kTolerance);
  scale = CalculateScaleFactor(rect, kSize, true);
  EXPECT_NEAR(3.0f, scale, kTolerance);

  // Odd size
  rect = gfx::Rect(10, 1000);
  scale = CalculateScaleFactor(rect, kSize, false);
  EXPECT_NEAR(0.01634f, scale, kTolerance);
  scale = CalculateScaleFactor(rect, kSize, true);
  EXPECT_NEAR(0.01263f, scale, kTolerance);
}

TEST(PdfTransformTest, CalculateMediaBoxAndCropBox) {
  PdfRect media_box;
  PdfRect crop_box;

  // Assume both boxes are there.
  InitializeBoxToDefaultPortraitValues(&media_box);
  InitializeBoxToDefaultLandscapeValue(&crop_box);
  CalculateMediaBoxAndCropBox(true, true, true, &media_box, &crop_box);
  ExpectDefaultPortraitBox(media_box);
  ExpectDefaultLandscapeBox(crop_box);

  // Assume both boxes are missing.
  InitializeBoxToInvalidValues(&media_box);
  InitializeBoxToInvalidValues(&crop_box);
  CalculateMediaBoxAndCropBox(false, false, false, &media_box, &crop_box);
  ExpectDefaultPortraitBox(media_box);
  ExpectDefaultPortraitBox(crop_box);
  CalculateMediaBoxAndCropBox(true, false, false, &media_box, &crop_box);
  ExpectDefaultLandscapeBox(media_box);
  ExpectDefaultLandscapeBox(crop_box);

  // Assume crop box is missing.
  constexpr PdfRect kExpctedBox(0, 0, 42, 420);
  media_box = kExpctedBox;
  InitializeBoxToInvalidValues(&crop_box);
  CalculateMediaBoxAndCropBox(false, true, false, &media_box, &crop_box);
  ExpectBoxesAreEqual(kExpctedBox, media_box);
  ExpectBoxesAreEqual(kExpctedBox, crop_box);

  // Assume media box is missing.
  InitializeBoxToInvalidValues(&media_box);
  CalculateMediaBoxAndCropBox(false, false, true, &media_box, &crop_box);
  ExpectBoxesAreEqual(kExpctedBox, media_box);
  ExpectBoxesAreEqual(kExpctedBox, crop_box);
}

TEST(PdfTransformTest, CalculateClipBoxBoundary) {
  PdfRect media_box;
  PdfRect crop_box;
  PdfRect result;

  // media box and crop box are the same.
  InitializeBoxToDefaultPortraitValues(&media_box);
  InitializeBoxToDefaultPortraitValues(&crop_box);
  result = CalculateClipBoxBoundary(media_box, crop_box);
  ExpectDefaultPortraitBox(result);

  // media box is portrait and crop box is landscape.
  InitializeBoxToDefaultLandscapeValue(&crop_box);
  result = CalculateClipBoxBoundary(media_box, crop_box);
  EXPECT_FLOAT_EQ(0, result.left());
  EXPECT_FLOAT_EQ(0, result.bottom());
  EXPECT_FLOAT_EQ(kDefaultWidth, result.right());
  EXPECT_FLOAT_EQ(kDefaultWidth, result.top());

  // crop box is smaller than media box.
  crop_box = PdfRect(
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/100,
      /*top=*/200);
  result = CalculateClipBoxBoundary(media_box, crop_box);
  EXPECT_FLOAT_EQ(0, result.left());
  EXPECT_FLOAT_EQ(0, result.bottom());
  EXPECT_FLOAT_EQ(100, result.right());
  EXPECT_FLOAT_EQ(200, result.top());

  // crop box is smaller than the media box in one dimension and longer in the
  // other.
  crop_box = PdfRect(
      /*left=*/0,
      /*bottom=*/0,
      /*right=*/100,
      /*top=*/2000);
  result = CalculateClipBoxBoundary(media_box, crop_box);
  EXPECT_FLOAT_EQ(0, result.left());
  EXPECT_FLOAT_EQ(0, result.bottom());
  EXPECT_FLOAT_EQ(100, result.right());
  EXPECT_FLOAT_EQ(kDefaultHeight, result.top());
}

TEST(PdfTransformTest, CalculateScaledClipBoxOffset) {
  constexpr gfx::Rect rect(kDefaultWidth, kDefaultHeight);

  // `rect` and `clip_box` are the same size.
  PdfRect clip_box;
  InitializeBoxToDefaultPortraitValues(&clip_box);
  gfx::Vector2dF offset = CalculateScaledClipBoxOffset(rect, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());

  // `rect` is larger than `clip_box`.
  *clip_box.writable_top() /= 2;
  *clip_box.writable_right() /= 4;
  offset = CalculateScaledClipBoxOffset(rect, clip_box);
  EXPECT_FLOAT_EQ(229.5f, offset.x());
  EXPECT_FLOAT_EQ(198, offset.y());
}

TEST(PdfTransformTest, CalculateNonScaledClipBoxOffset) {
  int page_width = kDefaultWidth;
  int page_height = kDefaultHeight;

  // `rect`, page size and `clip_box` are the same.
  PdfRect clip_box;
  InitializeBoxToDefaultPortraitValues(&clip_box);
  gfx::Vector2dF offset =
      CalculateNonScaledClipBoxOffset(0, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(1, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(2, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(3, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(180, offset.x());
  EXPECT_FLOAT_EQ(-180, offset.y());

  // Smaller `clip_box`.
  *clip_box.writable_top() /= 4;
  *clip_box.writable_right() /= 2;
  offset =
      CalculateNonScaledClipBoxOffset(0, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(594, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(1, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(2, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(306, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(3, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(486, offset.x());
  EXPECT_FLOAT_EQ(414, offset.y());

  // Larger page size.
  InitializeBoxToDefaultPortraitValues(&clip_box);
  page_width += 10;
  page_height += 20;
  offset =
      CalculateNonScaledClipBoxOffset(0, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(20, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(1, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(2, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(10, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
  offset =
      CalculateNonScaledClipBoxOffset(3, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(200, offset.x());
  EXPECT_FLOAT_EQ(-170, offset.y());
}

TEST(PdfTransformTest, CalculateCenterClipBoxOffset) {
  constexpr int kPageWidth = kDefaultWidth + 10;
  constexpr int kPageHeight = kDefaultHeight + 20;

  // Centering the smaller `clip_box`.
  PdfRect clip_box;
  InitializeBoxToDefaultPortraitValues(&clip_box);
  gfx::Vector2dF offset = CalculateCenterClipBoxOffset(
      /*rotation=*/0, kPageWidth, kPageHeight, clip_box);
  EXPECT_FLOAT_EQ(5, offset.x());
  EXPECT_FLOAT_EQ(10, offset.y());
  offset = CalculateCenterClipBoxOffset(/*rotation=*/1, kPageHeight, kPageWidth,
                                        clip_box);
  EXPECT_FLOAT_EQ(5, offset.x());
  EXPECT_FLOAT_EQ(10, offset.y());
  offset = CalculateCenterClipBoxOffset(/*rotation=*/2, kPageWidth, kPageHeight,
                                        clip_box);
  EXPECT_FLOAT_EQ(5, offset.x());
  EXPECT_FLOAT_EQ(10, offset.y());
  offset = CalculateCenterClipBoxOffset(/*rotation=*/3, kPageHeight, kPageWidth,
                                        clip_box);
  EXPECT_FLOAT_EQ(5, offset.x());
  EXPECT_FLOAT_EQ(10, offset.y());
}

// https://crbug.com/491160 and https://crbug.com/588757
TEST(PdfTransformTest, ReversedMediaBox) {
  int page_width = kDefaultWidth;
  int page_height = kDefaultHeight;
  constexpr gfx::Rect rect(kDefaultWidth, kDefaultHeight);

  constexpr PdfRect expected_media_box_b491160 = {0, -792, 612, 0};
  PdfRect media_box_b491160 = {0, 0, 612, -792};
  PdfRect clip_box;
  CalculateMediaBoxAndCropBox(false, true, false, &media_box_b491160,
                              &clip_box);
  ExpectBoxesAreEqual(expected_media_box_b491160, media_box_b491160);
  ExpectBoxesAreEqual(expected_media_box_b491160, clip_box);

  gfx::Vector2dF offset = CalculateScaledClipBoxOffset(rect, media_box_b491160);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(792, offset.y());

  offset = CalculateNonScaledClipBoxOffset(0, page_width, page_height,
                                           media_box_b491160);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(792, offset.y());

  PdfRect media_box_b588757 = {0, 792, 612, 0};
  CalculateMediaBoxAndCropBox(false, true, false, &media_box_b588757,
                              &clip_box);
  ExpectDefaultPortraitBox(media_box_b588757);
  ExpectDefaultPortraitBox(clip_box);

  offset = CalculateScaledClipBoxOffset(rect, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());

  offset =
      CalculateNonScaledClipBoxOffset(0, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());

  PdfRect media_box_left_right_flipped = {612, 792, 0, 0};
  CalculateMediaBoxAndCropBox(false, true, false, &media_box_left_right_flipped,
                              &clip_box);
  ExpectDefaultPortraitBox(media_box_left_right_flipped);
  ExpectDefaultPortraitBox(clip_box);

  offset = CalculateScaledClipBoxOffset(rect, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());

  offset =
      CalculateNonScaledClipBoxOffset(0, page_width, page_height, clip_box);
  EXPECT_FLOAT_EQ(0, offset.x());
  EXPECT_FLOAT_EQ(0, offset.y());
}

}  // namespace chrome_pdf
