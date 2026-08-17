// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_worklog_compact_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_compact_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Spacing & Layout Constants
const CGFloat kWidgetCornerRadius = 12.0;
const CGFloat kWidgetBorderWidth = 1.0;

const CGFloat kLayoutSpacing = 16.0;
const CGFloat kLayoutSpacingSmall = 8.0;
const CGFloat kIconSize = 16.0;

const CGFloat kButtonFontSize = 16.0;

// Struct defining the layout and assets for a mock timeline step.
struct MockStepConfig {
  NSString* title;
  NSString* subtitle = nil;
  Symbol iconSymbol = SymbolNone;
  ActuationWorklogItemStyle style;
  NSString* chipText = nil;
  Symbol chipIconSymbol = SymbolNone;
};

}  // namespace

@interface AIPrototypingWorklogCompactViewController () <
    ActuationWorklogCompactViewDelegate>
@end

@implementation AIPrototypingWorklogCompactViewController {
  ActuationWorklogCompactView* _worklogView;
  NSArray<ActuationWorklogItem*>* _mockSteps;
  NSArray<ActuationWorklogChip*>* _mockChips;
  NSUInteger _currentMockIndex;
  NSLayoutConstraint* _widgetHeightConstraint;

  UILabel* _descriptionLabel;
  UIView* _widgetContainer;
  UIButton* _nextButton;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Worklog Compact";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  _currentMockIndex = 0;
  [self setupMockSteps];
  [self createSubviews];
  [self setupConstraints];

  // Seed the first step on screen load
  [self appendNextMockStepAnimated:NO];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  _widgetContainer.layer.borderColor =
      [UIColor colorNamed:kSeparatorColor].CGColor;
}

#pragma mark - ActuationWorklogCompactViewDelegate

- (void)worklogCompactView:(ActuationWorklogCompactView*)view
           didChangeHeight:(CGFloat)targetHeight {
  _widgetHeightConstraint.constant = targetHeight;
  [self.view layoutIfNeeded];
}

#pragma mark - Private

// Pre-populates the mock steps array with predefined ActuationWorklogItems.
- (void)setupMockSteps {
  const MockStepConfig kMockStepConfigs[] = {
      // 1. No chip -> No chip (Labeled -> Simple)
      {.title = @"Finding theaters near D.C.",
       .subtitle = @"Finding best matches for AMC theaters within 10 miles.",
       .iconSymbol = SymbolMapPinAndEllipse,
       .style = ActuationWorklogItemStyle::kLabeled},
      {.title = @"Opening amctheatres.com",
       .style = ActuationWorklogItemStyle::kSimple},

      // 2. No chip -> chip (Simple -> Simple)
      {.title = @"Searching for AMC theaters.",
       .style = ActuationWorklogItemStyle::kSimple,
       .chipText = @"Google Search",
       .chipIconSymbol = SymbolMagnifyingglass},

      // 3. Chip -> Same chip (Simple -> Labeled)
      {.title = @"Checking seats availability.",
       .subtitle = @"Checking 7:30 PM showtimes for AMC Georgetown 14.",
       .iconSymbol = SymbolPersonTwoFill,
       .style = ActuationWorklogItemStyle::kLabeled,
       .chipText = @"Google Search",
       .chipIconSymbol = SymbolMagnifyingglass},

      // 4. Chip -> different chip (Labeled -> Card)
      {.title = @"Sign in to amctheatres.com",
       .subtitle =
           @"Gemini can use your saved info in Chrome to sign in for you.",
       .iconSymbol = SymbolKeyFill,
       .style = ActuationWorklogItemStyle::kCard,
       .chipText = @"Chrome Autofill",
       .chipIconSymbol = SymbolKeyFill},

      // 5. Chip -> no chip (Card -> Card)
      {.title = @"Calendar: Movie Showtime added",
       .subtitle = @"Sun, June 16 - 3:00 - 5:00 PM at 320 Bowling Dr.",
       .iconSymbol = SymbolCalendar,
       .style = ActuationWorklogItemStyle::kCard},

      // 6. Card (chip) -> Simple (no chip)
      {.title = @"Filling payment info",
       .subtitle = @"To continue the task, Gemini can ask Google Wallet to "
                   @"fill out credit card info.",
       .iconSymbol = SymbolCreditCardFill,
       .style = ActuationWorklogItemStyle::kCard,
       .chipText = @"Google Wallet",
       .chipIconSymbol = SymbolCreditCardFill},
      {.title = @"Verifying seat selection.",
       .style = ActuationWorklogItemStyle::kSimple},

      // 7. Labeled (chip) -> Simple (no chip)
      {.title = @"Sending ticket receipt.",
       .subtitle = @"Mailing confirmation ticket to your inbox.",
       .iconSymbol = SymbolMailFill,
       .style = ActuationWorklogItemStyle::kLabeled,
       .chipText = @"Google Wallet",
       .chipIconSymbol = SymbolCreditCardFill},
      {.title = @"Done.", .style = ActuationWorklogItemStyle::kSimple},
  };

  NSMutableArray<ActuationWorklogItem*>* steps = [NSMutableArray array];
  NSMutableArray<ActuationWorklogChip*>* chips = [NSMutableArray array];
  for (const MockStepConfig& config : kMockStepConfigs) {
    UIImage* icon = config.iconSymbol != SymbolNone
                        ? SymbolWithPointSize(config.iconSymbol, kIconSize)
                        : nil;
    ActuationWorklogItem* item =
        [[ActuationWorklogItem alloc] initWithTitle:config.title
                                           subtitle:config.subtitle
                                               icon:icon
                                              style:config.style
                                             active:YES];
    [steps addObject:item];

    ActuationWorklogChip* chip = nil;
    if (config.chipText) {
      UIImage* chipIcon =
          config.chipIconSymbol != SymbolNone
              ? SymbolWithPointSize(config.chipIconSymbol, kIconSize)
              : nil;
      chip = [[ActuationWorklogChip alloc] initWithText:config.chipText
                                                   icon:chipIcon];
    }
    [chips addObject:chip ?: (id)[NSNull null]];
  }
  _mockSteps = [steps copy];
  _mockChips = [chips copy];
}

