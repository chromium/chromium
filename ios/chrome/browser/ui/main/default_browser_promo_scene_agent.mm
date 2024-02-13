// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/version.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/post_default_abandonment/features.h"
#import "ios/chrome/browser/ui/default_promo/post_restore/features.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"

@interface DefaultBrowserPromoSceneAgent ()

// Indicates whether the user has already seen the post restore default browser
// promo in the current app session.
@property(nonatomic, assign) BOOL postRestorePromoSeenInCurrentSession;

// YES if the main profile for this scene is signed in.
@property(nonatomic, readonly, getter=isSignedIn) BOOL signedIn;

@end

@implementation DefaultBrowserPromoSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

#pragma mark - Private

// Registers the post restore default browser promo if the user is eligible.
// Otherwise, deregisters. To be eligible, they must be in the first session
// after an iOS restore and have previously set Chrome as their default browser.
- (void)updatePostRestorePromoRegistration {
  if (!_postRestorePromoSeenInCurrentSession &&
      IsPostRestoreDefaultBrowserEligibleUser()) {
    // TODO(crbug.com/1453786): register other variations.
    if (GetPostRestoreDefaultBrowserPromoType() ==
        PostRestoreDefaultBrowserPromoType::kAlert) {
      self.promosManager->RegisterPromoForSingleDisplay(
          promos_manager::Promo::PostRestoreDefaultBrowserAlert);
      _postRestorePromoSeenInCurrentSession = YES;
    }
  } else {
    self.promosManager->DeregisterPromo(
        promos_manager::Promo::PostRestoreDefaultBrowserAlert);
  }
}

- (void)updatePostDefaultAbandonmentPromoRegistration {
  if (IsEligibleForPostDefaultAbandonmentPromo()) {
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::PostDefaultAbandonment);
  } else {
    self.promosManager->DeregisterPromo(
        promos_manager::Promo::PostDefaultAbandonment);
  }
}

// Register All Tabs Default Browser promo if eligible and otherwise,
// deregister.
- (void)updateAllTabsPromoRegistration {
  if (!IsChromeLikelyDefaultBrowser() && self.isSignedIn) {
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::AllTabsDefaultBrowser);
  } else {
    self.promosManager->DeregisterPromo(
        promos_manager::Promo::AllTabsDefaultBrowser);
  }
}

// Register Made for iOS Default Browser promo and otherwise, deregister.
- (void)updateMadeForIOSPromoRegistration {
  if (!IsChromeLikelyDefaultBrowser()) {
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::MadeForIOSDefaultBrowser);
  } else {
    self.promosManager->DeregisterPromo(
        promos_manager::Promo::MadeForIOSDefaultBrowser);
  }
}

// Register Stay Safe Default Browser promo and otherwise, deregister.
- (void)updateStaySafePromoRegistration {
  if (!IsChromeLikelyDefaultBrowser()) {
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::StaySafeDefaultBrowser);
  } else {
    self.promosManager->DeregisterPromo(
        promos_manager::Promo::StaySafeDefaultBrowser);
  }
}

- (BOOL)isSignedIn {
  ChromeBrowserState* browserState =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetBrowserState();

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  DCHECK(authenticationService);
  DCHECK(authenticationService->initialized());
  return authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin);
}

#pragma mark - BaseDefaultBrowserPromoSchedulerSceneAgent

- (bool)promoCanBeDisplayed {
  return ShouldRegisterPromoWithPromoManager(self.signedIn,
                                             /*is_omnibox_copy_paste=*/true);
}

- (void)resetPromoHandler {
}

- (void)initPromoHandler:(Browser*)browser {
}

- (void)notifyHandlerShowPromo {
  PromosManagerSceneAgent* promosManagerSceneAgent =
      [PromosManagerSceneAgent agentFromScene:self.sceneState];
  if (!promosManagerSceneAgent) {
    return;
  }

  self.promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowser);

  [promosManagerSceneAgent maybeForceDisplayPromo];
}

- (void)notifyHandlerDismissPromo:(BOOL)animated {
}

- (void)onEnteringBackground:(PromoReason)currentPromoReason
              promoIsShowing:(bool)promoIsShowing {
}

- (void)onEnteringForeground {
  DCHECK(self.promosManager);

  [self updatePostRestorePromoRegistration];
  [self updatePostDefaultAbandonmentPromoRegistration];
  [self updateAllTabsPromoRegistration];
  [self updateMadeForIOSPromoRegistration];
  [self updateStaySafePromoRegistration];

  if (ShouldRegisterPromoWithPromoManager(self.signedIn,
                                          /*is_omnibox_copy_paste=*/false)) {
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::DefaultBrowser);
  } else {
    self.promosManager->DeregisterPromo(promos_manager::Promo::DefaultBrowser);
  }
}

- (void)logPromoAppear:(PromoReason)currentPromoReason {
}

- (void)logPromoAction:(PromoReason)currentPromoReason
        promoShownTime:(base::TimeTicks)promoShownTime {
}

- (void)logPromoUserDismiss:(PromoReason)currentPromoReason
             promoShownTime:(base::TimeTicks)promoShownTime {
}

- (void)logPromoTimeout:(PromoReason)currentPromoReason
         promoShownTime:(base::TimeTicks)promoShownTime {
}

@end
