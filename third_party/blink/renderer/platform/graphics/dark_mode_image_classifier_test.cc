// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace blink {
namespace {

const float kEpsilon = 0.00001;

}  // namespace

class DarkModeImageClassifierTest : public testing::Test {
 public:
  DarkModeImageClassifierTest() {
    dark_mode_image_classifier_ = std::make_unique<DarkModeImageClassifier>(
        DarkModeImageClassifierPolicy::kNumColorsWithMlFallback);
  }

  // Loads the image from |file_name|.
  scoped_refptr<BitmapImage> GetImage(const String& file_name) {
    SCOPED_TRACE(file_name);
    String file_path = test::BlinkWebTestsDir() + file_name;
    std::optional<Vector<char>> data = test::ReadFromFile(file_path);
    CHECK(data && data->size());
    scoped_refptr<SharedBuffer> image_data =
        SharedBuffer::Create(std::move(*data));

    scoped_refptr<BitmapImage> image = BitmapImage::Create();
    image->SetData(image_data, true);
    return image;
  }

  DarkModeImageClassifier* image_classifier() {
    return dark_mode_image_classifier_.get();
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  std::unique_ptr<DarkModeImageClassifier> dark_mode_image_classifier_;
};

TEST_F(DarkModeImageClassifierTest, ValidImage) {
  scoped_refptr<BitmapImage> image;
  SkBitmap bitmap;
  SkPixmap pixmap;

  image = GetImage("/images/resources/twitter_favicon.ico");

  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);
  EXPECT_EQ(image_classifier()->Classify(
                pixmap, SkIRect::MakeWH(image->width(), image->height())),
            DarkModeResult::kApplyFilter);
}

TEST_F(DarkModeImageClassifierTest, InvalidImage) {
  scoped_refptr<BitmapImage> image;
  SkBitmap bitmap;
  SkPixmap pixmap;

  // Empty pixmap.
  SkIRect src = SkIRect::MakeWH(50, 50);
  EXPECT_EQ(image_classifier()->Classify(pixmap, src),
            DarkModeResult::kDoNotApplyFilter);

  // |src| larger than image size.
  image = GetImage("/images/resources/twitter_favicon.ico");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);
  EXPECT_EQ(
      image_classifier()->Classify(
          pixmap, SkIRect::MakeWH(image->width() + 10, image->height() + 10)),
      DarkModeResult::kDoNotApplyFilter);

  // Empty src rect.
  EXPECT_EQ(image_classifier()->Classify(pixmap, SkIRect()),
            DarkModeResult::kDoNotApplyFilter);
}

TEST_F(DarkModeImageClassifierTest, ImageSpriteAllFragmentsSame) {
  scoped_refptr<BitmapImage> image;
  SkBitmap bitmap;
  SkPixmap pixmap;
  image = GetImage("/images/resources/sprite_all_fragments_same.png");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 0, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 36, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 72, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 108, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 144, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 180, 95, 36)),
      DarkModeResult::kApplyFilter);
}

TEST_F(DarkModeImageClassifierTest, ImageSpriteAlternateFragmentsSame) {
  scoped_refptr<BitmapImage> image;
  SkBitmap bitmap;
  SkPixmap pixmap;
  image = GetImage("/images/resources/sprite_alternate_fragments_same.png");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 0, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 36, 95, 36)),
      DarkModeResult::kDoNotApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 72, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 108, 95, 36)),
      DarkModeResult::kDoNotApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 144, 95, 36)),
      DarkModeResult::kApplyFilter);

  EXPECT_EQ(
      image_classifier()->Classify(pixmap, SkIRect::MakeXYWH(0, 180, 95, 36)),
      DarkModeResult::kDoNotApplyFilter);
}

