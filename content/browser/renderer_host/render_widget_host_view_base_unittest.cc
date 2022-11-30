// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_util.h"

namespace content {

namespace {

display::Display CreateDisplay(int width,
                               int height,
                               display::Display::Rotation rotation) {
  display::Display display;
  display.set_panel_rotation(rotation);
  display.set_bounds(gfx::Rect(width, height));

  return display;
}

} // anonymous namespace

TEST(RenderWidgetHostViewBaseTest, OrientationTypeForMobile) {
  // Square display (width == height).
  {
    display::Display display =
        CreateDisplay(100, 100, display::Display::ROTATE_0);
    EXPECT_EQ(display::mojom::ScreenOrientation::kPortraitPrimary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(200, 200, display::Display::ROTATE_90);
    EXPECT_EQ(display::mojom::ScreenOrientation::kLandscapePrimary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(0, 0, display::Display::ROTATE_180);
    EXPECT_EQ(display::mojom::ScreenOrientation::kPortraitSecondary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(10000, 10000, display::Display::ROTATE_270);
    EXPECT_EQ(display::mojom::ScreenOrientation::kLandscapeSecondary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));
  }

  // natural width > natural height.
  {
    display::Display display = CreateDisplay(1, 0, display::Display::ROTATE_0);
    EXPECT_EQ(display::mojom::ScreenOrientation::kLandscapePrimary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(19999, 20000, display::Display::ROTATE_90);
    EXPECT_EQ(display::mojom::ScreenOrientation::kPortraitSecondary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(200, 100, display::Display::ROTATE_180);
    EXPECT_EQ(display::mojom::ScreenOrientation::kLandscapeSecondary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(1, 10000, display::Display::ROTATE_270);
    EXPECT_EQ(display::mojom::ScreenOrientation::kPortraitPrimary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));
  }

  // natural width < natural height.
  {
    display::Display display = CreateDisplay(0, 1, display::Display::ROTATE_0);
    EXPECT_EQ(display::mojom::ScreenOrientation::kPortraitPrimary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(20000, 19999, display::Display::ROTATE_90);
    EXPECT_EQ(display::mojom::ScreenOrientation::kLandscapePrimary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(100, 200, display::Display::ROTATE_180);
    EXPECT_EQ(display::mojom::ScreenOrientation::kPortraitSecondary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));

    display = CreateDisplay(10000, 1, display::Display::ROTATE_270);
    EXPECT_EQ(display::mojom::ScreenOrientation::kLandscapeSecondary,
              display::DisplayUtil::GetOrientationTypeForMobile(display));
  }
}

TEST(RenderWidgetHostViewBaseTest, OrientationTypeForDesktop) {
  // On Desktop, the primary orientation is the first computed one so a test
  // similar to OrientationTypeForMobile is not possible.
  // Instead this test will only check one configuration and verify that the
  // method reports two landscape and two portrait orientations with one primary
  // and one secondary for each.

  // natural width > natural height.
  {
    display::Display display = CreateDisplay(1, 0, display::Display::ROTATE_0);
    display::mojom::ScreenOrientation landscape_1 =
        display::DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(
        landscape_1 == display::mojom::ScreenOrientation::kLandscapePrimary ||
        landscape_1 == display::mojom::ScreenOrientation::kLandscapeSecondary);

    display = CreateDisplay(200, 100, display::Display::ROTATE_180);
    display::mojom::ScreenOrientation landscape_2 =
        display::DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(
        landscape_2 == display::mojom::ScreenOrientation::kLandscapePrimary ||
        landscape_2 == display::mojom::ScreenOrientation::kLandscapeSecondary);

    EXPECT_NE(landscape_1, landscape_2);

    display = CreateDisplay(19999, 20000, display::Display::ROTATE_90);
    display::mojom::ScreenOrientation portrait_1 =
        display::DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(
        portrait_1 == display::mojom::ScreenOrientation::kPortraitPrimary ||
        portrait_1 == display::mojom::ScreenOrientation::kPortraitSecondary);

    display = CreateDisplay(1, 10000, display::Display::ROTATE_270);
    display::mojom::ScreenOrientation portrait_2 =
        display::DisplayUtil::GetOrientationTypeForDesktop(display);
    EXPECT_TRUE(
        portrait_2 == display::mojom::ScreenOrientation::kPortraitPrimary ||
        portrait_2 == display::mojom::ScreenOrientation::kPortraitSecondary);

    EXPECT_NE(portrait_1, portrait_2);

  }
}

} // namespace content
