// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_scene_agent.h"

#import "base/json/values_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "components/version_info/channel.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/app_store_rating/constants.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/common/channel_info.h"

@interface AppStoreRatingSceneAgent ()

// Determines whether the user has used Chrome for at least 3
// different days within the past 7 days for stable channel.
// In Canary and Dev channels, the requirement is at least 1 day
// in the past 7 days.
@property(nonatomic, assign, readonly, getter=isDaysInPastWeekRequirementMet)
    BOOL daysInPastWeekRequirementMet;

// Determines whether the user has used Chrome for at least 15
// different days overall for stable channel. In Canary and Dev channels,
// the requirement is at least 1 day.
@property(nonatomic, assign, readonly, getter=isTotalDaysRequirementMet)
    BOOL totalDaysRequirementMet;

// Determines whether the user has enabled the Credentials
// Provider Extension.
@property(nonatomic, assign, readonly, getter=isCPEEnabled) BOOL CPEEnabled;

// The PromosManager is used to register promos.
@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation AppStoreRatingSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  if (self = [super init]) {
    _promosManager = promosManager;
  }
  return self;
}

- (BOOL)isUserEngaged {
  if (IsAppStoreRatingLoosenedTriggersEnabled()) {
    return IsChromeLikelyDefaultBrowser() || self.CPEEnabled;
  }
  return IsChromeLikelyDefaultBrowser() && self.daysInPastWeekRequirementMet &&
         self.totalDaysRequirementMet && self.CPEEnabled;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    [self updateUserDefaults];
    BOOL isUserEngaged = [self isUserEngaged];
    base::UmaHistogramBoolean("IOS.AppStoreRating.UserIsEligible",
                              isUserEngaged);
    if (isUserEngaged && [self promoShownOver365DaysAgo]) {
      [self requestPromoDisplay];
    }
  }
}

#pragma mark - Getters

- (BOOL)isDaysInPastWeekRequirementMet {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  const int activeDaysInPastWeek =
      prefService->GetList(kAppStoreRatingActiveDaysInPastWeekKey).size();
  const int appStoreRatingTotalDaysOnChromeRequirement =
      (GetChannel() == version_info::Channel::DEV ||
       GetChannel() == version_info::Channel::CANARY)
          ? 1
          : 3;
  return activeDaysInPastWeek >= appStoreRatingTotalDaysOnChromeRequirement;
}

- (BOOL)isTotalDaysRequirementMet {
  const int appStoreRatingDaysOnChromeInPastWeekRequirement =
      (GetChannel() == version_info::Channel::DEV ||
       GetChannel() == version_info::Channel::CANARY)
          ? 1
          : 15;

  return GetApplicationContext()->GetLocalState()->GetInteger(
             kAppStoreRatingTotalDaysOnChromeKey) >=
         appStoreRatingDaysOnChromeInPastWeekRequirement;
}

- (BOOL)isCPEEnabled {
  DCHECK(self.sceneState.browserProviderInterface.mainBrowserProvider.browser);
  PrefService* pref_service =
      self.sceneState.browserProviderInterface.mainBrowserProvider.browser
          ->GetBrowserState()
          ->GetPrefs();
  return password_manager_util::IsCredentialProviderEnabledOnStartup(
      pref_service);
}

#pragma mark - Private

// Calls the PromosManager to request iOS displays the
// App Store Rating prompt to the user.
- (void)requestPromoDisplay {
  if (!_promosManager || !GetApplicationContext()->GetLocalState()->GetBoolean(
                             prefs::kAppStoreRatingPolicyEnabled)) {
    return;
  }
  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::AppStoreRating);
  [self recordPromoRequested];
}

// Returns an array of user's active days in the past week, not including the
// current session.
- (std::vector<base::Time>)activeDaysInPastWeek {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  const base::Value::List& storedActiveDaysInPastWeek =
      prefService->GetList(kAppStoreRatingActiveDaysInPastWeekKey);
  std::vector<base::Time> activeDaysInPastWeek;
  base::Time midnightToday = base::Time::Now().UTCMidnight();
  for (const base::Value& storedDate : storedActiveDaysInPastWeek) {
    base::Time date = ValueToTime(storedDate)->UTCMidnight();
    if (midnightToday - date < base::Days(7)) {
      activeDaysInPastWeek.push_back(date.UTCMidnight());
    }
  }

  return activeDaysInPastWeek;
}

// Stores array of user's active days in the past week to
// `kAppStoreRatingActiveDaysInPastWeekKey` in ApplicationContext.
- (void)storeActiveDaysInPastWeek:
    (const std::vector<base::Time>&)activeDaysInPastWeek {
  base::Value::List datesToStore;
  for (base::Time date : activeDaysInPastWeek) {
    datesToStore.Append(TimeToValue(date));
  }

  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->SetList(kAppStoreRatingActiveDaysInPastWeekKey,
                       std::move(datesToStore));
}

// Updates `kAppStoreRatingTotalDaysOnChromeKey` and
// `kAppStoreRatingActiveDaysInPastWeekKey`. This method is
// destructive and may modify `kAppStoreRatingActiveDaysInPastWeekKey`.
- (void)updateUserDefaults {
  std::vector<base::Time> activeDaysInPastWeek = [self activeDaysInPastWeek];

  // Check if today has been recorded. If not, record today.
  base::Time today = base::Time::Now().UTCMidnight();
  BOOL isTodayRecorded = !activeDaysInPastWeek.empty() &&
                         today - activeDaysInPastWeek.back() < base::Days(1);

  if (isTodayRecorded) {
    return;
  }

  activeDaysInPastWeek.push_back(today);
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  const int totalDaysOnChrome =
      prefService->GetInteger(kAppStoreRatingTotalDaysOnChromeKey) + 1;
  prefService->SetInteger(kAppStoreRatingTotalDaysOnChromeKey,
                          totalDaysOnChrome);

  [self storeActiveDaysInPastWeek:activeDaysInPastWeek];
}

// Called when promo is registered with promos manager. Saves today's date in
// ApplicationContext.
- (void)recordPromoRequested {
  base::Time today = base::Time::Now().UTCMidnight();
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  prefService->SetTime(kAppStoreRatingLastShownPromoDayKey, today);
}

// Checks if the the promo was already requested for the user within the past
// 365 days.
- (BOOL)promoShownOver365DaysAgo {
  PrefService* prefService = GetApplicationContext()->GetLocalState();
  base::Time lastShown =
      prefService->GetTime(kAppStoreRatingLastShownPromoDayKey);
  if (lastShown == base::Time()) {
    return YES;
  }
  base::TimeDelta daysSincePromoLastShown =
      base::Time::Now().UTCMidnight() - lastShown;
  return daysSincePromoLastShown > base::Days(365);
}
@end