// Instantiates and adds the subviews to the view hierarchy.
- (void)createSubviews {
  _descriptionLabel = [[UILabel alloc] init];
  _descriptionLabel.text =
      @"Simulates a single focused item timeline compact. Tap 'Trigger Next "
      @"Step' to append a new step and animate the transition.";
  _descriptionLabel.numberOfLines = 0;
  _descriptionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_descriptionLabel];

  _widgetContainer = [[UIView alloc] init];
  _widgetContainer.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  _widgetContainer.layer.borderColor =
      [UIColor colorNamed:kSeparatorColor].CGColor;
  _widgetContainer.layer.borderWidth = kWidgetBorderWidth;
  _widgetContainer.layer.cornerRadius = kWidgetCornerRadius;
  _widgetContainer.clipsToBounds = YES;
  _widgetContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_widgetContainer];

  _worklogView = [[ActuationWorklogCompactView alloc] init];
  _worklogView.delegate = self;
  _worklogView.translatesAutoresizingMaskIntoConstraints = NO;
  [_widgetContainer addSubview:_worklogView];
  AddSameConstraints(_worklogView, _widgetContainer);

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.attributedTitle = [[NSAttributedString alloc]
      initWithString:@"Trigger Next Step"
          attributes:@{
            NSFontAttributeName : [UIFont boldSystemFontOfSize:kButtonFontSize]
          }];

  __weak __typeof(self) weakSelf = self;
  UIAction* action = [UIAction actionWithHandler:^(UIAction* primaryAction) {
    [weakSelf appendNextMockStepAnimated:YES];
  }];
  _nextButton = [UIButton buttonWithConfiguration:buttonConfiguration
                                    primaryAction:action];
  _nextButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_nextButton];
}

// Configures Auto Layout constraints for the subviews.
- (void)setupConstraints {
  _widgetHeightConstraint =
      [_widgetContainer.heightAnchor constraintEqualToConstant:0.0];
  AddSameConstraintsToSidesWithInsets(
      _descriptionLabel, self.view.safeAreaLayoutGuide,
      LayoutSides::kTop | LayoutSides::kHorizontal,
      NSDirectionalEdgeInsetsMake(kLayoutSpacingSmall, kLayoutSpacing, 0.0,
                                  kLayoutSpacing));

  AddSameConstraintsToSidesWithInsets(
      _widgetContainer, self.view.safeAreaLayoutGuide, LayoutSides::kHorizontal,
      NSDirectionalEdgeInsetsMake(0.0, kLayoutSpacing, 0.0, kLayoutSpacing));

  [NSLayoutConstraint activateConstraints:@[
    _widgetHeightConstraint,
    [_widgetContainer.topAnchor
        constraintEqualToAnchor:_descriptionLabel.bottomAnchor
                       constant:kLayoutSpacing],
    [_nextButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-kLayoutSpacingSmall],
    [_nextButton.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

// Advances the compact log simulation to the next step, triggering the
// sliding transition if `animated` is `YES`.
- (void)appendNextMockStepAnimated:(BOOL)animated {
  // Reset showcase cycle.
  if (_currentMockIndex >= _mockSteps.count) {
    _currentMockIndex = 0;
  }

  ActuationWorklogItem* item = _mockSteps[_currentMockIndex];
  id chipObj = _mockChips[_currentMockIndex];
  ActuationWorklogChip* chip =
      [chipObj isKindOfClass:[ActuationWorklogChip class]] ? chipObj : nil;

  _currentMockIndex++;

  [_worklogView transitionToItem:item chip:chip animated:animated];
}

@end