TEST_F(DarkModeImageClassifierTest, BlockSamples) {
  SkBitmap bitmap;
  SkPixmap pixmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(4, 4), 4 * 4);
  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  std::vector<SkColor> sampled_pixels;
  int transparent_pixels_count = -1;

  // All transparent.
  // ┌──────┐
  // │ AAAA │
  // │ AAAA │
  // │ AAAA │
  // │ AAAA │
  // └──────┘
  bitmap.eraseColor(SK_AlphaTRANSPARENT);
  bitmap.peekPixels(&pixmap);
  image_classifier()->GetBlockSamples(pixmap, SkIRect::MakeXYWH(0, 0, 4, 4), 16,
                                      &sampled_pixels,
                                      &transparent_pixels_count);
  EXPECT_EQ(sampled_pixels.size(), 0u);
  EXPECT_EQ(transparent_pixels_count, 16);

  // All pixels red.
  // ┌──────┐
  // │ RRRR │
  // │ RRRR │
  // │ RRRR │
  // │ RRRR │
  // └──────┘
  bitmap.eraseColor(SK_AlphaTRANSPARENT);
  paint.setColor(SK_ColorRED);
  canvas.drawIRect(SkIRect::MakeXYWH(0, 0, 4, 4), paint);
  bitmap.peekPixels(&pixmap);
  image_classifier()->GetBlockSamples(pixmap, SkIRect::MakeXYWH(0, 0, 4, 4), 16,
                                      &sampled_pixels,
                                      &transparent_pixels_count);
  EXPECT_EQ(sampled_pixels.size(), 16u);
  EXPECT_EQ(transparent_pixels_count, 0);
  for (auto color : sampled_pixels)
    EXPECT_EQ(color, SK_ColorRED);

  // Mixed.
  // ┌──────┐
  // │ RRGG │
  // │ RRGG │
  // │ BBAA │
  // │ BBAA │
  // └──────┘
  bitmap.eraseColor(SK_AlphaTRANSPARENT);
  paint.setColor(SK_ColorRED);
  canvas.drawIRect(SkIRect::MakeXYWH(0, 0, 2, 2), paint);
  paint.setColor(SK_ColorGREEN);
  canvas.drawIRect(SkIRect::MakeXYWH(2, 0, 2, 2), paint);
  paint.setColor(SK_ColorBLUE);
  canvas.drawIRect(SkIRect::MakeXYWH(0, 2, 2, 2), paint);
  bitmap.peekPixels(&pixmap);
  // Full block.
  image_classifier()->GetBlockSamples(pixmap, SkIRect::MakeXYWH(0, 0, 4, 4), 16,
                                      &sampled_pixels,
                                      &transparent_pixels_count);
  EXPECT_EQ(sampled_pixels.size(), 12u);
  EXPECT_EQ(transparent_pixels_count, 4);
  // Red block.
  image_classifier()->GetBlockSamples(pixmap, SkIRect::MakeXYWH(0, 0, 2, 2), 4,
                                      &sampled_pixels,
                                      &transparent_pixels_count);
  EXPECT_EQ(sampled_pixels.size(), 4u);
  EXPECT_EQ(transparent_pixels_count, 0);
  for (auto color : sampled_pixels)
    EXPECT_EQ(color, SK_ColorRED);
  // Green block.
  image_classifier()->GetBlockSamples(pixmap, SkIRect::MakeXYWH(2, 0, 2, 2), 4,
                                      &sampled_pixels,
                                      &transparent_pixels_count);
  EXPECT_EQ(sampled_pixels.size(), 4u);
  EXPECT_EQ(transparent_pixels_count, 0);
  for (auto color : sampled_pixels)
    EXPECT_EQ(color, SK_ColorGREEN);
  // Blue block.
  image_classifier()->GetBlockSamples(pixmap, SkIRect::MakeXYWH(0, 2, 2, 2), 4,
                                      &sampled_pixels,
                                      &transparent_pixels_count);
  EXPECT_EQ(sampled_pixels.size(), 4u);
  EXPECT_EQ(transparent_pixels_count, 0);
  for (auto color : sampled_pixels)
    EXPECT_EQ(color, SK_ColorBLUE);
  // Alpha block.
  image_classifier()->GetBlockSamples(pixmap, SkIRect::MakeXYWH(2, 2, 2, 2), 4,
                                      &sampled_pixels,
                                      &transparent_pixels_count);
  EXPECT_EQ(sampled_pixels.size(), 0u);
  EXPECT_EQ(transparent_pixels_count, 4);
}

