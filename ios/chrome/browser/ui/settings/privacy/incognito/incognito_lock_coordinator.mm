// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_mediator.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

@interface IncognitoLockCoordinator () <
    IncognitoLockViewControllerPresentationDelegate>

@end

@implementation IncognitoLockCoordinator {
  // View controller presented by this coordinator.
  IncognitoLockViewController* _viewController;
  IncognitoLockMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  ReauthenticationModule* reauthModule = [[ReauthenticationModule alloc] init];
  _viewController =
      [[IncognitoLockViewController alloc] initWithReauthModule:reauthModule];
  _viewController.presentationDelegate = self;

  _mediator = [[IncognitoLockMediator alloc]
      initWithLocalState:GetApplicationContext()->GetLocalState()];
  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController.presentationDelegate = nil;
  _viewController.mutator = nil;
  _mediator.consumer = nil;
  _viewController = nil;
  _mediator = nil;
}

#pragma mark - IncognitoLockViewControllerPresentationDelegate

- (void)incognitoLockViewControllerDidRemove:
    (IncognitoLockViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate incognitoLockCoordinatorDidRemove:self];
}

@end
