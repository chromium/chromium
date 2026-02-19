// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_buttons_utils.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
constexpr CGFloat kShadowOpacity = 0.12;
constexpr CGFloat kShadowYOffset = 1;
}  // namespace

UIColor* ToolbarButtonColor() {
  return [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        if (traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
          return [UIColor colorNamed:kStaticGrey700Color];
        }
        return [UIColor colorNamed:kStaticGrey300Color];
      }];
}

UIColor* ToolbarLocationBarBackgroundColor(bool incognito) {
  if (incognito) {
    return [UIColor colorNamed:kStaticGrey900Color];
  }
  return ToolbarButtonColor();
}

void ConfigureShadowForToolbarButton(UIView* button) {
  button.layer.shadowColor = UIColor.whiteColor.CGColor;
  button.layer.shadowOpacity = kShadowOpacity;
  button.layer.shadowOffset = CGSizeMake(0, kShadowYOffset);
  button.layer.shadowRadius = 0;
}
