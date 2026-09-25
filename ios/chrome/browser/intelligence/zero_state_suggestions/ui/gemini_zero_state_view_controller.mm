// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_mutator.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// UI Constants for chip layout.
constexpr CGFloat kChipInterItemSpacing = 8.0;
constexpr CGFloat kChipContentPaddingVertical = 12.0;
constexpr CGFloat kChipContentPaddingHorizontal = 16.0;
constexpr CGFloat kContainerContentPadding = 12.0;
constexpr CGFloat kContainerTopPadding = 36.0;
constexpr CGFloat kGeminiLogoSize = 38.0;
constexpr CGFloat kLogoToGreetingSpacing = 16.0;
constexpr CGFloat kChipsContainerHeight = 180.0;

}  // namespace

@implementation GeminiZeroStateViewController {
  // Stack view containing chip button items.
  UIStackView* _suggestionsStack;
  // Image view displaying the Gemini logo.
  UIImageView* _logoImageView;
  // Label containing the greeting text.
  UILabel* _greetingLabel;
  // Currently displayed suggestions.
  NSArray<ZeroStateSuggestion*>* _zeroStateSuggestions;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.clipsToBounds = YES;

  UIStackView* headerStack = [[UIStackView alloc] init];
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;
  headerStack.axis = UILayoutConstraintAxisVertical;
  headerStack.spacing = kLogoToGreetingSpacing;
  [self.view addSubview:headerStack];

  _logoImageView = [[UIImageView alloc] init];
  _logoImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _logoImageView.contentMode = UIViewContentModeCenter;
  _logoImageView.image =
      [GeminiUIUtils createGradientGeminiLogo:kGeminiLogoSize];
  [_logoImageView setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisVertical];
  [_logoImageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [headerStack addArrangedSubview:_logoImageView];

  _greetingLabel = [[UILabel alloc] init];
  _greetingLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _greetingLabel.numberOfLines = 0;
  _greetingLabel.textAlignment = NSTextAlignmentCenter;
  _greetingLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  _greetingLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [_greetingLabel setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisVertical];
  [_greetingLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [self updateGreetingText];
  [headerStack addArrangedSubview:_greetingLabel];

  _suggestionsStack = [[UIStackView alloc] init];
  _suggestionsStack.translatesAutoresizingMaskIntoConstraints = NO;
  _suggestionsStack.axis = UILayoutConstraintAxisVertical;
  _suggestionsStack.spacing = kChipInterItemSpacing;
  _suggestionsStack.alignment = UIStackViewAlignmentLeading;
  _suggestionsStack.layoutMarginsRelativeArrangement = YES;
  _suggestionsStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      0, kContainerContentPadding, kContainerContentPadding,
      kContainerContentPadding);
  [_suggestionsStack setContentHuggingPriority:UILayoutPriorityRequired
                                       forAxis:UILayoutConstraintAxisVertical];
  [_suggestionsStack
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [self.view addSubview:_suggestionsStack];

  UILayoutGuide* headerLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:headerLayoutGuide];

  AddSameConstraintsToSides(headerStack, self.view,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      _suggestionsStack, self.view,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

  NSLayoutConstraint* guideTopConstraint = [headerLayoutGuide.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:kContainerTopPadding];
  guideTopConstraint.priority = UILayoutPriorityDefaultLow;

  NSLayoutConstraint* centerHeaderConstraint = [headerStack.centerYAnchor
      constraintEqualToAnchor:headerLayoutGuide.centerYAnchor];
  centerHeaderConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    guideTopConstraint,
    [headerLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor
                       constant:-(kChipsContainerHeight +
                                  kContainerContentPadding)],
    [_suggestionsStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:headerLayoutGuide.bottomAnchor
                                    constant:kContainerContentPadding],
    [headerStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:headerLayoutGuide.topAnchor],
    [headerStack.bottomAnchor
        constraintLessThanOrEqualToAnchor:headerLayoutGuide.bottomAnchor],
    centerHeaderConstraint,
  ]];

  [self updateSuggestionChips];
}

#pragma mark - GeminiZeroStateConsumer

- (void)setZeroStateSuggestions:(NSArray<ZeroStateSuggestion*>*)suggestions {
  _zeroStateSuggestions = [suggestions copy];

  if ([self isViewLoaded]) {
    [self updateSuggestionChips];
  }
}

#pragma mark - Private

// Updates the greeting label text based on the first name provided by the
// mutator.
- (void)updateGreetingText {
  NSString* trimmedName = [[self.mutator userFirstName]
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
  _greetingLabel.text =
      trimmedName.length > 0
          ? l10n_util::GetNSStringF(
                IDS_GEMINI_IOS_ZERO_STATE_GREETING_WITH_FIRST_NAME,
                base::SysNSStringToUTF16(trimmedName))
          : l10n_util::GetNSString(IDS_GEMINI_IOS_ZERO_STATE_GREETING);
}

// Rebuilds the chip button UI elements based on current suggestions.
- (void)updateSuggestionChips {
  for (UIView* subview in _suggestionsStack.arrangedSubviews) {
    [subview removeFromSuperview];
  }

  __weak __typeof(self) weakSelf = self;
  for (ZeroStateSuggestion* suggestion in _zeroStateSuggestions) {
    UIButtonConfiguration* config =
        [UIButtonConfiguration filledButtonConfiguration];
    config.title = suggestion.text;
    config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    config.contentInsets = NSDirectionalEdgeInsetsMake(
        kChipContentPaddingVertical, kChipContentPaddingHorizontal,
        kChipContentPaddingVertical, kChipContentPaddingHorizontal);
    config.titleAlignment = UIButtonConfigurationTitleAlignmentCenter;
    config.baseBackgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];

    UIAction* tapAction = [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator geminiZeroStateViewController:weakSelf
                                  didSelectSuggestion:suggestion];
    }];

    UIButton* chipButton = [UIButton buttonWithConfiguration:config
                                               primaryAction:tapAction];
    [chipButton setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisVertical];
    [chipButton
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];
    [_suggestionsStack addArrangedSubview:chipButton];
  }
}

@end
