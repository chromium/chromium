// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"

#import "base/notreached.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"

@interface DefaultBrowserPromoManager () <DefaultBrowserPromoCommands>

// Default browser promo command handler.
@property(nonatomic, readonly) id<DefaultBrowserPromoCommands>
    defaultBrowserPromoHandler;

// Coordinator for the video default browser promo.
@property(nonatomic, strong)
    VideoDefaultBrowserPromoCoordinator* videoDefaultPromoCoordinator;

// Coordinator that manages the generic default browser promo.
@property(nonatomic, strong)
    DefaultBrowserPromoCoordinator* genericDefaultPromoCoordinator;

// Coordinator that manages the tailored promo modals.
@property(nonatomic, strong) TailoredPromoCoordinator* tailoredPromoCoordinator;

// Tracks whether or not the Video promo FET should be dismissed.
@property(nonatomic, assign) BOOL shouldDismissVideoPromoFET;

// Feature engagement tracker reference.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

@end

@implementation DefaultBrowserPromoManager

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  PrefService* prefService = browserState->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  self.tracker = feature_engagement::TrackerFactory::GetForBrowserState(
      self.browser->GetBrowserState());

  if (IsUserPolicyNotificationNeeded(authService, prefService)) {
    // Showing the User Policy notification has priority over showing the
    // default browser promo. Both dialogs are competing for the same time slot
    // which is after the browser startup and the browser UI is initialized.
    [self hidePromo];
    return;
  }

  // Don't show the default browser promo if the user is in the default browser
  // blue dot experiment.
  // TODO(crbug.com/1410229) clean-up experiment code when fully launched.
  if (!AreDefaultBrowserPromosEnabled()) {
    [self hidePromo];
    return;
  }

  // Bypass the all of the triggering criteria if enabled.
  if (ShouldForceDefaultPromoType()) {
    [self showPromo:ForceDefaultPromoType()];
    return;
  }

  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    if (IsDefaultBrowserVideoPromoEnabled()) {
      [self showPromo:DefaultPromoTypeVideo];
      return;
    }

    [self showPromo:DefaultPromoTypeGeneral];
    return;
  }

  // Video promo takes priority over other default browser promos.
  BOOL isDBVideoPromoEnabled =
      IsDBVideoPromoHalfscreenEnabled() || IsDBVideoPromoFullscreenEnabled();
  if (isDBVideoPromoEnabled && [self maybeTriggerVideoPromoWithFET]) {
    return;
  }

  BOOL isSignedIn = [self isSignedIn];

  // Tailored promos take priority over general promo.
  if (IsTailoredPromoEligibleUser(isSignedIn)) {
    // Show the generic default browser promo when the default browser promo
    // generic and tailored train experiment is enabled with the only-generic
    // arm.
    if (IsDefaultBrowserPromoGenericTailoredTrainEnabled() &&
        IsDefaultBrowserPromoOnlyGenericArmTrain()) {
      if (IsDefaultBrowserVideoPromoEnabled()) {
        [self showPromo:DefaultPromoTypeVideo];
        return;
      }

      [self showPromo:DefaultPromoTypeGeneral];
      return;
    }

    // Should only show tailored promos
    [self showPromo:MostRecentInterestDefaultPromoType(!isSignedIn)];
    return;
  }

  // When the default browser promo generic and tailored train experiment is
  // enabled, the generic default browser promo will only be shown when the user
  // is eligible for a tailored promo.
  if (IsDefaultBrowserPromoGenericTailoredTrainEnabled()) {
    [self hidePromo];
    return;
  }

  // When the default browser video promo with generic triggering conditions is
  // enabled, the generic default btowser promo is replaced with the video
  // promo.
  BOOL isGenericPromoVideo = IsDBVideoPromoWithGenericFullscreenEnabled() ||
                             IsDBVideoPromoWithGenericHalfscreenEnabled();
  if (isGenericPromoVideo) {
    [self showPromo:DefaultPromoTypeVideo];
    return;
  }

  [self showPromo:DefaultPromoTypeGeneral];
}

- (void)stop {
  [self.videoDefaultPromoCoordinator stop];
  if (self.shouldDismissVideoPromoFET && self.tracker) {
    self.tracker->Dismissed(
        feature_engagement::kIPHiOSDefaultBrowserVideoPromoTriggerFeature);
  }
  self.videoDefaultPromoCoordinator = nil;

  [self.genericDefaultPromoCoordinator stop];
  self.genericDefaultPromoCoordinator = nil;

  [self.tailoredPromoCoordinator stop];
  self.tailoredPromoCoordinator = nil;

  [self.promosUIHandler promoWasDismissed];
  self.promosUIHandler = nil;

  [super stop];
}

#pragma mark - DefaultBrowserPromoCommands

- (void)hidePromo {
  id<DefaultBrowserPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserPromoCommands);
  [handler hidePromo];
}

#pragma mark - public

+ (void)showPromoForTesting:(DefaultPromoType)promoType {
}

#pragma mark - private

- (BOOL)isSignedIn {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  DCHECK(authService);
  DCHECK(authService->initialized());
  return authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

- (void)showPromo:(DefaultPromoType)promoType {
  switch (promoType) {
    case DefaultPromoTypeStaySafe:
      [self showTailoredPromoWithType:DefaultPromoTypeStaySafe];
      break;
    case DefaultPromoTypeMadeForIOS:
      [self showTailoredPromoWithType:DefaultPromoTypeMadeForIOS];
      break;
    case DefaultPromoTypeAllTabs:
      [self showTailoredPromoWithType:DefaultPromoTypeAllTabs];
      break;
    case DefaultPromoTypeGeneral:
      [self showGenericPromo];
      break;
    case DefaultPromoTypeVideo:
      [self showVideoPromo];
      break;
  }

  // Used for testing only.
  [DefaultBrowserPromoManager showPromoForTesting:promoType];
}

- (void)showVideoPromo {
  self.videoDefaultPromoCoordinator =
      [[VideoDefaultBrowserPromoCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
  self.videoDefaultPromoCoordinator.handler = self;
  self.videoDefaultPromoCoordinator.isHalfScreen =
      IsDBVideoPromoHalfscreenEnabled() ||
      IsDBVideoPromoWithGenericHalfscreenEnabled();
  [self.videoDefaultPromoCoordinator start];
}

- (void)showGenericPromo {
  self.genericDefaultPromoCoordinator = [[DefaultBrowserPromoCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  self.genericDefaultPromoCoordinator.handler = self;
  [self.genericDefaultPromoCoordinator start];
}

- (void)showTailoredPromoWithType:(DefaultPromoType)type {
  DCHECK(!self.tailoredPromoCoordinator);
  self.tailoredPromoCoordinator = [[TailoredPromoCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                            type:type];
  self.tailoredPromoCoordinator.handler = self;
  [self.tailoredPromoCoordinator start];
}

- (BOOL)maybeTriggerVideoPromoWithFET {
  if (self.tracker && IsVideoPromoEligibleUser(self.tracker)) {
    if (self.tracker->ShouldTriggerHelpUI(
            feature_engagement::
                kIPHiOSDefaultBrowserVideoPromoTriggerFeature)) {
      self.shouldDismissVideoPromoFET = true;
      [self showPromo:DefaultPromoTypeVideo];
      return true;
    }
  }
  return false;
}

@end
