// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_switching_promo/model/account_switching_promo_scene_agent.h"

#import <memory>

#import "base/check.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/not_fatal_until.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_util.h"
#import "ios/chrome/browser/signin/model/account_preview_data_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {

// Returns whether `AccountSwitchingPromoSceneAgent` should be created for
// `profile`, i.e. whether `profile` is the personal profile, account preview
// data service is available, and `kEnableAccountPreviewSwitchingAccount` is
// enabled.
bool ShouldCreateAccountSwitchingPromoSceneAgent(ProfileIOS* profile) {
  CHECK(profile, base::NotFatalUntil::M170);
  if (!profile || !IsPersonalProfile(profile)) {
    return false;
  }
  if (!AccountPreviewDataServiceFactory::GetForProfile(profile)) {
    return false;
  }
  return base::FeatureList::IsEnabled(
      switches::kEnableAccountPreviewSwitchingAccount);
}

}  // namespace

@interface AccountSwitchingPromoSceneAgent () <IdentityManagerObserving>
@end

@implementation AccountSwitchingPromoSceneAgent {
  raw_ptr<feature_engagement::Tracker> _featureEngagementTracker;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}

+ (instancetype)agentWithProfile:(ProfileIOS*)profile {
  if (!ShouldCreateAccountSwitchingPromoSceneAgent(profile)) {
    return nil;
  }
  return [[self alloc]
       initWithIdentityManager:IdentityManagerFactory::GetForProfile(profile)
      featureEngagementTracker:feature_engagement::TrackerFactory::
                                   GetForProfile(profile)];
}

// Initializes the scene agent.
// `identityManager`: Identity manager used to observe sign-in changes. Must not
// be null.
// `featureEngagementTracker`: Tracker notified when the primary account
// changes. Must not be null.
- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
               featureEngagementTracker:
                   (feature_engagement::Tracker*)featureEngagementTracker {
  self = [super init];
  if (self) {
    CHECK(identityManager, base::NotFatalUntil::M170);
    CHECK(featureEngagementTracker, base::NotFatalUntil::M170);
    if (!identityManager || !featureEngagementTracker) {
      return nil;
    }

    _featureEngagementTracker = featureEngagementTracker;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  _identityManagerObserver.reset();
  _featureEngagementTracker = nullptr;
  [sceneState removeObserver:self];
}

#pragma mark - IdentityManagerObserving

- (void)primaryAccountDidChange:
    (const signin::PrimaryAccountChangeEvent&)event {
  CHECK(_featureEngagementTracker, base::NotFatalUntil::M170);
  if (!_featureEngagementTracker) {
    return;
  }
  // TODO(crbug.com/571565524): This tracks only signing in with a personal
  // account (since this agent is only created for the personal profile). Check
  // whether signing in with a managed account should also be tracked.
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    _featureEngagementTracker->NotifyEvent(
        feature_engagement::events::kIOSAccountSwitchingPrimaryAccountChanged);
  }
}

@end
