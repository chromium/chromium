// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_promo_scene_agent.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/post_default_abandonment/features.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

@interface DefaultBrowserPromoSceneAgent ()

// YES if the main profile for this scene is signed in.
@property(nonatomic, readonly, getter=isSignedIn) BOOL signedIn;

// The feature engagement tracker for self, if it exists.
@property(nonatomic, readonly)
    feature_engagement::Tracker* featureEngagementTracker;

@end

@implementation DefaultBrowserPromoSceneAgent {
  // Indicates whether the user has already seen the post restore default
  // browser promo in the current app session.
  BOOL _postRestorePromoSeenInCurrentSession;
}

#pragma mark - Private

// Registers the post restore default browser promo if the user is eligible.
// Otherwise, deregisters. To be eligible, they must be in the first session
// after an iOS restore and have previously set Chrome as their default browser.
- (void)updatePostRestorePromoRegistration {
  if (!_postRestorePromoSeenInCurrentSession &&
      IsPostRestoreDefaultBrowserEligibleUser()) {
      self.promosManager->RegisterPromoForSingleDisplay(
          promos_manager::Promo::PostRestoreDefaultBrowserAlert);
      _postRestorePromoSeenInCurrentSession = YES;
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

// Register Generic Default Browser promo and otherwise, deregister.
- (void)updateGenericPromoRegistration {
  if (!IsChromeLikelyDefaultBrowser()) {
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::DefaultBrowser);
  } else {
    self.promosManager->DeregisterPromo(promos_manager::Promo::DefaultBrowser);
  }
}

// Signed in users are eligible for generic default browser promo. Notify FET if
// user is currently signed in.
- (void)notifyFETSigninStatus {
  if (!self.isSignedIn) {
    return;
  }

  if (feature_engagement::Tracker* tracker = self.featureEngagementTracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
  }
}

- (void)maybeSetTriggerCriteriaExperimentStartTimestamp {
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled() &&
      !HasTriggerCriteriaExperimentStarted()) {
    SetTriggerCriteriaExperimentStartTimestamp();
  }
}

- (void)maybeNotifyFETTriggerCriteriaExperimentConditionMet {
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled() &&
      HasTriggerCriteriaExperimentStarted21days()) {
    if (feature_engagement::Tracker* tracker = self.featureEngagementTracker) {
      tracker->NotifyEvent(
          feature_engagement::events::
              kDefaultBrowserPromoTriggerCriteriaConditionsMet);
    }
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  DCHECK(self.promosManager);

  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return;
  }

  if (level == SceneActivationLevelForegroundActive) {
    [self updatePostRestorePromoRegistration];
    [self updatePostDefaultAbandonmentPromoRegistration];
    [self updateAllTabsPromoRegistration];
    [self updateMadeForIOSPromoRegistration];
    [self updateStaySafePromoRegistration];
    [self updateGenericPromoRegistration];

    [self notifyFETSigninStatus];
    [self maybeSetTriggerCriteriaExperimentStartTimestamp];
    [self maybeNotifyFETTriggerCriteriaExperimentConditionMet];
  }
}

#pragma mark - Private properties

- (BOOL)isSignedIn {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return NO;
  }

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  DCHECK(authenticationService);
  DCHECK(authenticationService->initialized());
  return authenticationService->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin);
}

- (feature_engagement::Tracker*)featureEngagementTracker {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return nullptr;
  }

  return feature_engagement::TrackerFactory::GetForProfile(profile);
}

@end
