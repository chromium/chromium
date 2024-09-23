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
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Size of the trailing button.
const CGFloat kTrailingButtonPointSize = 17.0f;
/// Maximum number of lines displayed for search suggestions.
const NSInteger kWrappingSuggestNumberOfLines = 2;

}  // namespace

NSString* const OmniboxPopupRowCellReuseIdentifier = @"OmniboxPopupRowCell";
const CGFloat kOmniboxPopupCellMinimumHeight = 58;

/// Redefines "Content View interface" as readwrite.
@interface OmniboxPopupRowContentConfiguration ()

// Background.
@property(nonatomic, assign, readwrite) BOOL isBackgroundHighlighted;

// Leading Icon.
@property(nonatomic, strong, readwrite) id<OmniboxIcon> leadingIcon;
@property(nonatomic, assign, readwrite) BOOL leadingIconHighlighted;

// Primary text.
@property(nonatomic, strong, readwrite) NSAttributedString* primaryText;
@property(nonatomic, assign, readwrite) NSInteger primaryTextNumberOfLines;

// Secondary Text.
@property(nonatomic, strong, readwrite) NSAttributedString* secondaryText;
@property(nonatomic, assign, readwrite) NSInteger secondaryTextNumberOfLines;
@property(nonatomic, assign, readwrite) BOOL secondaryTextFading;
@property(nonatomic, assign, readwrite) BOOL secondaryTextDisplayAsURL;

// Trailing Icon.
@property(nonatomic, strong, readwrite) UIImage* trailingIcon;
@property(nonatomic, strong, readwrite) UIColor* trailingIconTintColor;
@property(nonatomic, strong, readwrite)
    NSString* trailingButtonAccessibilityIdentifier;

// Margins.
@property(nonatomic, assign, readwrite)
    NSDirectionalEdgeInsets directionalLayoutMargin;
@property(nonatomic, assign, readwrite) BOOL isPopoutOmnibox;

@end

@implementation OmniboxPopupRowContentConfiguration

/// Layout this cell with the given data before displaying.
+ (instancetype)cellConfiguration {
  return [[OmniboxPopupRowContentConfiguration alloc] init];
}

+ (NSAttributedString*)highlightedAttributedStringWithString:
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

- (void)setSuggestion:(id<AutocompleteSuggestion>)suggestion {
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
    // `imageWithHorizontallyFlippedOrientation` is flipping the icon
    // automatically when the UI is RTL/LTR.
    _trailingIcon = [_trailingIcon imageWithHorizontallyFlippedOrientation];
    _trailingIcon = [_trailingIcon
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  // Accessibility actions.
  NSMutableArray<UIAccessibilityCustomAction*>* customActions =
      [[NSMutableArray alloc] init];
  if (trailingButtonActionName) {
    [customActions addObject:[[UIAccessibilityCustomAction alloc]
                                 initWithName:trailingButtonActionName
                                       target:self
                                     selector:@selector(trailingButtonTapped)]];
  }
  self.accessibilityCustomActions = customActions;
}

#pragma mark - UIContentConfiguration

- (id)copyWithZone:(NSZone*)zone {
  __typeof__(self) configuration = [[self.class alloc] init];
  configuration.suggestion = self.suggestion;
  configuration.delegate = self.delegate;
  configuration.indexPath = self.indexPath;
  configuration.showSeparator = self.showSeparator;
  configuration.semanticContentAttribute = self.semanticContentAttribute;
  configuration.faviconRetriever = self.faviconRetriever;
  configuration.imageRetriever = self.imageRetriever;

  // Setting `suggestion` already sets some properties in "Content View
  // interface". Update the properties that can change with
  // `updatedConfigurationForState`.
  configuration.isBackgroundHighlighted = self.isBackgroundHighlighted;
  configuration.leadingIconHighlighted = self.leadingIconHighlighted;
  configuration.primaryText = self.primaryText;
  configuration.secondaryText = self.secondaryText;
  configuration.trailingIconTintColor = self.trailingIconTintColor;
  configuration.directionalLayoutMargin = self.directionalLayoutMargin;
  configuration.isPopoutOmnibox = self.isPopoutOmnibox;
  return configuration;
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
  OmniboxPopupRowContentConfiguration* configuration = [self copy];

  UIViewConfigurationState* viewState = state;
  // Highlight.
  const BOOL allowHighlight = viewState.highlighted || viewState.selected;
  configuration.isBackgroundHighlighted = allowHighlight;
  configuration.leadingIconHighlighted = allowHighlight;
  configuration.primaryText =
      allowHighlight
          ? [self.class highlightedAttributedStringWithString:_suggestion.text]
          : _suggestion.text;
  configuration.secondaryText =
      allowHighlight
          ? [self.class highlightedAttributedStringWithString:_suggestion
                                                                  .detailText]
          : _suggestion.detailText;
  configuration.trailingIconTintColor =
      allowHighlight ? UIColor.whiteColor : [UIColor colorNamed:kBlueColor];

  // Update margins for popout omnibox. Popout omnibox is only available on
  // regular size class.
  configuration.isPopoutOmnibox =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET &&
      IsRegularXRegularSizeClass(state.traitCollection);

  return configuration;
}

#pragma mark - Private

/// Handles tap on the trailing button.
- (void)trailingButtonTapped {
  CHECK(_indexPath);
  [self.delegate omniboxPopupRowWithConfiguration:self
                  didTapTrailingButtonAtIndexPath:_indexPath];
}

@end
