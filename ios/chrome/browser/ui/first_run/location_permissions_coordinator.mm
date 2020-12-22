// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/location_permissions_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"
#import "ios/chrome/browser/ui/first_run/location_permissions_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Histogram enum values for the user engagement of this modal.
enum class LocationPermissionsFirstRunModalIOSEnum {
  // The primary action button was tapped. The system prompt is shown.
  kLocationPromptShown = 0,
  // The First Run location permissions modal was dismissed.
  kDismissed = 1,
  // kMaxValue should share the value of the highest enumerator.
  kMaxValue = kDismissed,
};

}

@interface LocationPermissionsCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

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
  self.locationPermissionsViewController.presentationController.delegate = self;
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

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self logModalInteractionForAction:LocationPermissionsFirstRunModalIOSEnum::
                                         kDismissed];
  [[OmniboxGeolocationController sharedInstance] systemPromptSkippedForNewUser];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self logModalInteractionForAction:LocationPermissionsFirstRunModalIOSEnum::
                                         kLocationPromptShown];
  [self.handler dismissLocationPermissionsExplanationModal];
  [[OmniboxGeolocationController sharedInstance]
      triggerSystemPromptForNewUser:YES];
}

- (void)confirmationAlertSecondaryAction {
  [self logModalInteractionForAction:LocationPermissionsFirstRunModalIOSEnum::
                                         kDismissed];
  [self.handler dismissLocationPermissionsExplanationModal];
  [[OmniboxGeolocationController sharedInstance] systemPromptSkippedForNewUser];
}

- (void)confirmationAlertDismissAction {
  // No-op.
}

- (void)confirmationAlertLearnMoreAction {
  // No-op.
}

#pragma mark - Private

- (void)logModalInteractionForAction:
    (LocationPermissionsFirstRunModalIOSEnum)action {
  base::UmaHistogramEnumeration(
      "IOS.LocationPermissions.FirstRunModal.Interaction", action);
}

@end
