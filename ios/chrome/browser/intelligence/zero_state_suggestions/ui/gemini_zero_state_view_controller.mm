// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_mutator.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// UI Constants for chip layout.
constexpr CGFloat kPlaceholderChipWidths[] = {200.0, 260.0, 140.0};
NSString* const kPlaceholderChipTitle = @" ";
constexpr NSTimeInterval kSuggestionChipsAnimationDuration = 0.3;
constexpr CGFloat kChipInterItemSpacing = 8.0;
constexpr CGFloat kChipContentPaddingVertical = 12.0;
constexpr CGFloat kChipContentPaddingHorizontal = 16.0;
constexpr CGFloat kContainerTopPadding = 36.0;
constexpr CGFloat kGeminiLogoSize = 38.0;
constexpr CGFloat kLogoToGreetingSpacing = 16.0;
constexpr CGFloat kCloseButtonSize = 44.0;
constexpr CGFloat kCloseButtonSymbolPointSize = 18.0;

}  // namespace

@implementation GeminiZeroStateViewController {
  // Stack view containing chip button items.
  UIStackView* _suggestionsStack;
  // Image view displaying the Gemini logo.
  UIImageView* _logoImageView;
  // Label containing the greeting text.
  UILabel* _greetingLabel;
  // Fixed list of suggestion chip buttons (initially placeholders).
  NSArray<UIButton*>* _suggestionButtons;
  // Fixed width constraints corresponding to each button in
  // `_suggestionButtons` when in the placeholder state.
  NSArray<NSLayoutConstraint*>* _placeholderWidthConstraints;
}

#pragma mark - UIViewController

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
  _suggestionsStack.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, 0, kChipInterItemSpacing, 0);
  [_suggestionsStack setContentHuggingPriority:UILayoutPriorityRequired
                                       forAxis:UILayoutConstraintAxisVertical];
  [_suggestionsStack
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [self.view addSubview:_suggestionsStack];

  [self setupSuggestionButtons];

  UIButtonConfiguration* closeButtonConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  closeButtonConfig.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  closeButtonConfig.image =
      SymbolWithPointSize(SymbolXMark, kCloseButtonSymbolPointSize);
  closeButtonConfig.baseBackgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  closeButtonConfig.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];

  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.mutator didTapZeroStateCloseButton];
  }];
  UIButton* closeButton = [UIButton buttonWithConfiguration:closeButtonConfig
                                              primaryAction:closeAction];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ICON_CLOSE);
  [self.view addSubview:closeButton];
  AddSquareConstraints(closeButton, kCloseButtonSize);

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
    [closeButton.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    guideTopConstraint,
    [headerLayoutGuide.bottomAnchor
        constraintEqualToAnchor:_suggestionsStack.topAnchor
                       constant:-kContainerTopPadding],
    [headerStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:headerLayoutGuide.topAnchor],
    [headerStack.bottomAnchor
        constraintLessThanOrEqualToAnchor:headerLayoutGuide.bottomAnchor],
    centerHeaderConstraint,
  ]];
}

#pragma mark - GeminiZeroStateConsumer

- (void)setZeroStateSuggestions:(NSArray<ZeroStateSuggestion*>*)suggestions {
  if (!suggestions) {
    [self resetToPlaceholderSuggestionChips];
    return;
  }

  // This ensures the view hierarchy is ready for the subsequent button updates
  [self.view layoutIfNeeded];

  __weak __typeof(self) weakSelf = self;
  for (NSUInteger i = 0; i < _suggestionButtons.count; ++i) {
    UIButton* chipButton = _suggestionButtons[i];
    [self removeAllActionsFromButton:chipButton];

    // If we receive less than 3 suggestions, hide the remaining buttons
    if (i >= suggestions.count) {
      chipButton.hidden = YES;
      chipButton.userInteractionEnabled = NO;
      continue;
    }

    ZeroStateSuggestion* suggestion = suggestions[i];
    _placeholderWidthConstraints[i].active = NO;

    UIButtonConfiguration* config = chipButton.configuration;
    config.title = suggestion.text;
    chipButton.configuration = config;
    chipButton.hidden = NO;
    chipButton.userInteractionEnabled = YES;
    chipButton.titleLabel.alpha = 0.0;

    UIAction* tapAction = [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator geminiZeroStateViewController:weakSelf
                                  didSelectSuggestion:suggestion];
    }];
    [chipButton addAction:tapAction
         forControlEvents:UIControlEventTouchUpInside];
  }

  [UIView animateWithDuration:kSuggestionChipsAnimationDuration
                   animations:^{
                     [weakSelf layoutSuggestionChips];
                   }];
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

