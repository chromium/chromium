// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/ui_bundled/default_browser_promo_scene_agent.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/post_default_abandonment/features.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_prefs.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

@interface DefaultBrowserPromoSceneAgent () <ProfileStateObserver>

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

// Registers the generic default browser promo if the user is eligible.
// Otherwise, deregisters. Eligibility depends on the latest usage of the
// Reading Mode feature.
- (void)updateReaderModeRegistration {
  if (IsReaderModeAvailable() &&
      base::FeatureList::IsEnabled(kEnableReaderModeDefaultBrowserPromo)) {
    if (self.isEligibleForReaderModeDefaultBrowserPromo) {
      self.promosManager->RegisterPromoForSingleDisplay(
          promos_manager::Promo::DefaultBrowser);
      // Only for the duration of the reader mode experiment, deregister other
      // Default Browser promos.
      // TODO(crbug.com/435671056): Remove this logic as soon as the experiment
      // is over, to avoid accidentally preventing Default Browser promos to a
      // substantial portion of the user base.
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::PostRestoreDefaultBrowserAlert);
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::DefaultBrowserRemindMeLater);
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::PostDefaultAbandonment);
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::AllTabsDefaultBrowser);
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::MadeForIOSDefaultBrowser);
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::StaySafeDefaultBrowser);
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::DefaultBrowserOffCycle);
    } else {
      // TODO(crbug.com/435671056): Remove this logic as soon as the experiment
      // is over, to avoid accidentally preventing Default Browser promos to a
      // substantial portion of the user base.
      self.promosManager->DeregisterPromo(
          promos_manager::Promo::DefaultBrowser);
    }
  }
}

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

// Register or deregister Default Browser off-cycle promo.
- (void)updateOffCyclePromoRegistration {
  if (IsDefaultBrowserOffCyclePromoEnabled() &&
      !IsChromeLikelyDefaultBrowser()) {
    // The off-cycle promo replaces the generic one.
    self.promosManager->DeregisterPromo(promos_manager::Promo::DefaultBrowser);
    self.promosManager->RegisterPromoForSingleDisplay(
        promos_manager::Promo::DefaultBrowserOffCycle);
  } else {
    self.promosManager->DeregisterPromo(
        promos_manager::Promo::DefaultBrowserOffCycle);
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

- (void)checkSegmentationBeforeUpdatingGenericPromoRegistration {
  segmentation_platform::SegmentationPlatformService* segmentationService =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          self.sceneState.profileState.profile);
  segmentation_platform::PredictionOptions options;
  options.on_demand_execution = true;
  __weak DefaultBrowserPromoSceneAgent* weakSelf = self;
  segmentationService->GetClassificationResult(
      segmentation_platform::kIosDefaultBrowserPromoKey, options, nil,
      base::BindOnce(
          ^(const segmentation_platform::ClassificationResult& result) {
            // Register the generic promo if the model returned a show result or
            // if the model execution failed, since failure should not disable
            // the promo.
            if (result.status !=
                    segmentation_platform::PredictionStatus::kSucceeded ||
                result.ordered_labels[0] ==
                    segmentation_platform::kIosDefaultBrowserPromoShowLabel) {
              [weakSelf updateGenericPromoRegistration];
            } else {
              weakSelf.promosManager->DeregisterPromo(
                  promos_manager::Promo::DefaultBrowser);
            }
          }));
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];

  [self.sceneState.profileState addObserver:self];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  // Monitor the profile initialization stages to consider showing a promo at a
  // point in the initialization of the app that allows it.
  [self updatePromoRegistrationIfUIReady];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self updatePromoRegistrationIfUIReady];
  [self shareLikelyDefaultBrowserStatus];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  [self.sceneState.profileState removeObserver:self];
  [self.sceneState removeObserver:self];
}

#pragma mark - Private properties

// Shares the status of whether Chrome is likely the default browser
// with 1P apps.
- (void)shareLikelyDefaultBrowserStatus {
  if (!IsShareDefaultBrowserStatusEnabled()) {
    return;
  }

  if (self.sceneState.activationLevel != SceneActivationLevelBackground) {
    return;
  }

  NSUserDefaults* sharedDefaults = app_group::GetCommonGroupUserDefaults();

  [sharedDefaults setBool:IsChromeLikelyDefaultBrowser()
                   forKey:app_group::kChromeLikelyDefaultBrowser];
}

- (BOOL)isSignedIn {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return NO;
  }

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  DCHECK(identityManager);
  return identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

- (BOOL)isEligibleForReaderModeDefaultBrowserPromo {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return NO;
  }
  return reader_mode_prefs::IsReaderModeRecentlyUsed(*profile->GetPrefs()) &&
         !IsChromeLikelyDefaultBrowser();
}

- (feature_engagement::Tracker*)featureEngagementTracker {
  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return nullptr;
  }

  return feature_engagement::TrackerFactory::GetForProfile(profile);
}

// Registers/deregisters default browser promos if UI is ready.
- (void)updatePromoRegistrationIfUIReady {
  // Check that the profile initialization is over (the stage
  // ProfileInitStage::kFinal is reached).
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return;
  }

  //  Check that the scene is in the foreground.
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    return;
  }

  DCHECK(self.promosManager);

  [self updatePostRestorePromoRegistration];
  [self updatePostDefaultAbandonmentPromoRegistration];
  [self updateAllTabsPromoRegistration];
  [self updateMadeForIOSPromoRegistration];
  [self updateStaySafePromoRegistration];
  if (IsDefaultBrowserPromoPropensityModelEnabled()) {
    [self checkSegmentationBeforeUpdatingGenericPromoRegistration];
  } else {
    [self updateGenericPromoRegistration];
  }
  // The off-cycle promo registration must be checked after the generic promo
  // because the off-cycle promo can deregister the generic one.
  [self updateOffCyclePromoRegistration];

  // The reader-mode promo registration must happen after all other
  // registrations because it can deregister all the other fullscreen default
  // browser promo, for experiment purposes.
  [self updateReaderModeRegistration];

  [self notifyFETSigninStatus];
  [self maybeSetTriggerCriteriaExperimentStartTimestamp];
  [self maybeNotifyFETTriggerCriteriaExperimentConditionMet];
}

@end
