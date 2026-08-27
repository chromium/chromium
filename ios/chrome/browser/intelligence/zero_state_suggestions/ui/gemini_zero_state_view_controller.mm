// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_mutator.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// UI Constants for chip layout.
constexpr CGFloat kChipInterItemSpacing = 8.0;
constexpr CGFloat kChipContentPaddingVertical = 10.0;
constexpr CGFloat kChipContentPaddingHorizontal = 16.0;
constexpr CGFloat kContainerContentPadding = 12.0;

}  // namespace

@implementation GeminiZeroStateViewController {
  // Stack view containing chip button items.
  UIStackView* _stackView;
  // Label containing the greeting text.
  UILabel* _greetingLabel;
  // Currently displayed suggestions.
  NSArray<ZeroStateSuggestion*>* _zeroStateSuggestions;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.axis = UILayoutConstraintAxisVertical;
  containerStack.alignment = UIStackViewAlignmentFill;
  containerStack.spacing = kContainerContentPadding;
  [self.view addSubview:containerStack];

  AddSameConstraints(containerStack, self.view);

  _greetingLabel = [[UILabel alloc] init];
  _greetingLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _greetingLabel.text = @"TODO_PLACEHOLDER_TEXT";
  _greetingLabel.textAlignment = NSTextAlignmentCenter;
  _greetingLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  _greetingLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [containerStack addArrangedSubview:_greetingLabel];

  _stackView = [[UIStackView alloc] init];
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.spacing = kChipInterItemSpacing;
  _stackView.alignment = UIStackViewAlignmentLeading;
  _stackView.layoutMarginsRelativeArrangement = YES;
  _stackView.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      0, kContainerContentPadding, 0, kContainerContentPadding);
  [containerStack addArrangedSubview:_stackView];

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

// Rebuilds the chip button UI elements based on current suggestions.
- (void)updateSuggestionChips {
  for (UIView* subview in _stackView.arrangedSubviews) {
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
    [_stackView addArrangedSubview:chipButton];
  }
}

@end
