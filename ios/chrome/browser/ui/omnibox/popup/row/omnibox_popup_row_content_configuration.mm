// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_configuration.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_configuration+view.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Size of the trailing button.
const CGFloat kTrailingButtonPointSize = 17.0f;
/// Maximum number of lines displayed for search suggestions.
const NSInteger kWrappingSuggestNumberOfLines = 2;
/// Offset to align the suggestions with the omnibox leading image.
const CGFloat kOmniboxLayoutGuideLeadingOffset = -10.0f;

}  // namespace

@implementation OmniboxPopupRowContentConfiguration {
  /// Autocomplete suggestion displayed in the view.
  id<AutocompleteSuggestion> _suggestion;
}

- (instancetype)initWithAutocompleteSuggestion:
    (id<AutocompleteSuggestion>)suggestion {
  self = [super init];
  if (self) {
    _suggestion = suggestion;

    // Leading Icon.
    _leadingIcon = _suggestion.icon;

    // Primary Text.
    _primaryText = _suggestion.text;
    if (_suggestion.isWrapping) {
      _primaryTextNumberOfLines = kWrappingSuggestNumberOfLines;
    } else {
      _primaryTextNumberOfLines = 1;
    }

    // Secondary Text.
    _secondaryText = _suggestion.detailText;
    _secondaryTextNumberOfLines =
        _suggestion.hasAnswer ? _suggestion.numberOfLines : 1;
    _secondaryTextFading = !_suggestion.hasAnswer;
    _secondaryTextDisplayAsURL = _suggestion.isURL;

    // Trailing Button.
    NSString* trailingButtonActionName = nil;
    if (_suggestion.isTabMatch) {
      _trailingIcon = DefaultSymbolWithPointSize(kNavigateToTabSymbol,
                                                 kTrailingButtonPointSize);
      _trailingButtonAccessibilityIdentifier =
          kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
      trailingButtonActionName =
          l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB);

    } else if (_suggestion.isAppendable) {
      _trailingIcon = DefaultSymbolWithPointSize(kRefineQuerySymbol,
                                                 kTrailingButtonPointSize);
      _trailingButtonAccessibilityIdentifier =
          kOmniboxPopupRowAppendAccessibilityIdentifier;
      trailingButtonActionName =
          l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_APPEND);
    }

    if (_trailingIcon) {
      // Starting from iOS 16 `imageWithHorizontallyFlippedOrientation` is
      // flipping the icon automatically when the UI is RTL/LTR.
      if (@available(iOS 16, *)) {
        _trailingIcon = [_trailingIcon imageWithHorizontallyFlippedOrientation];
      }
      _trailingIcon = [_trailingIcon
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    }

    // Accessibility actions.
    NSMutableArray<UIAccessibilityCustomAction*>* customActions =
        [[NSMutableArray alloc] init];
    if (trailingButtonActionName) {
      [customActions
          addObject:[[UIAccessibilityCustomAction alloc]
                        initWithName:trailingButtonActionName
                              target:self
                            selector:@selector(trailingButtonTapped)]];
    }
    self.accessibilityCustomActions = customActions;
  }
  return self;
}

/// Layout this cell with the given data before displaying.
+ (instancetype)configurationWithAutocompleteSuggestion:
    (id<AutocompleteSuggestion>)suggestion {
  return [[OmniboxPopupRowContentConfiguration alloc]
      initWithAutocompleteSuggestion:suggestion];
}

#pragma mark - UIContentConfiguration

- (id)copyWithZone:(NSZone*)zone {
  // The configuration is immutable.
  return self;
}

- (UIView<UIContentView>*)makeContentView {
  return [[OmniboxPopupRowContentView alloc] initWithConfiguration:self];
}

/// Updates the configuration for different state of the view. This contains
/// everything that can change in the configuration's lifetime.
- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  if (![state isKindOfClass:[UIViewConfigurationState class]]) {
    return self;
  }

  UIViewConfigurationState* viewState = state;
  // Highlight.
  const BOOL allowHighlight = viewState.highlighted || viewState.selected;
  _showSelectedBackgroundView = allowHighlight;
  _leadingIconHighlighted = allowHighlight;
  _primaryText =
      allowHighlight
          ? [self highlightedAttributedStringWithString:_suggestion.text]
          : _suggestion.text;
  _secondaryText =
      allowHighlight
          ? [self highlightedAttributedStringWithString:_suggestion.detailText]
          : _suggestion.detailText;
  _trailingIconTintColor =
      allowHighlight ? UIColor.whiteColor : [UIColor colorNamed:kBlueColor];

  // Constraint to omnibox layout guide.
  if (self.omniboxLayoutGuide && CanUseOmniboxLayoutGuide()) {
    if (!ShouldApplyOmniboxLayoutGuide(state.traitCollection)) {
      _directionalLayoutMargin = NSDirectionalEdgeInsetsZero;
    } else {
      CGRect omniboxFrame = self.omniboxLayoutGuide.layoutFrame;
      CGRect popupFrame = self.omniboxLayoutGuide.owningView.bounds;
      UIEdgeInsets safeAreaInsets =
          self.omniboxLayoutGuide.owningView.safeAreaInsets;
      CGFloat leftSpace = CGRectGetMinX(omniboxFrame) -
                          CGRectGetMinX(popupFrame) - safeAreaInsets.left;
      CGFloat rightSpace = CGRectGetMaxX(popupFrame) -
                           CGRectGetMaxX(omniboxFrame) - safeAreaInsets.right;
      BOOL omniboxIsRTL =
          [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                      _semanticContentAttribute] ==
          UIUserInterfaceLayoutDirectionRightToLeft;
      CGFloat spacing = omniboxIsRTL ? rightSpace : leftSpace;
      CGFloat leadingMargin = spacing + kOmniboxLayoutGuideLeadingOffset;
      _directionalLayoutMargin =
          NSDirectionalEdgeInsetsMake(0, leadingMargin, 0, 0);
    }
  }

  return self;
}

#pragma mark - Private

/// Returns the input string but painted white when the blue and white
/// highlighting is enabled in pedals. Returns the original string otherwise.
- (NSAttributedString*)highlightedAttributedStringWithString:
    (NSAttributedString*)string {
  if (!string.length) {
    return nil;
  }
  NSMutableAttributedString* mutableString =
      [[NSMutableAttributedString alloc] initWithAttributedString:string];
  [mutableString addAttribute:NSForegroundColorAttributeName
                        value:[UIColor whiteColor]
                        range:NSMakeRange(0, string.length)];
  return mutableString;
}

/// Handles tap on the trailing button.
- (void)trailingButtonTapped {
  CHECK(_indexPath);
  [self.delegate omniboxPopupRowWithConfiguration:self
                  didTapTrailingButtonAtIndexPath:_indexPath];
}

@end
