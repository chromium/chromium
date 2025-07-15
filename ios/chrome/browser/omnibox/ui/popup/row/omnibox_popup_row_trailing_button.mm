// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_trailing_button.h"

#import "base/check.h"
#import "ios/chrome/browser/omnibox/public/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
/// Size of the trailing button.
const CGFloat kTrailingButtonIconPointSize = 17.0f;

}  // namespace

@implementation OmniboxPopupRowTrailingButton

- (void)setTrailingIconType:(TrailingIconType)trailingIconType {
  _trailingIconType = trailingIconType;

  if (trailingIconType == TrailingIconType::kNone) {
    self.accessibilityIdentifier = nil;
    self.hidden = YES;
    return;
  }

  self.hidden = NO;
  UIImage* icon;

  if (trailingIconType == TrailingIconType::kRefineQuery) {
    icon = DefaultSymbolWithPointSize(kRefineQuerySymbol,
                                      kTrailingButtonIconPointSize);
    self.accessibilityIdentifier =
        kOmniboxPopupRowAppendAccessibilityIdentifier;
  } else {
    icon = DefaultSymbolWithPointSize(kNavigateToTabSymbol,
                                      kTrailingButtonIconPointSize);
    self.accessibilityIdentifier =
        kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
  }
  // `imageWithHorizontallyFlippedOrientation` is flipping the icon
  // automatically when the UI is RTL/LTR.
  icon = [icon imageWithHorizontallyFlippedOrientation];
  icon = [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [self setImage:icon forState:UIControlStateNormal];
  [self updateTintColor];
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  _isHighlighted = isHighlighted;
  [self updateTintColor];
}

- (void)updateTintColor {
  self.tintColor =
      self.isHighlighted ? UIColor.whiteColor : [UIColor colorNamed:kBlueColor];
}

@end
