// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/parcel_tracking/metrics.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_mediator.h"
#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller.h"

@implementation ParcelTrackingOptInCoordinator {
  web::WebState* _webState;
  NSArray<CustomTextCheckingResult*>* _parcels;
  ParcelTrackingOptInMediator* _mediator;
  ParcelTrackingOptInViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                                   parcels:(NSArray<CustomTextCheckingResult*>*)
                                               parcels {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _webState = webState;
    _parcels = parcels;
  }
  return self;
}

- (void)start {
  [super start];
  _mediator = [[ParcelTrackingOptInMediator alloc] initWithWebState:_webState];
  _mediator.parcelTrackingCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ParcelTrackingOptInCommands);
  _viewController = [[ParcelTrackingOptInViewController alloc] init];
  _viewController.actionHandler = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  self.browser->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kIosParcelTrackingOptInPromptDisplayed, true);
  base::UmaHistogramBoolean(parcel_tracking::kOptInPromptDisplayedHistogramName,
                            true);
}

- (void)stop {
  [self dismissPrompt];
  _mediator = nil;
  _viewController = nil;
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self dismissPrompt];
  self.browser->GetBrowserState()->GetPrefs()->SetInteger(
      prefs::kIosParcelTrackingOptInStatus,
      static_cast<int>(IOSParcelTrackingOptInStatus::kAlwaysTrack));
  [_mediator didTapPrimaryActionButton:_parcels];
  base::UmaHistogramEnumeration(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kAlwaysTrack);
}

- (void)confirmationAlertSecondaryAction {
  [self dismissPrompt];
  self.browser->GetBrowserState()->GetPrefs()->SetInteger(
      prefs::kIosParcelTrackingOptInStatus,
      static_cast<int>(IOSParcelTrackingOptInStatus::kNeverTrack));
  base::UmaHistogramEnumeration(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kNoThanks);
}

- (void)confirmationAlertTertiaryAction {
  [self dismissPrompt];
  self.browser->GetBrowserState()->GetPrefs()->SetInteger(
      prefs::kIosParcelTrackingOptInStatus,
      static_cast<int>(IOSParcelTrackingOptInStatus::kAskToTrack));
  [_mediator didTapTertiaryActionButton:_parcels];
  base::UmaHistogramEnumeration(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kAskEveryTime);
}

#pragma mark - Private

// Dismisses the view controller.
- (void)dismissPrompt {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
}

// TODO(crbug.com/1473449): handle swipe dismiss.

@end