// Initializes the 3 suggestion chip buttons and their placeholder width
// constraints, and adds them to `_suggestionsStack`.
- (void)setupSuggestionButtons {
  NSMutableArray<UIButton*>* buttons = [[NSMutableArray alloc] init];
  NSMutableArray<NSLayoutConstraint*>* widthConstraints =
      [[NSMutableArray alloc] init];

  for (CGFloat width : kPlaceholderChipWidths) {
    UIButton* chipButton = [self createPlaceholderChipButton];
    NSLayoutConstraint* widthConstraint =
        [chipButton.widthAnchor constraintEqualToConstant:width];
    widthConstraint.active = YES;

    [buttons addObject:chipButton];
    [widthConstraints addObject:widthConstraint];
    [_suggestionsStack addArrangedSubview:chipButton];
  }

  _suggestionButtons = [buttons copy];
  _placeholderWidthConstraints = [widthConstraints copy];
}

// Resets all suggestion chip buttons to their empty placeholder state.
- (void)resetToPlaceholderSuggestionChips {
  for (NSUInteger i = 0; i < _suggestionButtons.count; ++i) {
    UIButton* chipButton = _suggestionButtons[i];
    [self removeAllActionsFromButton:chipButton];

    UIButtonConfiguration* config = chipButton.configuration;
    config.title = kPlaceholderChipTitle;
    chipButton.configuration = config;
    chipButton.hidden = NO;
    chipButton.userInteractionEnabled = NO;
    chipButton.titleLabel.alpha = 0.0;
    _placeholderWidthConstraints[i].active = YES;
  }
}

// Removes all registered `UIAction`s from `button`.
- (void)removeAllActionsFromButton:(UIButton*)button {
  [button enumerateEventHandlers:^(UIAction* action, id target, SEL selector,
                                   UIControlEvents controlEvents, BOOL* stop) {
    if (action) {
      [button removeAction:action forControlEvents:controlEvents];
    }
  }];
}

// Creates an empty suggestion chip button in its initial placeholder state.
- (UIButton*)createPlaceholderChipButton {
  UIButtonConfiguration* config =
      [UIButtonConfiguration filledButtonConfiguration];
  config.title = kPlaceholderChipTitle;
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.contentInsets = NSDirectionalEdgeInsetsMake(
      kChipContentPaddingVertical, kChipContentPaddingHorizontal,
      kChipContentPaddingVertical, kChipContentPaddingHorizontal);
  config.titleAlignment = UIButtonConfigurationTitleAlignmentCenter;
  config.baseBackgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];

  UIButton* chipButton = [UIButton buttonWithConfiguration:config
                                             primaryAction:nil];
  chipButton.translatesAutoresizingMaskIntoConstraints = NO;
  chipButton.userInteractionEnabled = NO;
  chipButton.titleLabel.alpha = 0.0;
  [chipButton setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisVertical];
  [chipButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  return chipButton;
}

// Flushes pending layout for the suggestion chip buttons and the root view,
// and fades in the chip button titles.
- (void)layoutSuggestionChips {
  [self.view layoutIfNeeded];
  for (UIButton* chipButton in _suggestionButtons) {
    if (chipButton.hidden) {
      continue;
    }
    [chipButton layoutIfNeeded];
    [UIView performWithoutAnimation:^{
      [chipButton.titleLabel.layer removeAllAnimations];
      chipButton.titleLabel.alpha = 0.0;
    }];
    chipButton.titleLabel.alpha = 1.0;
  }
}

@end
