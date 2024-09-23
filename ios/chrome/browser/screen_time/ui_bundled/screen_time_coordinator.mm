// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen_time/ui_bundled/screen_time_coordinator.h"

#import "ios/chrome/browser/screen_time/model/screen_time_history_deleter_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/screen_time/ui_bundled/screen_time_mediator.h"
#import "ios/chrome/browser/screen_time/ui_bundled/screen_time_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface ScreenTimeCoordinator ()
@property(nonatomic, strong) ScreenTimeViewController* screenTimeViewController;
@property(nonatomic, strong) ScreenTimeMediator* mediator;
@end

@implementation ScreenTimeCoordinator

#pragma mark - Public properties

- (UIViewController*)viewController {
  return self.screenTimeViewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.browser->GetProfile()->IsOffTheRecord()) {
    self.screenTimeViewController =
        [ScreenTimeViewController sharedOTRInstance];
  } else {
    self.screenTimeViewController = [ScreenTimeViewController sharedInstance];
  }

  self.mediator = [[ScreenTimeMediator alloc]
        initWithWebStateList:self.browser->GetWebStateList()
      suppressUsageRecording:self.browser->GetProfile()->IsOffTheRecord()];

  ScreenTimeHistoryDeleterFactory::GetForProfile(self.browser->GetProfile());
}

- (void)stop {
  self.mediator = nil;
  [self.screenTimeViewController willMoveToParentViewController:nil];
  [self.screenTimeViewController.view removeFromSuperview];
  [self.screenTimeViewController removeFromParentViewController];
  self.screenTimeViewController = nil;
}

#pragma mark - Private properties

- (void)setMediator:(ScreenTimeMediator*)mediator {
  if (_mediator == mediator)
    return;

  if (_mediator) {
    [_mediator disconnect];
    _mediator = nil;
  }

  if (mediator) {
    DCHECK_NE(nil, self.screenTimeViewController);
    _mediator = mediator;
    _mediator.consumer = self.screenTimeViewController;
  }
}

- (void)setScreenTimeViewController:
    (ScreenTimeViewController*)screenTimeViewController {
  if (_screenTimeViewController == screenTimeViewController)
    return;

  if (_screenTimeViewController) {
    [_screenTimeViewController willMoveToParentViewController:nil];
    [_screenTimeViewController.view removeFromSuperview];
    [_screenTimeViewController removeFromParentViewController];
    _screenTimeViewController = nil;
  }

  if (screenTimeViewController) {
    DCHECK_NE(nil, self.baseViewController);
    UIView* screenTimeView = screenTimeViewController.view;
    screenTimeView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.baseViewController addChildViewController:screenTimeViewController];
    [self.baseViewController.view addSubview:screenTimeView];
    AddSameConstraints(screenTimeView, self.baseViewController.view);
    [screenTimeViewController
        didMoveToParentViewController:self.baseViewController];
    _screenTimeViewController = screenTimeViewController;
  }
}

@end
