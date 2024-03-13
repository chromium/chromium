// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_view_controller.h"

@interface PersonalizeGoogleServicesCoordinator () <
    PersonalizeGoogleServicesViewControllerPresentationDelegate,
    PersonalizeGoogleServicesCommandHandler>
@end

@implementation PersonalizeGoogleServicesCoordinator {
  PersonalizeGoogleServicesViewController* _viewController;
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
  _viewController = [[PersonalizeGoogleServicesViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.presentationDelegate = self;
  _viewController.handler = self;

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

#pragma mark - PersonalizeGoogleServicesViewControllerPresentationDelegate

- (void)personalizeGoogleServicesViewcontrollerDidRemove:
    (PersonalizeGoogleServicesViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate personalizeGoogleServicesCoordinatorWasRemoved:self];
}

#pragma mark - PersonalizeGoogleServicesCommandHandler

- (void)openWebAppActivityDialog {
  // TODO(crbug.com/324091979): Open Web & App Activity page.
}

- (void)openLinkedGoogleServicesDialog {
  // TODO(crbug.com/324091979): Open Linked Google services page.
}

@end
