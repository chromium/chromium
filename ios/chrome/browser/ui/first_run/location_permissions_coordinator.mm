// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/location_permissions_coordinator.h"

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/ui/first_run/location_permissions_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationPermissionsCoordinator () <ConfirmationAlertActionHandler>

// The fullscreen confirmation modal promo view controller this coordiantor
// manages.
@property(nonatomic, strong)
    LocationPermissionsViewController* locationPermissionsViewController;
@end

@implementation LocationPermissionsCoordinator

#pragma mark - Public Methods.

- (void)start {
  [super start];
  self.locationPermissionsViewController =
      [[LocationPermissionsViewController alloc] init];
  self.locationPermissionsViewController.actionHandler = self;
  self.locationPermissionsViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  [self.baseViewController
      presentViewController:self.locationPermissionsViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self.locationPermissionsViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.locationPermissionsViewController = nil;
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.handler dismissLocationPermissionsExplanationModal];
  [[OmniboxGeolocationController sharedInstance]
      triggerSystemPromptForNewUser:YES];
}

- (void)confirmationAlertSecondaryAction {
  [self.handler dismissLocationPermissionsExplanationModal];
}

- (void)confirmationAlertDismissAction {
  // No-op.
}

- (void)confirmationAlertLearnMoreAction {
  // No-op.
}

@end