TEST_F(DarkModeImageClassifierTest, FeaturesAndClassification) {
  DarkModeImageClassifier::Features features;
  scoped_refptr<BitmapImage> image;
  SkBitmap bitmap;
  SkPixmap pixmap;

  // Test Case 1:
  // Grayscale
  // Color Buckets Ratio: Low
  // Decision Tree: Apply
  // Neural Network: NA

  // The data members of DarkModeImageClassifier have to be reset for every
  // image as the same classifier object is used for all the tests.
  image = GetImage("/images/resources/grid-large.png");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);
  features = image_classifier()
                 ->GetFeatures(pixmap,
                               SkIRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeResult::kApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeResult::kApplyFilter);
  EXPECT_FALSE(features.is_colorful);
  EXPECT_NEAR(0.1875f, features.color_buckets_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.transparency_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.background_ratio, kEpsilon);

  // Test Case 2:
  // Grayscale
  // Color Buckets Ratio: Medium
  // Decision Tree: Can't Decide
  // Neural Network: Apply
  image = GetImage("/images/resources/apng08-ref.png");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);
  features = image_classifier()
                 ->GetFeatures(pixmap,
                               SkIRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeResult::kDoNotApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeResult::kNotClassified);
  EXPECT_FALSE(features.is_colorful);
  EXPECT_NEAR(0.8125f, features.color_buckets_ratio, kEpsilon);
  EXPECT_NEAR(0.446667f, features.transparency_ratio, kEpsilon);
  EXPECT_NEAR(0.03f, features.background_ratio, kEpsilon);

  // Test Case 3:
  // Color
  // Color Buckets Ratio: Low
  // Decision Tree: Apply
  // Neural Network: NA.
  image = GetImage("/images/resources/twitter_favicon.ico");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);
  features = image_classifier()
                 ->GetFeatures(pixmap,
                               SkIRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeResult::kApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeResult::kApplyFilter);
  EXPECT_TRUE(features.is_colorful);
  EXPECT_NEAR(0.0002441f, features.color_buckets_ratio, kEpsilon);
  EXPECT_NEAR(0.542092f, features.transparency_ratio, kEpsilon);
  EXPECT_NEAR(0.1500000f, features.background_ratio, kEpsilon);

  // Test Case 4:
  // Color
  // Color Buckets Ratio: High
  // Decision Tree: Do Not Apply
  // Neural Network: NA.
  image = GetImage("/images/resources/blue-wheel-srgb-color-profile.png");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);
  features = image_classifier()
                 ->GetFeatures(pixmap,
                               SkIRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeResult::kDoNotApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeResult::kDoNotApplyFilter);
  EXPECT_TRUE(features.is_colorful);
  EXPECT_NEAR(0.032959f, features.color_buckets_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.transparency_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.background_ratio, kEpsilon);

  // Test Case 5:
  // Color
  // Color Buckets Ratio: Medium
  // Decision Tree: Apply
  // Neural Network: NA.
  image = GetImage("/images/resources/ycbcr-444-float.jpg");
  bitmap = image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
  bitmap.peekPixels(&pixmap);
  features = image_classifier()
                 ->GetFeatures(pixmap,
                               SkIRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeResult::kApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeResult::kApplyFilter);
  EXPECT_TRUE(features.is_colorful);
  EXPECT_NEAR(0.0151367f, features.color_buckets_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.transparency_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.background_ratio, kEpsilon);
}

}  // namespace blink
