// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/following_feed_overlay/following_feed_overlay_coordinator.h"

#import <ostream>

#import "base/check_op.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/following_feed_overlay/following_feed_overlay_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/following_feed_overlay/following_feed_overlay_view_controller_delegate.h"

namespace {

/// Returns the configured navigation controller for
/// FollowingFeedOverlayCoordinator.
UINavigationController* GetConfiguredNavigationController() {
  UIViewController* root_view_controller = [[UIViewController alloc] init];
  root_view_controller.view.backgroundColor = UIColor.clearColor;

  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:root_view_controller];
  navigation_controller.modalPresentationStyle =
      UIModalPresentationOverFullScreen;
  return navigation_controller;
}

}  // namespace

@interface FollowingFeedOverlayCoordinator () <
    FollowingFeedOverlayViewControllerDelegate>
@end

@implementation FollowingFeedOverlayCoordinator {
  /// Navigatio controller that handles the display of the following feed
  /// overlay controller. This is used to simulate the navigation animation.
  UINavigationController* _navigationController;
  /// The view controller containing the following feed.
  FollowingFeedOverlayViewController* _followingFeedOverlayViewController;
  /// Whether the coordinator has been started.
  BOOL _started;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _navigationController = GetConfiguredNavigationController();
    _followingFeedOverlayViewController =
        [[FollowingFeedOverlayViewController alloc] init];
    _started = NO;
  }
  return self;
}

- (void)start {
  CHECK(!_started);
  _followingFeedOverlayViewController.delegate = self;

  __weak FollowingFeedOverlayCoordinator* weakSelf = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:NO
                                      completion:^{
                                        [weakSelf displayFollowingFeedOverlay];
                                      }];
}

- (void)stop {
  _started = NO;
  [self maybeDismissModal];
  self.animatePresentation = NO;
}

#pragma mark - FollowingFeedOverlayViewControllerDelegate

- (void)followingFeedOverlayViewControllerDidDismiss:
    (FollowingFeedOverlayViewController*)viewController {
  CHECK_EQ(viewController, _followingFeedOverlayViewController);
  [self maybeDismissModal];
}

#pragma mark - Private

/// Helper method to display the following feed overlay.
- (void)displayFollowingFeedOverlay {
  [_navigationController pushViewController:_followingFeedOverlayViewController
                                   animated:self.animatePresentation];
  _started = YES;
}

/// Helper method for dismissal.
- (void)maybeDismissModal {
  BOOL followingFeedVisible = _navigationController.topViewController ==
                              _followingFeedOverlayViewController;
  CHECK(!(_started && followingFeedVisible));
  if (_started) {
    /// Following feed is dismissed by user tapping "back" button or swiping
    /// back.
    [self.feedControlDelegate handleFeedSelected:FeedTypeDiscover];
  } else if (followingFeedVisible) {
    /// User tapped on an article.
    [_navigationController popToRootViewControllerAnimated:NO];
  } else {
    /// Actually dismiss the modal.
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
    _followingFeedOverlayViewController.delegate = nil;
  }
}

@end
