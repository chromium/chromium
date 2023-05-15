// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/version.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DefaultBrowserPromoSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Register default browser promo manager to the promo manager.
  if (IsDefaultBrowserInPromoManagerEnabled()) {
    if (level == SceneActivationLevelForegroundActive) {
      DCHECK(sceneState.appState.mainBrowserState);
      AuthenticationService* authenticationService =
          AuthenticationServiceFactory::GetForBrowserState(
              sceneState.appState.mainBrowserState);
      DCHECK(authenticationService);
      DCHECK(authenticationService->initialized());
      BOOL isSignedIn = authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin);

      DCHECK(self.promosManager);
      if (ShouldRegisterPromoWithPromoManager(isSignedIn)) {
        self.promosManager->RegisterPromoForSingleDisplay(
            promos_manager::Promo::DefaultBrowser);
      } else {
        self.promosManager->DeregisterPromo(
            promos_manager::Promo::DefaultBrowser);
      }
    }
    return;
  }

  AppState* appState = self.sceneState.appState;
  // Can only present UI when activation level is
  // SceneActivationLevelForegroundActive. Show the UI if user has met the
  // qualifications to be shown the promo.
  if (level == SceneActivationLevelForegroundActive &&
      appState.shouldShowDefaultBrowserPromo && !appState.currentUIBlocker) {
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
    }

    appState.shouldShowDefaultBrowserPromo = NO;
  }
}

@end
