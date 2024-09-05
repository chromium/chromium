// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/toolbar/buttons/buttons_constants.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_type.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

namespace {

UIView* FindSubViewByID(UIView* view, NSString* accessibility_id) {
  if (view.accessibilityIdentifier == accessibility_id) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* found_view = FindSubViewByID(subview, accessibility_id);
    if (found_view) {
      return found_view;
    }
  }
  return nil;
}

// The size of the symbol image.
const CGFloat kSymbolToolbarPointSize = 24;

}  // namespace

class ToolbarButtonTest : public PlatformTest {
 public:
  void SetUp() override {
    image_loader_ = ^UIImage* {
      return DefaultSymbolWithPointSize(kMenuSymbol, kSymbolToolbarPointSize);
    };
    iph_highlighted_image_loader_ = ^UIImage* {
      return DefaultSymbolWithPointSize(kBackSymbol, kSymbolToolbarPointSize);
    };

    ToolbarType type;
    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      type = ToolbarType::kPrimary;
    } else {
      type = ToolbarType::kSecondary;
    }
    ToolbarButtonVisibilityConfiguration* visibility_configuration =
        [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];

    toolbar_button_ = [[ToolbarButton alloc] initWithImageLoader:image_loader_];
    toolbar_button_.visibilityMask =
        visibility_configuration.toolsMenuButtonVisibility;
    [toolbar_button_ updateHiddenInCurrentSizeClass];

    toolbar_button_highlight_image_ = [[ToolbarButton alloc]
              initWithImageLoader:image_loader_
        IPHHighlightedImageLoader:iph_highlighted_image_loader_];
    toolbar_button_highlight_image_.visibilityMask =
        visibility_configuration.toolsMenuButtonVisibility;
    [toolbar_button_highlight_image_ updateHiddenInCurrentSizeClass];

    PlatformTest::SetUp();
  }

  void TearDown() override { PlatformTest::TearDown(); }

 protected:
  ToolbarButton* toolbar_button_;
  ToolbarButton* toolbar_button_highlight_image_;

  ToolbarButtonImageLoader image_loader_;
  ToolbarButtonImageLoader iph_highlighted_image_loader_;
};

// Checks that setting IPH highlight property on a button that doesn't have
// highlighted image will do nothing.
TEST_F(ToolbarButtonTest, ToolbarButtonCorrectImageWithNoHighlight) {
  EXPECT_EQ(toolbar_button_.currentImage, image_loader_());

  toolbar_button_.iphHighlighted = YES;
  EXPECT_EQ(toolbar_button_.currentImage, image_loader_());
}

// Checks that setting IPH highlight property on a button that has highlighted
// image will change the button image.
TEST_F(ToolbarButtonTest, ToolbarButtonCorrectImageWithHighlight) {
  EXPECT_EQ(toolbar_button_highlight_image_.currentImage, image_loader_());

  toolbar_button_highlight_image_.iphHighlighted = YES;
  EXPECT_EQ(toolbar_button_highlight_image_.currentImage,
            iph_highlighted_image_loader_());
}

// Checks that setting blue dot property correctly adds or removes the blue dot
// subview.
TEST_F(ToolbarButtonTest, ToolbarButtonBlueDot) {
  EXPECT_FALSE(toolbar_button_.hasBlueDot);
  EXPECT_EQ(FindSubViewByID(toolbar_button_, kToolbarButtonBlueDotViewID), nil);

  toolbar_button_.hasBlueDot = YES;
  EXPECT_NE(FindSubViewByID(toolbar_button_, kToolbarButtonBlueDotViewID), nil);

  toolbar_button_.hasBlueDot = NO;
  EXPECT_EQ(FindSubViewByID(toolbar_button_, kToolbarButtonBlueDotViewID), nil);
}
