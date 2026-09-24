// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"

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
  //  Used for card layout.
  ActuationTaskCardView* _cardView;

  // USed for single and dual buttons layout.
  ChromeButton* _primaryButton;
  ChromeButton* _secondaryButton;
  UIStackView* _buttonStack;

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
  _cardView.hidden = YES;
  _buttonStack.hidden = YES;
  _primaryButton.hidden = YES;
  _secondaryButton.hidden = YES;

  BOOL hasIntervention = (data != nil);
  if (hasIntervention) {
    switch (data.type) {
      case ActuationInterventionType::kCard:
        _cardView.title = data.title;
        _cardView.subtitle = data.subtitle;
        _cardView.buttonTitle = data.primaryButtonText;
        _cardView.hidden = NO;
        break;
      case ActuationInterventionType::kSingleButton:
        _primaryButton.title = data.primaryButtonText;
        _primaryButton.hidden = NO;
        _buttonStack.hidden = NO;
        break;
      case ActuationInterventionType::kDualButton:
        _primaryButton.title = data.primaryButtonText;
        _secondaryButton.title = data.secondaryButtonText;
        _secondaryButton.hidden = NO;
        _primaryButton.hidden = NO;
        _buttonStack.hidden = NO;
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

  _secondaryButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
  _secondaryButton.accessibilityIdentifier =
      kActuationInterventionSecondaryButtonAccessibilityIdentifier;
  _secondaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_secondaryButton addTarget:self
                       action:@selector(didTapSecondaryButton)
             forControlEvents:UIControlEventTouchUpInside];

  _primaryButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _primaryButton.accessibilityIdentifier =
      kActuationInterventionPrimaryButtonAccessibilityIdentifier;
  _primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_primaryButton addTarget:self
                     action:@selector(didTapPrimaryButton)
           forControlEvents:UIControlEventTouchUpInside];

  _buttonStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _secondaryButton, _primaryButton ]];
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

// Handles primary action button taps.
- (void)didTapPrimaryButton {
  [self.delegate interventionView:self
                 didTriggerAction:ActuationInterventionAction::kPrimary];
}

// Handles secondary action button taps.
- (void)didTapSecondaryButton {
  [self.delegate interventionView:self
                 didTriggerAction:ActuationInterventionAction::kSecondary];
}

#pragma mark - ActuationTaskCardViewDelegate

- (void)taskCardViewDidTapActionButton:(ActuationTaskCardView*)view {
  [self didTapPrimaryButton];
}

- (void)taskCardView:(ActuationTaskCardView*)view
    didChangeCollapsedState:(BOOL)isCollapsed {
  // Non-collapsible card, required by ActuationTaskCardViewDelegate.
}

@end
