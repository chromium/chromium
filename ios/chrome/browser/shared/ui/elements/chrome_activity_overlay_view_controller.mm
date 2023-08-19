// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/chrome_activity_overlay_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Leading and trailing container view margin.
const CGFloat kContainerViewSpacing = 5;
// Spacing between elements(label,activity view) and container view.
const CGFloat kPaddingElementsFromContainerView = 25;
// Corner radius of container view.
const CGFloat kContainerCornerRadius = 10;
// UIActivityIndicatorView's height and width
const CGFloat kActivityIndicatorViewSize = 55;
}  // namespace

@implementation ChromeActivityOverlayViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kScrimBackgroundColor];
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  containerView.layer.cornerRadius = kContainerCornerRadius;
  containerView.layer.masksToBounds = YES;
  UIActivityIndicatorView* activityView = GetLargeUIActivityIndicatorView();
  activityView.color = [UIColor colorNamed:kTextPrimaryColor];
  activityView.translatesAutoresizingMaskIntoConstraints = NO;
  [activityView startAnimating];
  [containerView addSubview:activityView];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.text = self.messageText;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  label.numberOfLines = 0;
  [containerView addSubview:label];
  [self.view addSubview:containerView];

  NSArray* constraints = @[
    [label.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor
                       constant:kPaddingElementsFromContainerView],
    [label.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor
                       constant:-kPaddingElementsFromContainerView],
    [label.topAnchor constraintEqualToAnchor:containerView.topAnchor
                                    constant:kPaddingElementsFromContainerView],
    [label.bottomAnchor constraintEqualToAnchor:activityView.topAnchor
                                       constant:-5],
    [activityView.bottomAnchor
        constraintEqualToAnchor:containerView.bottomAnchor
                       constant:-kPaddingElementsFromContainerView],
    [activityView.centerXAnchor constraintEqualToAnchor:label.centerXAnchor],
    [activityView.heightAnchor
        constraintEqualToConstant:kActivityIndicatorViewSize],
    [activityView.widthAnchor
        constraintEqualToConstant:kActivityIndicatorViewSize]
  ];
  [NSLayoutConstraint activateConstraints:constraints];

  LayoutSides sides = LayoutSides::kLeading | LayoutSides::kTrailing;
  NSDirectionalEdgeInsets insets = NSDirectionalEdgeInsetsMake(
      0, kContainerViewSpacing, 0, kContainerViewSpacing);
  AddSameConstraintsToSidesWithInsets(containerView, self.view, sides, insets);
  AddSameCenterYConstraint(self.view, containerView);

  // To allow message text to be read by screen reader, and to make sure the
  // speech will finish.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.messageText);
}

@end
