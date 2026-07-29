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
const CGFloat kIconSize = 16.0;

const CGFloat kButtonFontSize = 16.0;

// Struct defining the layout and assets for a mock timeline step.
struct MockStepConfig {
  NSString* title;
  NSString* subtitle = nil;
  NSString* iconName = nil;
  ActuationWorklogItemStyle style;
};

}  // namespace

@interface AIPrototypingWorklogCompactViewController () <
    ActuationWorklogCompactViewDelegate>
@end

@implementation AIPrototypingWorklogCompactViewController {
  ActuationWorklogCompactView* _worklogView;
  NSArray<ActuationWorklogItem*>* _mockSteps;
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
      {.title = @"Task started",
       .subtitle = @"Use Gemini carefully and take control if needed. You "
                   @"are responsible for Gemini's actions.",
       .iconName = @"play.fill",
       .style = ActuationWorklogItemStyle::kLabeled},
      {.title = @"Opening a new Tab.",
       .style = ActuationWorklogItemStyle::kSimple},
      {.title = @"Searching for AMC theaters.",
       .style = ActuationWorklogItemStyle::kSimple},
      {.title = @"Searching for film showtimes.",
       .style = ActuationWorklogItemStyle::kSimple},
      {.title = @"Finding theaters near Washington, D.C.",
       .subtitle = @"Finding best matches for AMC theaters within 10 miles.",
       .iconName = @"mappin.and.ellipse",
       .style = ActuationWorklogItemStyle::kLabeled},
      {.title = @"Checking seats availability.",
       .subtitle = @"Checking 7:30 PM showtimes for AMC Georgetown 14.",
       .iconName = @"person.2.fill",
       .style = ActuationWorklogItemStyle::kLabeled},
      {.title = @"Sign in to amctheatres.com",
       .subtitle =
           @"Gemini can use your saved info in Chrome to sign in for you.",
       .iconName = @"key.fill",
       .style = ActuationWorklogItemStyle::kCard},
      {.title = @"Filling payment info",
       .subtitle = @"To continue the task, Gemini can ask Google Wallet to "
                   @"fill out credit card info.",
       .iconName = @"creditcard.fill",
       .style = ActuationWorklogItemStyle::kCard},
      {.title = @"Calendar: Golden Gate Tea party",
       .subtitle = @"Sun, June 16 - 3:00 - 5:00 PM at 320 Bowling Dr.",
       .iconName = @"calendar",
       .style = ActuationWorklogItemStyle::kCard},
  };

  NSMutableArray<ActuationWorklogItem*>* steps = [NSMutableArray array];
  for (const MockStepConfig& config : kMockStepConfigs) {
    UIImage* icon = config.iconName
                        ? DefaultSymbolWithPointSize(config.iconName, kIconSize)
                        : nil;
    ActuationWorklogItem* item =
        [[ActuationWorklogItem alloc] initWithTitle:config.title
                                           subtitle:config.subtitle
                                               icon:icon
                                              style:config.style
                                             active:NO];
    [steps addObject:item];
  }
  _mockSteps = [steps copy];
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
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing, 0.0,
                                  kLayoutSpacing));

  AddSameConstraintsToSidesWithInsets(
      _widgetContainer, self.view.safeAreaLayoutGuide,
      LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(0.0, kLayoutSpacing, 0.0, kLayoutSpacing));

  [NSLayoutConstraint activateConstraints:@[
    _widgetHeightConstraint,
    [_widgetContainer.topAnchor
        constraintEqualToAnchor:_descriptionLabel.bottomAnchor
                       constant:2.0 * kLayoutSpacing],
    [_nextButton.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-3.0 * kLayoutSpacing],
    [_nextButton.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

// Advances the compact log simulation to the next step, managing the active
// state of items and triggering the sliding transition if `animated` is `YES`.
- (void)appendNextMockStepAnimated:(BOOL)animated {
  // Deactivate the previous step.
  if (_currentMockIndex > 0) {
    _mockSteps[_currentMockIndex - 1].active = NO;
  }

  // Reset showcase cycle.
  if (_currentMockIndex >= _mockSteps.count) {
    _currentMockIndex = 0;
  }

  ActuationWorklogItem* item = _mockSteps[_currentMockIndex];
  item.active = YES;
  _currentMockIndex++;

  [_worklogView transitionToItem:item animated:animated];
}

@end
