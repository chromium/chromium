// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"

#import "base/check_op.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/common/ui/util/chrome_button.h"

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;

@interface ActuationInterventionView () <ActuationTaskCardViewDelegate>
@end

@implementation ActuationInterventionView {
  UIStackView* _stackView;
  // Used for card layout.
  ActuationTaskCardView* _cardView;

  // Used for row and column layouts. Holds one button per action.
  UIStackView* _buttonStack;

  // Intervention actions, kept to map taps to the action sent to the delegate.
  NSArray<ActuationInterventionAction*>* _actions;

  // Active only when unconfigured to collapse the view to zero height.
  NSLayoutConstraint* _collapsedHeightConstraint;
}

#pragma mark - Public

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setupSubviews];
    [self setupConstraints];
    [self configureWithData:nil];
  }
  return self;
}

- (void)configureWithData:(ActuationInterventionData*)data {
  _actions = data.actions;
  _cardView.hidden = YES;
  _buttonStack.hidden = YES;
  [self removeActionButtons];

  BOOL hasIntervention = (data != nil);
  if (hasIntervention) {
    switch (data.type) {
      case ActuationInterventionType::kCard:
        _cardView.title = data.title;
        _cardView.subtitle = data.subtitle;
        _cardView.buttonTitle = data.actions.firstObject.title;
        _cardView.hidden = NO;
        break;
      case ActuationInterventionType::kRow:
        [self showButtonsForActions:data.actions
                               axis:UILayoutConstraintAxisHorizontal];
        break;
      case ActuationInterventionType::kColumn:
        [self showButtonsForActions:data.actions
                               axis:UILayoutConstraintAxisVertical];
        break;
    }
  }

  self.hidden = !hasIntervention;
  _collapsedHeightConstraint.active = !hasIntervention;
}

#pragma mark - Private

// Creates the view hierarchy.
- (void)setupSubviews {
  self.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      kSpacingMedium, kSpacingLarge, kSpacingLarge, kSpacingLarge);

  _cardView = [[ActuationTaskCardView alloc] initWithTitle:@""
                                               buttonTitle:@""
                                               collapsible:NO];
  _cardView.accessibilityIdentifier =
      kActuationInterventionCardAccessibilityIdentifier;
  _cardView.delegate = self;
  _cardView.translatesAutoresizingMaskIntoConstraints = NO;

  _buttonStack = [[UIStackView alloc] init];
  _buttonStack.accessibilityIdentifier =
      kActuationInterventionButtonStackAccessibilityIdentifier;
  _buttonStack.distribution = UIStackViewDistributionFillEqually;
  _buttonStack.spacing = kSpacingSmall;
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;

  _stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _cardView, _buttonStack ]];
  _stackView.axis = UILayoutConstraintAxisVertical;
  _stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_stackView];
}

// Configures layout constraints.
- (void)setupConstraints {
  _collapsedHeightConstraint =
      [self.heightAnchor constraintEqualToConstant:0.0];

  UILayoutGuide* marginsGuide = self.layoutMarginsGuide;
  // Lowered priority to prevent conflicts when the view collapses.
  NSLayoutConstraint* bottomConstraint = [_stackView.bottomAnchor
      constraintEqualToAnchor:marginsGuide.bottomAnchor];
  bottomConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [_stackView.topAnchor constraintEqualToAnchor:marginsGuide.topAnchor],
    [_stackView.leadingAnchor
        constraintEqualToAnchor:marginsGuide.leadingAnchor],
    [_stackView.trailingAnchor
        constraintEqualToAnchor:marginsGuide.trailingAnchor],
    bottomConstraint,
  ]];
}

// Shows a button per action in `_buttonStack`, laid out along `axis`. A lone
// action is primary, multiple actions are tertiary.
- (void)showButtonsForActions:(NSArray<ActuationInterventionAction*>*)actions
                         axis:(UILayoutConstraintAxis)axis {
  _buttonStack.axis = axis;
  _buttonStack.hidden = NO;
  ChromeButtonStyle style =
      actions.count == 1 ? ChromeButtonStylePrimary : ChromeButtonStyleTertiary;
  for (NSUInteger index = 0; index < actions.count; ++index) {
    ChromeButton* button = [[ChromeButton alloc] initWithStyle:style];
    button.title = actions[index].title;
    button.tag = static_cast<NSInteger>(index);
    button.accessibilityIdentifier =
        ActuationInterventionActionButtonAccessibilityIdentifier(index);
    [button addTarget:self
                  action:@selector(didTapActionButton:)
        forControlEvents:UIControlEventTouchUpInside];
    [_buttonStack addArrangedSubview:button];
  }
}

// Removes all the buttons from `_buttonStack`.
- (void)removeActionButtons {
  for (UIView* button in _buttonStack.arrangedSubviews) {
    [button removeFromSuperview];
  }
}

// Handles taps on the action buttons, tagged with the index of their action.
- (void)didTapActionButton:(UIButton*)button {
  [self triggerActionAtIndex:static_cast<NSUInteger>(button.tag)];
}

// Notifies the delegate that the action at `index` of `_actions` was triggered.
- (void)triggerActionAtIndex:(NSUInteger)index {
  CHECK_LT(index, _actions.count);
  [self.delegate interventionView:self didTriggerAction:_actions[index]];
}

#pragma mark - ActuationTaskCardViewDelegate

- (void)taskCardViewDidTapActionButton:(ActuationTaskCardView*)view {
  [self triggerActionAtIndex:0];
}

- (void)taskCardView:(ActuationTaskCardView*)view
    didChangeCollapsedState:(BOOL)isCollapsed {
  // Non-collapsible card, required by ActuationTaskCardViewDelegate.
}

@end
