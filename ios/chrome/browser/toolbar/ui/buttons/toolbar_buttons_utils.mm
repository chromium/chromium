// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
constexpr CGFloat kShadowOpacity = 0.12;
constexpr CGFloat kShadowYOffset = 1;
}  // namespace

UIColor* ToolbarElementBackgroundColor(BOOL incognito) {
  if (incognito) {
    return [UIColor colorNamed:kStaticGrey900Color];
  }
  return [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
          return [UIColor colorNamed:kStaticGrey700Color];
        }
        return [UIColor colorNamed:kStaticGrey300Color];
      }];
}

void ConfigureShadowForToolbarElement(UIView* container, BOOL remove_shadow) {
  container.layer.shadowColor =
      remove_shadow ? nil : UIColor.whiteColor.CGColor;
  container.layer.shadowOpacity = remove_shadow ? 0.0 : kShadowOpacity;
  container.layer.shadowOffset =
      remove_shadow ? CGSizeZero : CGSizeMake(0, kShadowYOffset);
  container.layer.shadowRadius = 0;
}

void ConfigureCornerRadiusForToolbarButtonContainer(
    UIView* container,
    UITraitCollection* trait_collection) {
  // Whether the window has a regular height x compact width size class,
  // corresponding to iPhone portrait mode or a skinny iPad window.
  BOOL isRegularXCompactSizeClass =
      !IsCompactHeight(trait_collection) && IsCompactWidth(trait_collection);
  container.layer.cornerRadius = isRegularXCompactSizeClass
                                     ? kToolbarButtonSquareCornerRadius
                                     : kToolbarButtonSize / 2;
}
