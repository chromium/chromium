// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/chrome_activity_overlay_view_controller.h"

#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacing between container view and subviews.
const CGFloat kContainerViewSpacing = 25;
// Corner radius of container view.
const CGFloat kContainerCornerRadius = 10;
// UIActivityIndicatorView's height and width
const CGFloat kActivityIndicatorViewSize = 55;
}

@implementation ChromeActivityOverlayViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  containerView.layer.cornerRadius = kContainerCornerRadius;
  containerView.layer.masksToBounds = YES;
  UIActivityIndicatorView* activityView = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
  activityView.color = [UIColor colorNamed:kTextPrimaryColor];
  activityView.translatesAutoresizingMaskIntoConstraints = NO;
  [activityView startAnimating];
  [containerView addSubview:activityView];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.text = self.messageText;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [containerView addSubview:label];

  NSArray* constraints = @[
    [label.leadingAnchor constraintEqualToAnchor:containerView.leadingAnchor
                                        constant:kContainerViewSpacing],
    [label.trailingAnchor constraintEqualToAnchor:containerView.trailingAnchor
                                         constant:-kContainerViewSpacing],
    [label.topAnchor constraintEqualToAnchor:containerView.topAnchor
                                    constant:kContainerViewSpacing],
    [label.bottomAnchor constraintEqualToAnchor:activityView.topAnchor
                                       constant:-5],
    [activityView.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor
                       constant:-kContainerViewSpacing],
    [activityView.centerXAnchor constraintEqualToAnchor:label.centerXAnchor
                                               constant:0],
    [activityView.heightAnchor
        constraintEqualToConstant:kActivityIndicatorViewSize],
    [activityView.widthAnchor
        constraintEqualToConstant:kActivityIndicatorViewSize]
  ];
  [NSLayoutConstraint activateConstraints:constraints];

  [self.view addSubview:containerView];
  AddSameCenterConstraints(self.view, containerView);

  // To allow message text to be read by screen reader, and to make sure the
  // speech will finish.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.messageText);
}

@end
