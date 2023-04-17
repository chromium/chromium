// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"

#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  // Show the Default Browser promo UI if the user's past behavior fits
  // the categorization of potentially interested users or if the user is
  // signed in. Do not show if it is determined that Chrome is already the
  // default browser (checked in the if enclosing this comment) or if the user
  // has already seen the promo UI. If the user was in the experiment group
  // that showed the Remind Me Later button and tapped on it, then show the
  // promo again if now is the right time.

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  id<DefaultPromoCommands> defaultPromoHandler =
      HandlerForProtocol(dispatcher, DefaultPromoCommands);

  BOOL isSignedIn = [self isSignedIn];

  // Tailored promos take priority over general promo.
  if (IsTailoredPromoEligibleUser(isSignedIn)) {
    switch (MostRecentInterestDefaultPromoType(!isSignedIn)) {
      case DefaultPromoTypeGeneral:
        NOTREACHED();
        break;
      case DefaultPromoTypeStaySafe:
        [defaultPromoHandler showTailoredPromoStaySafe];
        break;
      case DefaultPromoTypeMadeForIOS:
        [defaultPromoHandler showTailoredPromoMadeForIOS];
        break;
      case DefaultPromoTypeAllTabs:
        [defaultPromoHandler showTailoredPromoAllTabs];
        break;
    }
    return;
  }

  [defaultPromoHandler showDefaultBrowserFullscreenPromo];
}

- (void)stop {
  [self.promosUIHandler promoWasDismissed];
  [super stop];
}

- (BOOL)isSignedIn {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  DCHECK(authService);
  DCHECK(authService->initialized());
  return authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

@end
