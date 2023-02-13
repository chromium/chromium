// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The duration for the presentation/dismissal animation of the inactive tabs
// view.
const NSTimeInterval kDuration = 0.2;
}  // namespace

@interface InactiveTabsCoordinator () <InactiveTabsViewControllerDelegate>

// The view controller displaying the inactive tabs.
@property(nonatomic, strong) InactiveTabsViewController* viewController;

// The mutually exclusive constraints for placing `viewController`.
@property(nonatomic, strong) NSLayoutConstraint* hiddenConstraint;
@property(nonatomic, strong) NSLayoutConstraint* visibleConstraint;

@end

@implementation InactiveTabsCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  self.viewController = [[InactiveTabsViewController alloc] init];
  self.viewController.delegate = self;
  UIView* baseView = self.baseViewController.view;
  UIView* view = self.viewController.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:self.viewController];
  [baseView addSubview:view];
  [self.viewController didMoveToParentViewController:self.baseViewController];

  self.hiddenConstraint =
      [baseView.trailingAnchor constraintEqualToAnchor:view.leadingAnchor];
  self.visibleConstraint =
      [baseView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [baseView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [baseView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [baseView.widthAnchor constraintEqualToAnchor:view.widthAnchor],
    self.hiddenConstraint,
  ]];

  [baseView layoutIfNeeded];
  [UIView animateWithDuration:kDuration
                   animations:^{
                     self.hiddenConstraint.active = NO;
                     self.visibleConstraint.active = YES;
                     [baseView layoutIfNeeded];
                   }];
}

- (void)stop {
  [super stop];

  UIView* baseView = self.baseViewController.view;

  [baseView layoutIfNeeded];
  [UIView animateWithDuration:kDuration
      animations:^{
        self.visibleConstraint.active = NO;
        self.hiddenConstraint.active = YES;
        [baseView layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        [self.viewController willMoveToParentViewController:nil];
        [self.viewController.view removeFromSuperview];
        [self.viewController removeFromParentViewController];
        self.viewController = nil;
      }];
}

#pragma mark - InactiveTabsViewControllerDelegate

- (void)inactiveTabsViewControllerDidTapBackButton:
    (InactiveTabsViewController*)inactiveTabsViewController {
  [self.delegate inactiveTabsCoordinatorDidFinish:self];
}

@end
