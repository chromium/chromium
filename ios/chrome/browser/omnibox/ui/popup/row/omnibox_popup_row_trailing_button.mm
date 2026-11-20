// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_trailing_button.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/omnibox/public/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/omnibox_popup_row_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

/// Size of the trailing button.
const CGFloat kTrailingButtonIconPointSizeMedium = 15.0f;

/// Size of the dot shown in the trailing button.
const CGFloat kTrailingButtonDotPointSize = 8.0f;

}  // namespace

@implementation OmniboxPopupRowTrailingButton {
  /// The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];

  if (self) {
    self.hidden = YES;
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
  [self invalidateIntrinsicContentSize];

  [self updateButtonImageForCurrentState];
}

- (void)setIsHighlighted:(BOOL)isHighlighted {
  if (_isHighlighted == isHighlighted) {
    return;
  }

  _isHighlighted = isHighlighted;
  [self updateTintColor];
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  if (self.trailingIconType != TrailingIconType::kBlueDot) {
    return [super intrinsicContentSize];
  }
  // The dot gets the append arrow's size, so that it is centered where the
  // arrow is on other rows.
  CGFloat multiplier = OmniboxPopupRowContentSizeMultiplierForCategory(
      self.traitCollection.preferredContentSizeCategory);
  UIImage* appendArrow = SymbolWithPointSize(
      SymbolRefineQuery, kTrailingButtonIconPointSizeMedium * multiplier);
  return appendArrow.size;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  if (self.trailingIconType != TrailingIconType::kBlueDot ||
      CGRectIsEmpty(self.bounds)) {
    return;
  }
  // Keep the dot image unstretched even if UIButton shrinks its content rect
  // vertically across cell reuse.
  CGSize imageSize = self.currentImage.size;
  if (imageSize.width > 0 && imageSize.height > 0) {
    self.imageView.frame =
        CGRectMakeCenteredRectInFrame(self.bounds.size, imageSize);
  }
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
  [self invalidateIntrinsicContentSize];
  [self updateButtonImageForCurrentState];
}

- (void)updateButtonImageForCurrentState {
  CGFloat multiplier = OmniboxPopupRowContentSizeMultiplierForCategory(
      self.traitCollection.preferredContentSizeCategory);

  if (multiplier) {
    UIImage* icon;
    self.hidden = NO;
    self.userInteractionEnabled = YES;
    switch (self.trailingIconType) {
      case TrailingIconType::kNone:
        [self setImage:nil forState:UIControlStateNormal];
        self.accessibilityIdentifier = nil;
        self.hidden = YES;
        return;
      case TrailingIconType::kRefineQuery: {
        // The arrow should point in the direction of the omnibox.
        Symbol symbol = self.refineQueryArrowDirectionDown
                            ? SymbolRefineQueryDown
                            : SymbolRefineQuery;
        icon = SymbolWithPointSize(
            symbol, kTrailingButtonIconPointSizeMedium * multiplier);
        self.accessibilityIdentifier =
            kOmniboxPopupRowAppendAccessibilityIdentifier;
        break;
      }
      case TrailingIconType::kOpenExistingTab:
        icon = SymbolWithPointSize(
            SymbolNavigateToTab,
            kTrailingButtonIconPointSizeMedium * multiplier);
        self.accessibilityIdentifier =
            kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
        break;
      case TrailingIconType::kBlueDot:
        icon = SymbolWithPointSize(SymbolCircleFill,
                                   kTrailingButtonDotPointSize * multiplier);
        self.accessibilityIdentifier = nil;
        // The dot is only an indicator. Taps on it go to the row, which opens
        // the suggestion.
        self.userInteractionEnabled = NO;
        break;
    }

    // `imageWithHorizontallyFlippedOrientation` is flipping the icon
    // automatically when the UI is RTL/LTR.
    icon = [icon imageWithHorizontallyFlippedOrientation];
    icon = [icon imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

    [self setImage:icon forState:UIControlStateNormal];
    [self updateTintColor];
    [self setNeedsLayout];
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
