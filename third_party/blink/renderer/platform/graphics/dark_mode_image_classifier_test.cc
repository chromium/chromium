// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {
namespace {

const float kEpsilon = 0.00001;

}  // namespace

class DarkModeImageClassifierTest : public testing::Test {
 public:
  DarkModeImageClassifierTest() {
    dark_mode_image_classifier_ = std::make_unique<DarkModeImageClassifier>();
  }

  // Loads the image from |file_name|.
  scoped_refptr<BitmapImage> GetImage(const String& file_name) {
    SCOPED_TRACE(file_name);
    String file_path = test::BlinkWebTestsDir() + file_name;
    scoped_refptr<SharedBuffer> image_data = test::ReadFromFile(file_path);
    EXPECT_TRUE(image_data.get() && image_data.get()->size());

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

TEST_F(DarkModeImageClassifierTest, FeaturesAndClassification) {
  DarkModeImageClassifier::Features features;
  scoped_refptr<BitmapImage> image;

  // Test Case 1:
  // Grayscale
  // Color Buckets Ratio: Low
  // Decision Tree: Apply
  // Neural Network: NA

  // The data members of DarkModeImageClassifier have to be reset for every
  // image as the same classifier object is used for all the tests.
  image = GetImage("/images/resources/grid-large.png");
  features = image_classifier()
                 ->GetFeatures(image->PaintImageForCurrentFrame(),
                               SkRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeClassification::kApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeClassification::kApplyFilter);
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
  features = image_classifier()
                 ->GetFeatures(image->PaintImageForCurrentFrame(),
                               SkRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeClassification::kDoNotApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeClassification::kNotClassified);
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
  features = image_classifier()
                 ->GetFeatures(image->PaintImageForCurrentFrame(),
                               SkRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeClassification::kApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeClassification::kApplyFilter);
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
  features = image_classifier()
                 ->GetFeatures(image->PaintImageForCurrentFrame(),
                               SkRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeClassification::kDoNotApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeClassification::kDoNotApplyFilter);
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
  features = image_classifier()
                 ->GetFeatures(image->PaintImageForCurrentFrame(),
                               SkRect::MakeWH(image->width(), image->height()))
                 .value();
  EXPECT_EQ(image_classifier()->ClassifyWithFeatures(features),
            DarkModeClassification::kApplyFilter);
  EXPECT_EQ(image_classifier()->ClassifyUsingDecisionTree(features),
            DarkModeClassification::kApplyFilter);
  EXPECT_TRUE(features.is_colorful);
  EXPECT_NEAR(0.0151367f, features.color_buckets_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.transparency_ratio, kEpsilon);
  EXPECT_NEAR(0.0f, features.background_ratio, kEpsilon);
}

TEST_F(DarkModeImageClassifierTest, InvalidImage) {
  PaintImage paint_image;
  SkRect src = SkRect::MakeWH(50, 50);
  SkRect dst = SkRect::MakeWH(50, 50);
  EXPECT_EQ(image_classifier()->Classify(paint_image, src, dst),
            DarkModeClassification::kDoNotApplyFilter);
}

TEST_F(DarkModeImageClassifierTest, Caching) {
  PaintImage::Id image_id = PaintImage::GetNextId();
  SkRect src1 = SkRect::MakeXYWH(0, 0, 50, 50);
  SkRect src2 = SkRect::MakeXYWH(5, 20, 100, 100);
  SkRect src3 = SkRect::MakeXYWH(6, -9, 50, 50);

  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src1),
            DarkModeClassification::kNotClassified);
  image_classifier()->AddCacheValue(image_id, src1,
                                    DarkModeClassification::kApplyFilter);
  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src1),
            DarkModeClassification::kApplyFilter);

  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src2),
            DarkModeClassification::kNotClassified);
  image_classifier()->AddCacheValue(image_id, src2,
                                    DarkModeClassification::kDoNotApplyFilter);
  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src2),
            DarkModeClassification::kDoNotApplyFilter);

  EXPECT_EQ(image_classifier()->GetCacheSize(image_id), 2u);
  DarkModeImageClassifier::RemoveCache(image_id);
  EXPECT_EQ(image_classifier()->GetCacheSize(image_id), 0u);

  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src1),
            DarkModeClassification::kNotClassified);
  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src2),
            DarkModeClassification::kNotClassified);
  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src3),
            DarkModeClassification::kNotClassified);
  image_classifier()->AddCacheValue(image_id, src3,
                                    DarkModeClassification::kApplyFilter);
  EXPECT_EQ(image_classifier()->GetCacheValue(image_id, src3),
            DarkModeClassification::kApplyFilter);

  EXPECT_EQ(image_classifier()->GetCacheSize(image_id), 1u);
}

}  // namespace blink
