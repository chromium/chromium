// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_configuration.h"

#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation ToolbarConfiguration {
  // Location bar background colors, resolved once because fullscreen progress
  // updates request them on every scroll frame.
  UIColor* _locationBarBackgroundColor;
  UIColor* _hiddenLocationBarBackgroundColor;
}

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
    // For the omnibox specifically, the background should be different in
    // incognito compared to dark mode.
    switch (style) {
      case ToolbarStyle::kNormal:
        _locationBarBackgroundColor =
            [UIColor colorNamed:kTextfieldBackgroundColor];
        break;
      case ToolbarStyle::kIncognito:
        _locationBarBackgroundColor = [UIColor colorNamed:kStaticGrey900Color];
        break;
    }
    _hiddenLocationBarBackgroundColor =
        [_locationBarBackgroundColor colorWithAlphaComponent:0.0];
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
  if (visibilityFactor <= 0.0) {
    return _hiddenLocationBarBackgroundColor;
  }
  if (visibilityFactor >= 1.0) {
    return _locationBarBackgroundColor;
  }
  return [_locationBarBackgroundColor colorWithAlphaComponent:visibilityFactor];
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

- (NSString*)accessibilityLabelForCollapsedPrimaryToolbarButton {
  return l10n_util::GetNSString(IDS_IOS_COLLAPSED_PRIMARY_TOOLBAR_BUTTON);
}

- (NSString*)accessibilityLabelForCollapsedSecondaryToolbarButton {
  return l10n_util::GetNSString(IDS_IOS_COLLAPSED_SECONDARY_TOOLBAR_BUTTON);
}

@end
