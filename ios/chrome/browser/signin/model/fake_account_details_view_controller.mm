// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_account_details_view_controller.h"

#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation FakeAccountDetailsViewController {
  __weak id<SystemIdentity> _identity;
  // Completion block to call once the view controller has been dismissed.
  ProceduralBlock _dismissalCompletion;
}

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity
             dismissalCompletion:(ProceduralBlock)dismissalCompletion {
  if ((self = [super init])) {
    _identity = identity;
    _dismissalCompletion = dismissalCompletion;
  }
  return self;
}

- (void)dismissAnimated:(BOOL)animated {
  ProceduralBlock dismissalCompletion = _dismissalCompletion;
  _dismissalCompletion = nil;
  [self dismissViewControllerAnimated:animated completion:dismissalCompletion];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* view = self.view;
  view.accessibilityIdentifier = kFakeAccountDetailsViewIdentifier;
  // Obnoxious color, this is a test screen.
  view.backgroundColor = [UIColor orangeColor];

  UIButton* doneButton = [[UIButton alloc] init];
  doneButton.translatesAutoresizingMaskIntoConstraints = NO;
  doneButton.accessibilityIdentifier = kFakeAccountDetailsDoneButtonIdentifier;
  [doneButton addTarget:self
                 action:@selector(doneAction:)
       forControlEvents:UIControlEventTouchUpInside];
  [doneButton setTitle:@"Done" forState:UIControlStateNormal];
  [view addSubview:doneButton];

  UITextView* detailView = [[UITextView alloc] init];
  detailView.translatesAutoresizingMaskIntoConstraints = NO;
  detailView.text =
      [NSString stringWithFormat:@"Details: %@", [_identity debugDescription]];
  [view addSubview:detailView];

  AddSameConstraintsToSides(doneButton, view.safeAreaLayoutGuide,
                            LayoutSides::kTop | LayoutSides::kLeading);
  AddSameConstraintsToSides(detailView, view.safeAreaLayoutGuide,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  [NSLayoutConstraint activateConstraints:@[
    [doneButton.bottomAnchor constraintEqualToAnchor:detailView.topAnchor],
    [detailView.heightAnchor constraintEqualToAnchor:view.heightAnchor
                                          multiplier:.5],
  ]];
}

#pragma mark - Private

// Called by the done button.
- (void)doneAction:(id)sender {
  [self dismissAnimated:YES];
}

@end
