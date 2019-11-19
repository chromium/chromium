// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarConfiguration

@synthesize style = _style;

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
  }
  return self;
}

- (UIColor*)NTPBackgroundColor {
  return color::DarkModeDynamicColor(ntp_home::kNTPBackgroundColor(),
                                     self.style == INCOGNITO,
                                     [UIColor colorNamed:kBackgroundDarkColor]);
}

- (UIColor*)backgroundColor {
  return color::DarkModeDynamicColor([UIColor colorNamed:kBackgroundColor],
                                     self.style == INCOGNITO,
                                     [UIColor colorNamed:kBackgroundDarkColor]);
}

- (UIColor*)buttonsTintColor {
  return color::DarkModeDynamicColor(
      [UIColor colorNamed:kToolbarButtonColor], self.style == INCOGNITO,
      [UIColor colorNamed:kToolbarButtonDarkColor]);
}

- (UIColor*)buttonsTintColorHighlighted {
  return color::DarkModeDynamicColor(
      [UIColor colorNamed:@"tab_toolbar_button_color_highlighted"],
      self.style == INCOGNITO,
      [UIColor colorNamed:@"tab_toolbar_button_color_highlighted_incognito"]);
}

- (UIColor*)buttonsSpotlightColor {
  return color::DarkModeDynamicColor(
      [UIColor colorNamed:@"tab_toolbar_button_halo_color"],
      self.style == INCOGNITO,
      [UIColor colorNamed:@"tab_toolbar_button_halo_color_incognito"]);
}

- (UIColor*)dimmedButtonsSpotlightColor {
  return color::DarkModeDynamicColor(
      [UIColor colorNamed:@"tab_toolbar_button_halo_color"],
      self.style == INCOGNITO,
      [UIColor colorNamed:@"tab_toolbar_button_halo_color_incognito"]);
}

- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor {
  // For the omnibox specifically, the background should be different in
  // incognito compared to dark mode.
  switch (self.style) {
    case NORMAL:
      return [[UIColor colorNamed:kTextfieldBackgroundColor]
          colorWithAlphaComponent:visibilityFactor];
    case INCOGNITO:
      return [[UIColor colorNamed:@"omnibox_incognito_background_color"]
          colorWithAlphaComponent:visibilityFactor];
  }
}

@end
