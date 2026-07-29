// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {
// Constants for default toolbar element shadow.
constexpr CGFloat kShadowOpacity = 0.12;
constexpr CGFloat kShadowYOffset = 1;

// Constants for glassToolbar background.
constexpr CGFloat kGlassDarkBackgroundAlpha = 0.5;
constexpr CGFloat kGlassLightBackgroundAlpha = 0.8;
constexpr CGFloat kGlassIncognitoBackgroundAlpha = 0.75;

// Constants for glassToolbar shadow.
constexpr CGFloat kGlassShadowRadius = 10;
constexpr CGFloat kGlassShadowOffsetY = 8;
constexpr CGFloat kGlassDarkShadowOpacity = 0.05;
constexpr CGFloat kGlassLightShadowOpacity = 0.16;
}  // namespace

UIColor* ToolbarElementBackgroundColor(BOOL incognito) {
  if (IsGlassToolbarEnabled()) {
    if (incognito) {
      return [UIColor colorWithWhite:0 alpha:kGlassIncognitoBackgroundAlpha];
    }
    return [UIColor
        colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
          if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
            return [UIColor colorWithWhite:0 alpha:kGlassDarkBackgroundAlpha];
          }
          return [UIColor colorWithWhite:1.0 alpha:kGlassLightBackgroundAlpha];
        }];
  }
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
  if (remove_shadow) {
    container.layer.shadowColor = nil;
    container.layer.shadowOpacity = 0.0;
    container.layer.shadowOffset = CGSizeZero;
    container.layer.shadowRadius = 0;
    return;
  }

  if (IsGlassToolbarEnabled()) {
    BOOL isDarkMode =
        container.overrideUserInterfaceStyle == UIUserInterfaceStyleDark ||
        container.traitCollection.userInterfaceStyle ==
            UIUserInterfaceStyleDark;
    container.layer.shadowColor =
        isDarkMode ? UIColor.whiteColor.CGColor : UIColor.blackColor.CGColor;
    container.layer.shadowOpacity =
        isDarkMode ? kGlassDarkShadowOpacity : kGlassLightShadowOpacity;
    container.layer.shadowOffset = CGSizeMake(0, kGlassShadowOffsetY);
    container.layer.shadowRadius = kGlassShadowRadius;
    return;
  }

  container.layer.shadowColor = UIColor.whiteColor.CGColor;
  container.layer.shadowOpacity = kShadowOpacity;
  container.layer.shadowOffset = CGSizeMake(0, kShadowYOffset);
  container.layer.shadowRadius = 0;
}

void ConfigureCornerRadiusForToolbarButtonContainer(
    UIView* container,
    UITraitCollection* trait_collection) {
  if (IsGlassToolbarEnabled()) {
    container.layer.cornerRadius = kToolbarButtonSize / 2;
    return;
  }
  // Whether the window has a regular height x compact width size class,
  // corresponding to iPhone portrait mode or a skinny iPad window.
  BOOL isRegularXCompactSizeClass =
      !IsCompactHeight(trait_collection) && IsCompactWidth(trait_collection);
  container.layer.cornerRadius = isRegularXCompactSizeClass
                                     ? kToolbarButtonSquareCornerRadius
                                     : kToolbarButtonSize / 2;
}
