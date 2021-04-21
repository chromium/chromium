// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin_screen_view_controller.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/material_components/activity_indicator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Size of the activity indicator.
const CGFloat kActivityIndicatorSize = 48;
}  // namespace

@interface SigninScreenViewController ()

// Button controlling the display of the selected identity.
@property(nonatomic, strong) IdentityButtonControl* identityControl;

// Scrim displayed above the view when the UI is disabled.
@property(nonatomic, strong) UIView* scrimView;
// Activity indicator for the scrim view.
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;

@end

@implementation SigninScreenViewController
@dynamic delegate;

#pragma mark - Public

- (void)viewDidLoad {
  self.titleText = @"Test Sign-in Screen";
  self.primaryActionString = @"Test Continue Button";

  self.identityControl =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  [self.identityControl addTarget:self
                           action:@selector(identityButtonControlTapped:
                                                               forEvent:)
                 forControlEvents:UIControlEventTouchUpInside];

  // TODO(crbug.com/1189836): Add the identity control to the wrapper view.

  // Call super after setting up the strings and others, as required per super
  // class.
  [super viewDidLoad];
}

#pragma mark - Properties

- (UIView*)scrimView {
  if (!_scrimView) {
    _scrimView = [[UIView alloc] initWithFrame:CGRectZero];
    _scrimView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrimView.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];

    self.activityIndicator = [[MDCActivityIndicator alloc]
        initWithFrame:CGRectMake(0, 0, kActivityIndicatorSize,
                                 kActivityIndicatorSize)];
    self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.activityIndicator];
    AddSameCenterConstraints(_scrimView, self.activityIndicator);
    self.activityIndicator.cycleColors = ActivityIndicatorBrandedCycleColors();
  }
  return _scrimView;
}

#pragma mark - SignInScreenConsumer

- (void)setUserImage:(UIImage*)userImage {
  if (userImage) {
    [self.identityControl setIdentityAvatar:userImage];
  } else {
    // TODO(crbug.com/1189836): Update with default avatar.
  }
}

- (void)setSelectedIdentityUserName:(NSString*)userName email:(NSString*)email {
  self.identityControl.hidden = NO;
  [self.identityControl setIdentityName:userName email:email];
}

- (void)hideIdentityButtonControl {
  self.identityControl.hidden = YES;
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  if (UIEnabled) {
    [self.scrimView removeFromSuperview];
    self.scrimView = nil;
  } else {
    [self.view addSubview:self.scrimView];
    AddSameConstraints(self.view, self.scrimView);
    [self.activityIndicator startAnimating];
  }
}

#pragma mark - AuthenticationFlowDelegate

- (void)didPresentDialog {
  [self.activityIndicator stopAnimating];
}

- (void)didDismissDialog {
  [self.activityIndicator startAnimating];
}

#pragma mark - Private

- (void)identityButtonControlTapped:(id)sender forEvent:(UIEvent*)event {
  UITouch* touch = event.allTouches.anyObject;
  [self.delegate showAccountPickerFromPoint:[touch locationInView:nil]];
}

@end
