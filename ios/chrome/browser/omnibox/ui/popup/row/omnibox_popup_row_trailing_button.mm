// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_trailing_button.h"

#import "base/check.h"
#import "ios/chrome/browser/omnibox/public/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

namespace {

/// Size of the trailing button.
const CGFloat kTrailingButtonIconPointSizeMedium = 15.0f;

}  // namespace

@implementation OmniboxPopupRowTrailingButton {
  /// The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];

  if (self) {
    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.self ]
                       withAction:@selector(traitCollectionDidChangeAction)];
  }
  return self;
}

- (void)setTrailingIconType:(TrailingIconType)trailingIconType {
  if (trailingIconType == _trailingIconType) {
    return;
  }

  _trailingIconType = trailingIconType;
  [self updateButtonImageForCurrentState];
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  if (_isHighlighted == isHighlighted) {
    return;
  }

  _isHighlighted = isHighlighted;
  [self updateTintColor];
}

#pragma mark - private

- (void)setRefineQueryArrowDirectionDown:(BOOL)refineQueryArrowDirectionDown {
  if (refineQueryArrowDirectionDown == _refineQueryArrowDirectionDown) {
    return;
  }

  _refineQueryArrowDirectionDown = refineQueryArrowDirectionDown;

  [self updateButtonImageForCurrentState];
}

- (void)traitCollectionDidChangeAction {
  [self updateButtonImageForCurrentState];
}

- (void)updateButtonImageForCurrentState {
  CGFloat multiplier = OmniboxPopupRowContentSizeMultiplierForCategory(
      self.traitCollection.preferredContentSizeCategory);

  if (multiplier) {
    UIImage* icon;
    self.hidden = NO;
    switch (self.trailingIconType) {
      case TrailingIconType::kNone:
        self.accessibilityIdentifier = nil;
        self.hidden = YES;
        return;
      case TrailingIconType::kRefineQuery: {
        // The arrow should point in the direction of the omnibox.
        NSString* iconName = self.refineQueryArrowDirectionDown
                                 ? kRefineQueryDownSymbol
                                 : kRefineQuerySymbol;
        icon = DefaultSymbolWithPointSize(
            iconName, kTrailingButtonIconPointSizeMedium * multiplier);
        self.accessibilityIdentifier =
            kOmniboxPopupRowAppendAccessibilityIdentifier;
        break;
      }
      case TrailingIconType::kOpenExistingTab:
        icon = DefaultSymbolWithPointSize(
            kNavigateToTabSymbol,
            kTrailingButtonIconPointSizeMedium * multiplier);
        self.accessibilityIdentifier =
            kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
        break;
    }

    // `imageWithHorizontallyFlippedOrientation` is flipping the icon
    // automatically when the UI is RTL/LTR.
    icon = [icon imageWithHorizontallyFlippedOrientation];
    icon = [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

    [self setImage:icon forState:UIControlStateNormal];
    [self updateTintColor];
  }
}

- (void)updateTintColor {
  if (self.isHighlighted) {
    self.tintColor = UIColor.whiteColor;
    return;
  }

  self.tintColor = [UIColor colorNamed:kBlueColor];
}

@end
