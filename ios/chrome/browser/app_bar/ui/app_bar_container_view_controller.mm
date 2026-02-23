// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_controller.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_delegate.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

@interface AppBarContainerViewController () <AppBarContainerViewDelegate>
@property(nonatomic, strong) AppBarContainerView* view;
@end

@implementation AppBarContainerViewController {
  AppBarViewController* _appBar;
  AppBarContainerView* _appBarContainer;
}

@dynamic view;

- (void)setAppBar:(AppBarViewController*)appBar {
  if (_appBar == appBar) {
    return;
  }
  [_appBar willMoveToParentViewController:nil];
  [_appBar.view removeFromSuperview];
  [_appBar removeFromParentViewController];

  _appBar = appBar;

  [self addChildViewController:_appBar];
  [self.view setAppBar:_appBar.view];
  [_appBar didMoveToParentViewController:self];
}

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[AppBarContainerView alloc] init];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateLayout];
      }
                      completion:nil];
}

#pragma mark - AppBarContainerViewDelegate

- (void)appBarContainerDidMoveToWindow:(AppBarContainerView*)appBarContainer {
  [self updateLayout];
}

#pragma mark - Private

- (void)updateLayout {
  UIWindowScene* windowScene = self.view.window.windowScene;
  if (!windowScene) {
    return;
  }

  UIInterfaceOrientation orientation =
      windowScene.effectiveGeometry.interfaceOrientation;

  CGFloat angle;

  switch (orientation) {
    case UIInterfaceOrientationLandscapeLeft:
      angle = M_PI_2;
      break;

    case UIInterfaceOrientationLandscapeRight:
      angle = -M_PI_2;
      break;

    default:  // Portrait
      angle = 0;
      break;
  }

  self.view.transform = CGAffineTransformMakeRotation(angle);
  [_appBar updateForAngle:-angle];
}

@end
