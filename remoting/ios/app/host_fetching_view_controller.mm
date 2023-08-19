// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_fetching_view_controller.h"

#import <MaterialComponents/MDCActivityIndicator.h>

#import "remoting/ios/app/remoting_theme.h"

static const CGFloat kActivityIndicatorStrokeWidth = 2.5f;
static const CGFloat kActivityIndicatorRadius = 20.f;

@implementation HostFetchingViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  MDCActivityIndicator* activityIndicator = [[MDCActivityIndicator alloc] init];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  activityIndicator.cycleColors = @[ RemotingTheme.refreshIndicatorColor ];
  activityIndicator.radius = kActivityIndicatorRadius;
  activityIndicator.strokeWidth = kActivityIndicatorStrokeWidth;
  [self.view addSubview:activityIndicator];
  [NSLayoutConstraint activateConstraints:@[
    [activityIndicator.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [activityIndicator.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
  ]];
  [activityIndicator startAnimating];
}

@end
