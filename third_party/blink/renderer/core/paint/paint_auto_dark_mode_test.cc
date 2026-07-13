// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class PaintAutoDarkModeTest : public testing::Test {
 public:
  void TestApplyFilterToImageIrrespectiveOfPageZoom(
      display::ScreenInfo screen_info) {
    DarkModeSettings settings;
    DarkModeFilter filter(settings);

    float page_zoom = 1.0f;
    float layout_zoom = 1.0f;
    float css_zoom = 1.0f;
    gfx::RectF src_rect;
    gfx::RectF dest_rect;

    // A 50x50 CSS icon gets filtered even if |dest_rect| becomes larger 250x250
    // than threshold size in larger zoom levels.
    src_rect = gfx::RectF(50, 50);
    page_zoom = 5.0f;
    css_zoom = 1.0f;
    layout_zoom = page_zoom * screen_info.device_scale_factor;
    dest_rect =
        gfx::RectF(50 * layout_zoom * css_zoom, 50 * layout_zoom * css_zoom);
    EXPECT_TRUE(filter.ShouldApplyFilterToImage(
        ImageClassifierHelper::GetImageTypeForTesting(dest_rect, src_rect,
                                                      layout_zoom)));

    // A 50x50 CSS icon with css zoom 5.0f becomes 250x250 and does not get
    // filterred as |dest_rect| is larger than threshold size.
    src_rect = gfx::RectF(50, 50);
    page_zoom = 5.0f;
    css_zoom = 5.0f;
    layout_zoom = page_zoom * screen_info.device_scale_factor;
    dest_rect =
        gfx::RectF(50 * layout_zoom * css_zoom, 50 * layout_zoom * css_zoom);
    EXPECT_FALSE(filter.ShouldApplyFilterToImage(
        ImageClassifierHelper::GetImageTypeForTesting(dest_rect, src_rect,
                                                      layout_zoom)));

    // An image with 200x200 CSS size gets classified as photo and does not get
    // filtered, even if |dest_rect| becomes smaller 50x50 than threshold size
    // in smaller zoom levels.
    src_rect = gfx::RectF(200, 200);
    page_zoom = 0.25f;
    css_zoom = 1.0f;
    layout_zoom = page_zoom * screen_info.device_scale_factor;
    dest_rect =
        gfx::RectF(200 * layout_zoom * css_zoom, 200 * layout_zoom * css_zoom);
    EXPECT_FALSE(filter.ShouldApplyFilterToImage(
        ImageClassifierHelper::GetImageTypeForTesting(dest_rect, src_rect,
                                                      layout_zoom)));

    // An image with 200x200 CSS size becomes 20x20 CSS size and gets classified
    // as icon as the CSS size is below the threshold.
    src_rect = gfx::RectF(200, 200);
    page_zoom = 0.25f;
    css_zoom = 0.1f;
    layout_zoom = page_zoom * screen_info.device_scale_factor;
    dest_rect =
        gfx::RectF(200 * layout_zoom * css_zoom, 200 * layout_zoom * css_zoom);
    EXPECT_TRUE(filter.ShouldApplyFilterToImage(
        ImageClassifierHelper::GetImageTypeForTesting(dest_rect, src_rect,
                                                      layout_zoom)));
  }

  DarkModeFilter::ImageType GetSVGDocumentType(float layout_zoom,
                                               const gfx::Rect& size) {
    LocalFrame& frame = page_holder_->GetFrame();
    frame.SetLayoutZoomFactor(layout_zoom);
    return ImageClassifierHelper::GetSVGDocumentType(frame, size);
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_ =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
};

TEST_F(PaintAutoDarkModeTest, ShouldApplyFilterToImage) {
  DarkModeSettings settings;
  DarkModeFilter filter(settings);

  // |dst| is smaller than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(gfx::RectF(50, 50),
                                                    gfx::RectF(50, 50))));

  // |dst| is smaller than threshold size, even |src| is larger.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(gfx::RectF(50, 50),
                                                    gfx::RectF(200, 200))));

  // |dst| is smaller than threshold size, |src| is smaller.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(gfx::RectF(50, 50),
                                                    gfx::RectF(20, 20))));

  // |src| having very smaller width, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(gfx::RectF(200, 5),
                                                    gfx::RectF(200, 5))));

  // |src| having very smaller height, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(gfx::RectF(5, 200),
                                                    gfx::RectF(5, 200))));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(gfx::RectF(200, 200),
                                                    gfx::RectF(20, 20))));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(gfx::RectF(20, 200),
                                                    gfx::RectF(20, 200))));
}

// Test for mobile display configuration
TEST_F(PaintAutoDarkModeTest, ShouldApplyFilterToImageOnMobile) {
  DarkModeSettings settings;
  DarkModeFilter filter(settings);

  display::ScreenInfo screen_info;
  screen_info.rect = gfx::Rect(360, 780);
  screen_info.device_scale_factor = 3.0f;
  const float layout_zoom = screen_info.device_scale_factor;

  // 44x44 CSS icon (132x132 device pixels) is below the threshold and filtered
  // after undoing the layout zoom (DSF).
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          gfx::RectF(132, 132), gfx::RectF(132, 132), layout_zoom)));

  // 70x70 CSS image (210x210 device pixels) is above the threshold and not
  // filtered.
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          gfx::RectF(210, 210), gfx::RectF(210, 210), layout_zoom)));
}

TEST_F(PaintAutoDarkModeTest, ShouldApplyFilterToImageIrrespectiveOfPageZoom) {
  display::ScreenInfo screen_info;
  screen_info.rect = gfx::Rect(1920, 1080);
  screen_info.device_scale_factor = 1.0f;

  TestApplyFilterToImageIrrespectiveOfPageZoom(screen_info);
}

TEST_F(PaintAutoDarkModeTest,
       ShouldApplyFilterToImageIrrespectiveOfPageZoomOnMobile) {
  display::ScreenInfo screen_info;
  screen_info.rect = gfx::Rect(360, 780);
  screen_info.device_scale_factor = 3.0f;

  TestApplyFilterToImageIrrespectiveOfPageZoom(screen_info);
}

TEST_F(PaintAutoDarkModeTest, SVGDocumentImage) {
  // Both dimensions are at or below the icon threshold (kMaxImageLength == 64).
  EXPECT_EQ(DarkModeFilter::ImageType::kIcon,
            GetSVGDocumentType(1.0f, gfx::Rect(50, 50)));

  // Either dimension above the threshold classifies the document as a photo.
  EXPECT_EQ(DarkModeFilter::ImageType::kPhoto,
            GetSVGDocumentType(1.0f, gfx::Rect(200, 200)));
  // Only the width exceeds the threshold.
  EXPECT_EQ(DarkModeFilter::ImageType::kPhoto,
            GetSVGDocumentType(1.0f, gfx::Rect(100, 50)));
  // Only the height exceeds the threshold.
  EXPECT_EQ(DarkModeFilter::ImageType::kPhoto,
            GetSVGDocumentType(1.0f, gfx::Rect(50, 100)));

  // A 40x40 CSS-sized SVG scaled up by a 5x layout zoom is still an icon after
  // the zoom is undone.
  EXPECT_EQ(DarkModeFilter::ImageType::kIcon,
            GetSVGDocumentType(5.0f, gfx::Rect(200, 200)));

  // A 400x400 CSS-sized SVG scaled down by a 0.25x layout zoom is still a photo
  // after the zoom is undone.
  EXPECT_EQ(DarkModeFilter::ImageType::kPhoto,
            GetSVGDocumentType(0.25f, gfx::Rect(100, 100)));
}

}  // namespace blink
