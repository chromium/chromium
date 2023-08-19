// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/version.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/default_promo/post_restore/features.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_scene_agent.h"

@interface DefaultBrowserPromoSceneAgent ()

// Indicates whether the user has already seen the post restore default browser
// promo in the current app session.
@property(nonatomic, assign) BOOL postRestorePromoSeenInCurrentSession;

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

// Registers the post restore default browser promo if the user is eligible. To
// be eligible, they must be in the first session after an iOS restore and have
// previously set Chrome as their default browser.
- (void)maybeRegisterPostRestorePromo {
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

// Returns whether the user is signed in.
- (bool)isSignedIn {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.sceneState.appState.mainBrowserState);
  DCHECK(authenticationService);
  DCHECK(authenticationService->initialized());
  return authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin);
}

#pragma mark - BaseDefaultBrowserPromoSchedulerSceneAgent

- (bool)promoCanBeDisplayed {
  return ShouldRegisterPromoWithPromoManager(
      [self isSignedIn], /*is_omnibox_copy_paste=*/true,
      feature_engagement::TrackerFactory::GetForBrowserState(
          self.sceneState.appState.mainBrowserState));
}

- (void)resetPromoHandler {
}

- (void)initPromoHandler:(Browser*)browser {
}

- (void)notifyHandlerShowPromo {
  if (!IsDefaultBrowserInPromoManagerEnabled()) {
    return;
  }
  self.promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DefaultBrowser);

  [[PromosManagerSceneAgent agentFromScene:self.sceneState]
      maybeForceDisplayPromo];
}

- (void)notifyHandlerDismissPromo:(BOOL)animated {
  if (IsDefaultBrowserInPromoManagerEnabled()) {
    return;
  }
  self.promosManager->DeregisterPromo(promos_manager::Promo::DefaultBrowser);
}

- (void)onEnteringBackground:(PromoReason)currentPromoReason
              promoIsShowing:(bool)promoIsShowing {
}

- (void)onEnteringForeground {
  // Post Restore promo takes priority over other default browser promos.
  [self maybeRegisterPostRestorePromo];

  AppState* appState = self.sceneState.appState;

  // Register default browser promo manager to the promo manager.
  if (IsDefaultBrowserInPromoManagerEnabled()) {
    DCHECK(self.sceneState.appState.mainBrowserState);

    DCHECK(self.promosManager);
    if (ShouldRegisterPromoWithPromoManager(
            [self isSignedIn], /*is_omnibox_copy_paste=*/false,
            feature_engagement::TrackerFactory::GetForBrowserState(
                appState.mainBrowserState))) {
      self.promosManager->RegisterPromoForSingleDisplay(
          promos_manager::Promo::DefaultBrowser);
    } else {
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::DefaultBrowser);
    }
    return;
  }

  // Can only present UI when activation level is
  // SceneActivationLevelForegroundActive. Show the UI if user has met the
  // qualifications to be shown the promo.
  if (appState.shouldShowDefaultBrowserPromo && !appState.currentUIBlocker) {
    id<DefaultPromoCommands> defaultPromoHandler =
        HandlerForProtocol(self.dispatcher, DefaultPromoCommands);
    switch (appState.defaultBrowserPromoTypeToShow) {
      case DefaultPromoTypeGeneral:
        [defaultPromoHandler showDefaultBrowserFullscreenPromo];
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
      case DefaultPromoTypeVideo:
        break;
    }

    appState.shouldShowDefaultBrowserPromo = NO;
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
