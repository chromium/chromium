// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"

#import "base/notreached.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultBrowserPromoManager ()

// Coordinator for the video default browser promo.
@property(nonatomic, strong)
    VideoDefaultBrowserPromoCoordinator* videoDefaultPromoCoordinator;

@end

@implementation DefaultBrowserPromoManager

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  PrefService* prefService = browserState->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  if (IsUserPolicyNotificationNeeded(authService, prefService)) {
    // Showing the User Policy notification has priority over showing the
    // default browser promo. Both dialogs are competing for the same time slot
    // which is after the browser startup and the browser UI is initialized.
    [self stop];
    return;
  }

  // Don't show the default browser promo if the user is in the default browser
  // blue dot experiment.
  // TODO(crbug.com/1410229) clean-up experiment code when fully launched.
  if (!AreDefaultBrowserPromosEnabled()) {
    [self stop];
    return;
  }

  // Bypass the all of the triggering criteria if enabled.
  if (ShouldForceDefaultPromoType()) {
    [self showPromo:ForceDefaultPromoType()];
    return;
  }

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);

  // Video promo takes priority over other default browser promos.
  if (IsDefaultBrowserVideoPromoEnabled() && tracker &&
      IsVideoPromoEligibleUser(tracker)) {
    if (tracker->ShouldTriggerHelpUI(
            feature_engagement::
                kIPHiOSDefaultBrowserVideoPromoTriggerFeature)) {
      [self showVideoPromo];
      return;
    }
  }

  BOOL isSignedIn = [self isSignedIn];

  // Tailored promos take priority over general promo.
  if (IsTailoredPromoEligibleUser(isSignedIn)) {
    // Should only show tailored promos
    [self showPromo:MostRecentInterestDefaultPromoType(!isSignedIn)];
    return;
  }

  [self showPromo:DefaultPromoTypeGeneral];
}

- (void)stop {
  if (self.videoDefaultPromoCoordinator) {
    [self.videoDefaultPromoCoordinator stop];
    self.videoDefaultPromoCoordinator = nil;
  }
  [self.promosUIHandler promoWasDismissed];
  [super stop];
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
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<DefaultPromoCommands> defaultPromoHandler =
      HandlerForProtocol(dispatcher, DefaultPromoCommands);

  switch (promoType) {
    case DefaultPromoTypeStaySafe:
      [defaultPromoHandler showTailoredPromoStaySafe];
      break;
    case DefaultPromoTypeMadeForIOS:
      [defaultPromoHandler showTailoredPromoMadeForIOS];
      break;
    case DefaultPromoTypeAllTabs:
      [defaultPromoHandler showTailoredPromoAllTabs];
      break;
    case DefaultPromoTypeGeneral:
      [defaultPromoHandler showDefaultBrowserFullscreenPromo];
      break;
    case DefaultPromoTypeVideo:
      [self showVideoPromo];
      break;
  }
}

- (void)showVideoPromo {
  self.videoDefaultPromoCoordinator =
      [[VideoDefaultBrowserPromoCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
  [self.videoDefaultPromoCoordinator start];
}

@end
