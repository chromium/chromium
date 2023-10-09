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
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_mediator.h"
#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller.h"

@interface ParcelTrackingOptInCoordinator () <
    UIAdaptivePresentationControllerDelegate>

@end

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
  _viewController.actionHandler = _viewController;
  _viewController.delegate = self;
  _viewController.presentationController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  base::UmaHistogramBoolean(parcel_tracking::kOptInPromptDisplayedHistogramName,
                            true);
}

- (void)stop {
  [self dismissPrompt];
  _mediator = nil;
  _viewController = nil;
  [super stop];
}

#pragma mark - ParcelTrackingOptInViewControllerDelegate

- (void)alwaysTrackTapped {
  [self dismissPrompt];
  PrefService* prefs = self.browser->GetBrowserState()->GetPrefs();
  prefs->SetBoolean(prefs::kIosParcelTrackingOptInPromptDisplayLimitMet, true);
  prefs->SetInteger(
      prefs::kIosParcelTrackingOptInStatus,
      static_cast<int>(IOSParcelTrackingOptInStatus::kAlwaysTrack));
  [_mediator didTapAlwaysTrack:_parcels];
  base::UmaHistogramEnumeration(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kAlwaysTrack);
}

- (void)askToTrackTapped {
  [self dismissPrompt];
  PrefService* prefs = self.browser->GetBrowserState()->GetPrefs();
  prefs->SetBoolean(prefs::kIosParcelTrackingOptInPromptDisplayLimitMet, true);
  prefs->SetInteger(
      prefs::kIosParcelTrackingOptInStatus,
      static_cast<int>(IOSParcelTrackingOptInStatus::kAskToTrack));
  [_mediator didTapAskToTrack:_parcels];
  base::UmaHistogramEnumeration(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kAskEveryTime);
}

- (void)noThanksTapped {
  [self dismissPrompt];
  PrefService* prefs = self.browser->GetBrowserState()->GetPrefs();
  prefs->SetBoolean(prefs::kIosParcelTrackingOptInPromptDisplayLimitMet, true);
  prefs->SetInteger(
      prefs::kIosParcelTrackingOptInStatus,
      static_cast<int>(IOSParcelTrackingOptInStatus::kNeverTrack));
  base::UmaHistogramEnumeration(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kNoThanks);
}

- (void)parcelTrackingSettingsPageLinkTapped {
  [self dismissPrompt];
  self.browser->GetBrowserState()->GetPrefs()->SetBoolean(
      prefs::kIosParcelTrackingOptInPromptDisplayLimitMet, true);
  id<ApplicationSettingsCommands> settingsCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationSettingsCommands);
  [settingsCommandHandler
      showGoogleServicesSettingsFromViewController:self.baseViewController];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // If user has swiped down on the prompt before as well, set
  // kIosParcelTrackingOptInPromptDisplayLimitMet to true to avoid showing the
  // prompt again.
  PrefService* prefs = self.browser->GetBrowserState()->GetPrefs();
  if (prefs->GetBoolean(prefs::kIosParcelTrackingOptInPromptSwipedDown)) {
    prefs->SetBoolean(prefs::kIosParcelTrackingOptInPromptDisplayLimitMet,
                      true);
    prefs->SetInteger(
        prefs::kIosParcelTrackingOptInStatus,
        static_cast<int>(IOSParcelTrackingOptInStatus::kNeverTrack));
  } else {
    prefs->SetBoolean(prefs::kIosParcelTrackingOptInPromptSwipedDown, true);
  }
  base::UmaHistogramEnumeration(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kSwipeToDismiss);
}

#pragma mark - Private

// Dismisses the view controller.
- (void)dismissPrompt {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
}

@end
