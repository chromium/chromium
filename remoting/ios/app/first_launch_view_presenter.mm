// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/first_launch_view_presenter.h"

#include "base/logging.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"

@interface FirstLaunchViewPresenter () {
  UINavigationController* _navController;
  __weak id<FirstLaunchViewControllerDelegate> _vcDelegate;

  // Null if the view is not presented.
  FirstLaunchViewController* _viewController;
}
@end

@implementation FirstLaunchViewPresenter

- (instancetype)initWithNavController:(UINavigationController*)navController
               viewControllerDelegate:
                   (id<FirstLaunchViewControllerDelegate>)delegate {
  _navController = navController;
  _vcDelegate = delegate;

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(userDidUpdateNotification:)
             name:kUserDidUpdate
           object:nil];

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)presentView {
  if (_viewController) {
    return;
  }
  _viewController = [[FirstLaunchViewController alloc] init];
  _viewController.delegate = _vcDelegate;
  // TODO(yuweih): We probably want a fading transition. For some reason the
  // transition doesn't seem to take effect.
  // TODO(yuweih): Maybe popToRootVC here (or call a delegate to do so) so that
  // other VC won't manipulate the view stack when the view is showing.
  [_navController pushViewController:_viewController animated:NO];
}

#pragma mark - Notification

- (void)userDidUpdateNotification:(NSNotification*)notification {
  BOOL isAuthenticated =
      [RemotingService.instance.authentication.user isAuthenticated];
  if (isAuthenticated && _viewController) {
    if (_navController.topViewController != _viewController) {
      LOG(ERROR)
          << "Couldn't pop FirstLaunchView due to broken view hierarchy.";
      return;
    }
    [_navController popViewControllerAnimated:NO];
    _viewController = nil;
  } else if (!isAuthenticated && !_viewController) {
    [self presentView];
  }
}

@end
