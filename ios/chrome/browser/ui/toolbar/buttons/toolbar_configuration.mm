// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation ToolbarConfiguration

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
  }
  return self;
}

- (UIColor*)NTPBackgroundColor {
  return ntp_home::NTPBackgroundColor();
}

- (UIColor*)backgroundColor {
  return [UIColor colorNamed:kBackgroundColor];
}

- (UIColor*)focusedBackgroundColor {
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

- (UIColor*)focusedLocationBarBackgroundColor {
  return [UIColor colorNamed:kTextfieldFocusedBackgroundColor];
}

- (UIColor*)buttonsTintColor {
  return [UIColor colorNamed:kToolbarButtonColor];
}

- (UIColor*)buttonsTintColorHighlighted {
  return [UIColor colorNamed:@"tab_toolbar_button_color_highlighted"];
}

- (UIColor*)buttonsTintColorIPHHighlighted {
  return [UIColor colorNamed:kSolidButtonTextColor];
}

- (UIColor*)buttonsIPHHighlightColor {
  return [UIColor colorNamed:kBlueColor];
}

- (UIColor*)locationBarBackgroundColorWithVisibility:(CGFloat)visibilityFactor {
  // For the omnibox specifically, the background should be different in
  // incognito compared to dark mode.
  switch (self.style) {
    case ToolbarStyle::kNormal:
      return [[UIColor colorNamed:kTextfieldBackgroundColor]
          colorWithAlphaComponent:visibilityFactor];
    case ToolbarStyle::kIncognito:
      return [[UIColor colorNamed:@"omnibox_incognito_background_color"]
          colorWithAlphaComponent:visibilityFactor];
  }
}

- (NSString*)accessibilityLabelForOpenNewTabButtonInGroup:(BOOL)inGroup {
  switch (self.style) {
    case ToolbarStyle::kNormal:
      return l10n_util::GetNSString(inGroup
                                        ? IDS_IOS_TOOLBAR_OPEN_NEW_TAB_IN_GROUP
                                        : IDS_IOS_TOOLBAR_OPEN_NEW_TAB);
    case ToolbarStyle::kIncognito:
      return l10n_util::GetNSString(
          inGroup ? IDS_IOS_TOOLBAR_OPEN_NEW_TAB_INCOGNITO_IN_GROUP
                  : IDS_IOS_TOOLBAR_OPEN_NEW_TAB_INCOGNITO);
  }
}

@end
