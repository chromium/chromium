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
    visibility_configuration_ =
        [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];

    toolbar_button_ = [[ToolbarButton alloc] initWithImageLoader:image_loader_];
    ForceLoadImage(toolbar_button_);

    toolbar_button_highlight_image_ = [[ToolbarButton alloc]
              initWithImageLoader:image_loader_
        IPHHighlightedImageLoader:iph_highlighted_image_loader_];
    ForceLoadImage(toolbar_button_highlight_image_);

    PlatformTest::SetUp();
  }

  void TearDown() override { PlatformTest::TearDown(); }

  // Workaround way to force the image to load from the image loader block.
  void ForceLoadImage(ToolbarButton* toolbar_button) {
    toolbar_button.visibilityMask =
        visibility_configuration_.toolsMenuButtonVisibility;
    [toolbar_button updateHiddenInCurrentSizeClass];
  }

 protected:
  ToolbarButton* toolbar_button_;
  ToolbarButton* toolbar_button_highlight_image_;

  ToolbarButtonVisibilityConfiguration* visibility_configuration_;

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

// Checks that setting a new image loader block updates the image, when the
// image was already loaded.
TEST_F(ToolbarButtonTest, SetImageLoader_LoadedImage) {
  // Prepare the other image loader block.
  UIImage* other_image =
      DefaultSymbolWithPointSize(kMailFillSymbol, kSymbolToolbarPointSize);
  __block bool called = false;
  auto other_image_loader = ^UIImage* {
    called = true;
    return other_image;
  };
  EXPECT_EQ(toolbar_button_.currentImage, image_loader_());
  EXPECT_NE(toolbar_button_.currentImage, other_image);
  EXPECT_FALSE(called);

  // Set the other image loader.
  [toolbar_button_ setImageLoader:other_image_loader];

  // Check that the other image loader has been called and `currentImage` is the
  // other image.
  EXPECT_TRUE(called);
  EXPECT_EQ(toolbar_button_.currentImage, other_image);
}

// Checks that setting a new image loader block doesn't update the image at
// first, but loads correctly the new image when requested.
TEST_F(ToolbarButtonTest, SetImageLoader_NotLoadedImage) {
  // Create a new button, whose image has not been loaded yet.
  UIImage* image =
      DefaultSymbolWithPointSize(kMenuSymbol, kSymbolToolbarPointSize);
  toolbar_button_ = [[ToolbarButton alloc] initWithImageLoader:^UIImage* {
    return image;
  }];
  // Prepare the other image loader block.
  UIImage* other_image =
      DefaultSymbolWithPointSize(kMailFillSymbol, kSymbolToolbarPointSize);
  __block bool called = false;
  auto other_image_loader = ^UIImage* {
    called = true;
    return other_image;
  };
  EXPECT_EQ(toolbar_button_.currentImage, nil);
  EXPECT_NE(toolbar_button_.currentImage, other_image);
  EXPECT_FALSE(called);

  // Set the other image loader.
  [toolbar_button_ setImageLoader:other_image_loader];

  // Check that the other image loader has not yet been called and
  // `currentImage` is still nil.
  EXPECT_FALSE(called);
  EXPECT_EQ(toolbar_button_.currentImage, nil);

  // Force load the image.
  ForceLoadImage(toolbar_button_);

  // Check that the other image loader has been called and `currentImage` is the
  // other image.
  EXPECT_TRUE(called);
  EXPECT_EQ(toolbar_button_.currentImage, other_image);
}
