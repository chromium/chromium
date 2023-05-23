// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/utils.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/settings/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

// Key in NSUserDefaults containing an NSDictionary used to store all the
// information.
extern NSString* const kDefaultBrowserUtilsKey;

namespace {

// Key in storage containing an NSDate corresponding to the last time
// an HTTP(S) link was sent and opened by the app.
NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";

// Key in storage containing an array of dates. Each date correspond to
// a general event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventGeneral = @"lastSignificantUserEvent";

// Key in storage containing an array of dates. Each date correspond to
// a stay safe event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventStaySafe =
    @"lastSignificantUserEventStaySafe";

// Key in storage containing an array of dates. Each date correspond to
// a made for iOS event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventMadeForIOS =
    @"lastSignificantUserEventMadeForIOS";

// Key in storage containing an array of dates. Each date correspond to
// an all tabs event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventAllTabs =
    @"lastSignificantUserEventAllTabs";

// Key in storage containing an NSDate indicating the last time a user
// interacted with ANY promo. The string value is kept from when the promos
// first launched to avoid changing the behavior for users that have already
// seen the promo.
NSString* const kLastTimeUserInteractedWithPromo =
    @"lastTimeUserInteractedWithFullscreenPromo";

// Key in storage containing a bool indicating if the user has
// previously interacted with a regular fullscreen promo.
NSString* const kUserHasInteractedWithFullscreenPromo =
    @"userHasInteractedWithFullscreenPromo";

// Key in storage containing a bool indicating if the user has
// previously interacted with a tailored fullscreen promo.
NSString* const kUserHasInteractedWithTailoredFullscreenPromo =
    @"userHasInteractedWithTailoredFullscreenPromo";

// Key in storage containing a bool indicating if the user has
// previously interacted with first run promo.
NSString* const kUserHasInteractedWithFirstRunPromo =
    @"userHasInteractedWithFirstRunPromo";

// Key in storage containing an int indicating the number of times the
// user has interacted with a non-modal promo.
NSString* const kUserInteractedWithNonModalPromoCount =
    @"userInteractedWithNonModalPromoCount";

// Key in storage containing an int indicating the number of times a
// promo has been displayed.
NSString* const kDisplayedPromoCount = @"displayedPromoCount";

// Key in storage containing an NSDate indicating the last time a user
// interacted with the "remind me later" panel.
NSString* const kRemindMeLaterPromoActionInteraction =
    @"remindMeLaterPromoActionInteraction";

// TODO(crbug.com/1445240): Remove in M116+.
// Key in storage containing a bool indicating if the user tapped on
// button to open settings.
NSString* const kOpenSettingsActionInteraction =
    @"openSettingsActionInteraction";

// Key in storage containing the timestamp of the last time the user opened the
// app via first-party intent.
NSString* const kTimestampAppLastOpenedViaFirstPartyIntent =
    @"TimestampAppLastOpenedViaFirstPartyIntent";

// Key in storage containing the timestamp of the last time the user pasted a
// valid URL into the omnibox.
NSString* const kTimestampLastValidURLPasted = @"TimestampLastValidURLPasted";

// Key in storage containing the timestamp of the last time the user opened the
// app via first-party intent.
NSString* const kTimestampAppLaunchOnColdStart =
    @"TimestampAppLaunchedOnColdStart";

const char kDefaultBrowserFullscreenPromoExperimentChangeStringsGroupParam[] =
    "show_switch_description";

// Maximum number of past event timestamps to record.
const size_t kMaxPastTimestampsToRecord = 10;

// Time threshold before activity timestamps should be removed.
constexpr base::TimeDelta kUserActivityTimestampExpiration = base::Days(21);

// Time threshold for the last URL open before no URL opens likely indicates
// Chrome is no longer the default browser.
constexpr base::TimeDelta kLatestURLOpenForDefaultBrowser = base::Days(21);

// Delay for the user to be reshown the fullscreen promo when the user taps on
// the "Remind Me Later" button.
constexpr base::TimeDelta kRemindMeLaterPresentationDelay = base::Hours(50);

// Cool down between fullscreen promos.
constexpr base::TimeDelta kFullscreenPromoCoolDown = base::Days(14);

// Short cool down between promos.
constexpr base::TimeDelta kPromosShortCoolDown = base::Days(3);

// Maximum time range between first-party app launches to notify the FET.
constexpr base::TimeDelta kMaximumTimeBetweenFirstPartyAppLaunches =
    base::Days(7);

// Maximum time range between app launches on cold start to notify the FET.
constexpr base::TimeDelta kMaximumTimeBetweenAppColdStartLaunches =
    base::Days(7);

// Maximum time range between valid user URL pastes to notify the FET.
constexpr base::TimeDelta kMaximumTimeBetweenValidURLPastes = base::Days(7);

// List of DefaultPromoType considered by MostRecentInterestDefaultPromoType.
const DefaultPromoType kDefaultPromoTypes[] = {
    DefaultPromoTypeStaySafe,
    DefaultPromoTypeAllTabs,
    DefaultPromoTypeMadeForIOS,
};

// Creates storage object from legacy keys.
NSMutableDictionary<NSString*, NSObject*>* CreateStorageObjectFromLegacyKeys() {
  NSMutableDictionary<NSString*, NSObject*>* dictionary =
      [[NSMutableDictionary alloc] init];

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in DefaultBrowserUtilsLegacyKeysForTesting()) {
    NSObject* object = [defaults objectForKey:key];
    if (object) {
      dictionary[key] = object;
      [defaults removeObjectForKey:key];
    }
  }

  return dictionary;
}

// Helper function to get the data for `key` from the storage object.
template <typename T>
T* GetObjectFromStorageForKey(NSString* key) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSDictionary<NSString*, NSObject*>* storage =
      [defaults objectForKey:kDefaultBrowserUtilsKey];

  // If the storage is missing, create it, possibly from the legacy keys.
  // This is used to support loading data written by version 109 or ealier.
  // Remove once migrating data from such old version is no longer supported.
  if (!storage) {
    storage = CreateStorageObjectFromLegacyKeys();
    [defaults setObject:storage forKey:kDefaultBrowserUtilsKey];
  }

  DCHECK(storage);
  return base::mac::ObjCCast<T>(storage[key]);
}

// Helper function to update storage with `dict`. If a key in `dict` maps
// to `NSNull` instance, it will be removed from storage.
void UpdateStorageWithDictionary(NSDictionary<NSString*, NSObject*>* dict) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSMutableDictionary<NSString*, NSObject*>* storage =
      [[defaults objectForKey:kDefaultBrowserUtilsKey] mutableCopy];

  // If the storage is missing, create it, possibly from the legacy keys.
  // This is used to support loading data written by version 109 or ealier.
  // Remove once migrating data from such old version is no longer supported.
  if (!storage) {
    storage = CreateStorageObjectFromLegacyKeys();
  }
  DCHECK(storage);

  for (NSString* key in dict) {
    NSObject* object = dict[key];
    if (object == [NSNull null]) {
      [storage removeObjectForKey:key];
    } else {
      storage[key] = object;
    }
  }

  [defaults setObject:storage forKey:kDefaultBrowserUtilsKey];
}

// Helper function to set `data` for `key` into the storage object.
void SetObjectIntoStorageForKey(NSString* key, NSObject* data) {
  UpdateStorageWithDictionary(@{key : data});
}

// Helper function to get the storage key for a specific promo type.
NSString* StorageKeyForDefaultPromoType(DefaultPromoType type) {
  switch (type) {
    case DefaultPromoTypeGeneral:
      return kLastSignificantUserEventGeneral;
    case DefaultPromoTypeMadeForIOS:
      return kLastSignificantUserEventMadeForIOS;
    case DefaultPromoTypeAllTabs:
      return kLastSignificantUserEventAllTabs;
    case DefaultPromoTypeStaySafe:
      return kLastSignificantUserEventStaySafe;
  }
  NOTREACHED();
  return nil;
}

// Loads from NSUserDefaults the time of the last non-expired events.
std::vector<base::Time> LoadTimestampsForPromoType(DefaultPromoType type) {
  NSString* key = StorageKeyForDefaultPromoType(type);
  NSArray* dates = GetObjectFromStorageForKey<NSArray>(key);
  if (!dates) {
    return {};
  }

  std::vector<base::Time> times;
  times.reserve(dates.count);

  const base::Time now = base::Time::Now();
  for (NSObject* object : dates) {
    NSDate* date = base::mac::ObjCCast<NSDate>(object);
    if (!date) {
      continue;
    }

    const base::Time time = base::Time::FromNSDate(date);
    if (now - time > kUserActivityTimestampExpiration) {
      continue;
    }

    times.push_back(time);
  }

  return times;
}

// Stores the time of the last recorded events for `type`.
void StoreTimestampsForPromoType(DefaultPromoType type,
                                 std::vector<base::Time> times) {
  NSMutableArray<NSDate*>* dates =
      [[NSMutableArray alloc] initWithCapacity:times.size()];

  // Only record up to kMaxPastTimestampsToRecord timestamps.
  if (times.size() > kMaxPastTimestampsToRecord) {
    const size_t count_to_erase = times.size() - kMaxPastTimestampsToRecord;
    times.erase(times.begin(), times.begin() + count_to_erase);
  }

  for (base::Time time : times) {
    [dates addObject:time.ToNSDate()];
  }

  NSString* key = StorageKeyForDefaultPromoType(type);
  SetObjectIntoStorageForKey(key, dates);
}

// Returns whether an event was logged for key occuring less than `delay`
// in the past.
bool HasRecordedEventForKeyLessThanDelay(NSString* key, base::TimeDelta delay) {
  NSDate* date = GetObjectFromStorageForKey<NSDate>(key);
  if (!date) {
    return false;
  }

  const base::Time time = base::Time::FromNSDate(date);
  return base::Time::Now() - time < delay;
}

// Returns whether an event was logged for key occuring more than `delay`
// in the past.
bool HasRecordedEventForKeyMoreThanDelay(NSString* key, base::TimeDelta delay) {
  NSDate* date = GetObjectFromStorageForKey<NSDate>(key);
  if (!date) {
    return false;
  }

  const base::Time time = base::Time::FromNSDate(date);
  return base::Time::Now() - time > delay;
}

// `YES` if user interacted with the first run default browser screen.
BOOL HasUserInteractedWithFirstRunPromoBefore() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kUserHasInteractedWithFirstRunPromo);
  return number.boolValue;
}

// Returns the number of time a default browser promo has been displayed.
NSInteger DisplayedPromoCount() {
  NSNumber* number = GetObjectFromStorageForKey<NSNumber>(kDisplayedPromoCount);
  return number.integerValue;
}

// Computes cool down between promos.
base::TimeDelta ComputeCooldown() {
  // `true` if the user is in the short delay group experiment and tap on the
  // "No thanks" button in first run default browser screen. Short cool down
  // should be set only one time, so after the first run promo there is a short
  // cool down before the next promo and after it goes back to normal.
  if (DisplayedPromoCount() < 2 && HasUserInteractedWithFirstRunPromoBefore()) {
    return kPromosShortCoolDown;
  }
  return kFullscreenPromoCoolDown;
}

}  // namespace

const char kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam[] =
    "show_remind_me_later";

void LogOpenHTTPURLFromExternalURL() {
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, [NSDate date]);
}

void LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoType type) {
  std::vector<base::Time> times = LoadTimestampsForPromoType(type);
  times.push_back(base::Time::Now());

  StoreTimestampsForPromoType(type, std::move(times));
}

void LogRemindMeLaterPromoActionInteraction() {
  DCHECK(IsInRemindMeLaterGroup());
  SetObjectIntoStorageForKey(kRemindMeLaterPromoActionInteraction,
                             [NSDate date]);
}

void LogToFETDefaultBrowserPromoShown(feature_engagement::Tracker* tracker) {
  // OTR browsers can sometimes pass a null tracker, check for that here.
  if (!tracker) {
    return;
  }
  tracker->NotifyEvent(feature_engagement::events::kDefaultBrowserPromoShown);
}

void LogToFETUserPastedURLIntoOmnibox(feature_engagement::Tracker* tracker) {
  base::RecordAction(
      base::UserMetricsAction("Mobile.Omnibox.iOS.PastedValidURL"));

  // OTR browsers can sometimes pass a null tracker, check for that here.
  if (!tracker) {
    return;
  }

  if (HasRecentValidURLPastesAndRecordsCurrentPaste()) {
    tracker->NotifyEvent(feature_engagement::events::kBlueDotPromoCriterionMet);

    if (IsDefaultBrowserVideoPromoEnabled()) {
      tracker->NotifyEvent(
          feature_engagement::events::kDefaultBrowserVideoPromoConditionsMet);
    }
  }
}

bool ShouldShowRemindMeLaterDefaultBrowserFullscreenPromo() {
  if (!IsInRemindMeLaterGroup()) {
    return false;
  }

  return HasRecordedEventForKeyMoreThanDelay(
      kRemindMeLaterPromoActionInteraction, kRemindMeLaterPresentationDelay);
}

bool ShouldTriggerDefaultBrowserHighlightFeature(
    const base::Feature& feature,
    feature_engagement::Tracker* tracker,
    syncer::SyncService* syncService) {
  // TODO(crbug.com/1410229) clean-up experiment code when fully launched.
  if (!IsBlueDotPromoEnabled() || IsChromeLikelyDefaultBrowser() ||
      (syncService && ShouldIndicateIdentityErrorInOverflowMenu(syncService))) {
    return false;
  }

  // We need to ask the FET whether or not we should show this IPH because if
  // yes, this will automatically notify the other dependent FET features that
  // their criteria have been met. We then automatically dismiss it. Since it's
  // just a shadow feature to enable the other two needed for the blue dot
  // promo, we ignore `ShouldTriggerHelpUI`'s return value.
  if (tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHiOSDefaultBrowserBadgeEligibilityFeature)) {
    tracker->Dismissed(
        feature_engagement::kIPHiOSDefaultBrowserBadgeEligibilityFeature);
  }

  // Now, we ask the appropriate FET feature if it should trigger, i.e. if we
  // should show the blue dot promo badge.
  if (tracker->ShouldTriggerHelpUI(feature)) {
    tracker->Dismissed(feature);
    return true;
  }

  return false;
}

bool IsInRemindMeLaterGroup() {
  std::string paramValue = base::GetFieldTrialParamValueByFeature(
      kDefaultBrowserFullscreenPromoExperiment,
      kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam);
  return !paramValue.empty();
}

bool IsInModifiedStringsGroup() {
  std::string paramValue = base::GetFieldTrialParamValueByFeature(
      kDefaultBrowserFullscreenPromoExperiment,
      kDefaultBrowserFullscreenPromoExperimentChangeStringsGroupParam);
  return !paramValue.empty();
}

bool AreDefaultBrowserPromosEnabled() {
  if (base::FeatureList::IsEnabled(kDefaultBrowserBlueDotPromo)) {
    return kBlueDotPromoUserGroupParam.Get() ==
           BlueDotPromoUserGroup::kAllDBPromosEnabled;
  }

  return true;
}

bool IsBlueDotPromoEnabled() {
  if (base::FeatureList::IsEnabled(kDefaultBrowserBlueDotPromo)) {
    return kBlueDotPromoUserGroupParam.Get() ==
               BlueDotPromoUserGroup::kOnlyBlueDotPromoEnabled ||
           kBlueDotPromoUserGroupParam.Get() ==
               BlueDotPromoUserGroup::kAllDBPromosEnabled;
  }

  return false;
}

bool IsDefaultBrowserInPromoManagerEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserRefactoringPromoManager);
}

bool IsDefaultBrowserVideoPromoEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserVideoPromo);
}

bool IsDefaultBrowserVideoPromoFullscreenEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kDefaultBrowserVideoPromo, "default_browser_video_promo_halfscreen",
      false);
}

bool HasUserInteractedWithFullscreenPromoBefore() {
  NSNumber* number = GetObjectFromStorageForKey<NSNumber>(
      kUserHasInteractedWithFullscreenPromo);
  return number.boolValue;
}

bool HasUserInteractedWithTailoredFullscreenPromoBefore() {
  NSNumber* number = GetObjectFromStorageForKey<NSNumber>(
      kUserHasInteractedWithTailoredFullscreenPromo);
  return number.boolValue;
}

NSInteger UserInteractionWithNonModalPromoCount() {
  NSNumber* number = GetObjectFromStorageForKey<NSNumber>(
      kUserInteractedWithNonModalPromoCount);
  return number.integerValue;
}

void LogUserInteractionWithFullscreenPromo() {
  const NSInteger displayed_promo_count = DisplayedPromoCount();
  NSDictionary<NSString*, NSObject*>* update = @{
    kUserHasInteractedWithFullscreenPromo : @YES,
    kLastTimeUserInteractedWithPromo : [NSDate date],
    kDisplayedPromoCount : @(displayed_promo_count + 1),
  };

  if (IsInRemindMeLaterGroup()) {
    // Clear any possible Remind Me Later timestamp saved.
    NSMutableDictionary<NSString*, NSObject*>* copy = [update mutableCopy];
    copy[kRemindMeLaterPromoActionInteraction] = [NSNull null];
    update = copy;
  }

  UpdateStorageWithDictionary(update);
}

void LogUserInteractionWithTailoredFullscreenPromo() {
  const NSInteger displayed_promo_count = DisplayedPromoCount();
  UpdateStorageWithDictionary(@{
    kUserHasInteractedWithTailoredFullscreenPromo : @YES,
    kLastTimeUserInteractedWithPromo : [NSDate date],
    kDisplayedPromoCount : @(displayed_promo_count + 1),
  });
}

void LogUserInteractionWithNonModalPromo() {
  const NSInteger interaction_count = UserInteractionWithNonModalPromoCount();
  const NSInteger displayed_promo_count = DisplayedPromoCount();
  UpdateStorageWithDictionary(@{
    kLastTimeUserInteractedWithPromo : [NSDate date],
    kUserInteractedWithNonModalPromoCount : @(interaction_count + 1),
    kDisplayedPromoCount : @(displayed_promo_count + 1),
  });
}

void LogUserInteractionWithFirstRunPromo(BOOL openedSettings) {
  const NSInteger displayed_promo_count = DisplayedPromoCount();
  UpdateStorageWithDictionary(@{
    kUserHasInteractedWithFirstRunPromo : @YES,
    kLastTimeUserInteractedWithPromo : [NSDate date],
    kDisplayedPromoCount : @(displayed_promo_count + 1),
  });
}

bool HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch() {
  const base::TimeDelta max_session_time =
      base::Seconds(GetFeedUnseenRefreshThresholdInSeconds());

  if (HasRecordedEventForKeyLessThanDelay(
          kTimestampAppLastOpenedViaFirstPartyIntent,
          kMaximumTimeBetweenFirstPartyAppLaunches)) {
    if (HasRecordedEventForKeyMoreThanDelay(
            kTimestampAppLastOpenedViaFirstPartyIntent, max_session_time)) {
      SetObjectIntoStorageForKey(kTimestampAppLastOpenedViaFirstPartyIntent,
                                 [NSDate date]);
      return YES;
    }

    return NO;
  }

  SetObjectIntoStorageForKey(kTimestampAppLastOpenedViaFirstPartyIntent,
                             [NSDate date]);
  return NO;
}

bool HasRecentValidURLPastesAndRecordsCurrentPaste() {
  if (HasRecordedEventForKeyLessThanDelay(kTimestampLastValidURLPasted,
                                          kMaximumTimeBetweenValidURLPastes)) {
    SetObjectIntoStorageForKey(kTimestampLastValidURLPasted, [NSDate date]);
    return YES;
  }

  SetObjectIntoStorageForKey(kTimestampLastValidURLPasted, [NSDate date]);
  return NO;
}

bool HasRecentTimestampForKey(NSString* eventKey) {
  const base::TimeDelta max_session_time =
      base::Seconds(GetFeedUnseenRefreshThresholdInSeconds());

  if (HasRecordedEventForKeyLessThanDelay(eventKey, max_session_time)) {
    return YES;
  }

  SetObjectIntoStorageForKey(eventKey, [NSDate date]);
  return NO;
}

bool IsChromeLikelyDefaultBrowser7Days() {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             base::Days(7));
}

bool IsChromeLikelyDefaultBrowser() {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             kLatestURLOpenForDefaultBrowser);
}

bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType promo_type) {
  std::vector<base::Time> times = LoadTimestampsForPromoType(promo_type);
  return !times.empty();
}

DefaultPromoType MostRecentInterestDefaultPromoType(
    BOOL skip_all_tabs_promo_type) {
  DefaultPromoType most_recent_event_type = DefaultPromoTypeGeneral;
  base::Time most_recent_event_time = base::Time::Min();

  for (DefaultPromoType promo_type : kDefaultPromoTypes) {
    // Ignore DefaultPromoTypeAllTabs if the extra requirements are not met.
    if (promo_type == DefaultPromoTypeAllTabs && skip_all_tabs_promo_type) {
      continue;
    }

    std::vector<base::Time> times = LoadTimestampsForPromoType(promo_type);
    if (times.empty()) {
      continue;
    }

    const base::Time last_time_for_type = times.back();
    if (last_time_for_type >= most_recent_event_time) {
      most_recent_event_type = promo_type;
      most_recent_event_time = last_time_for_type;
    }
  }
  return most_recent_event_type;
}

bool UserInPromoCooldown() {
  return HasRecordedEventForKeyLessThanDelay(kLastTimeUserInteractedWithPromo,
                                             ComputeCooldown());
}

// Visible for testing.
NSString* const kDefaultBrowserUtilsKey = @"DefaultBrowserUtils";

// Visible for testing.
const NSArray<NSString*>* DefaultBrowserUtilsLegacyKeysForTesting() {
  NSArray<NSString*>* const keysForTesting = @[
    // clang-format off
    kLastHTTPURLOpenTime,
    kLastSignificantUserEventGeneral,
    kLastSignificantUserEventStaySafe,
    kLastSignificantUserEventMadeForIOS,
    kLastSignificantUserEventAllTabs,
    kLastTimeUserInteractedWithPromo,
    kUserHasInteractedWithFullscreenPromo,
    kUserHasInteractedWithTailoredFullscreenPromo,
    kUserHasInteractedWithFirstRunPromo,
    kUserInteractedWithNonModalPromoCount,
    kDisplayedPromoCount,
    kRemindMeLaterPromoActionInteraction,
    // clang-format on
  ];

  return keysForTesting;
}

bool HasAppLaunchedOnColdStartAndRecordsLaunch() {
  if (HasRecordedEventForKeyLessThanDelay(
          kTimestampAppLaunchOnColdStart,
          kMaximumTimeBetweenAppColdStartLaunches)) {
    SetObjectIntoStorageForKey(kTimestampAppLaunchOnColdStart, [NSDate date]);
    return YES;
  }

  // Add a new timestamp if the timestamp was never recorded or if it was
  // recorded more than the maximum time between app cold starts.
  SetObjectIntoStorageForKey(kTimestampAppLaunchOnColdStart, [NSDate date]);
  return NO;
}

bool ShouldRegisterPromoWithPromoManager(bool is_signed_in) {
  // Consider showing the default browser promo if (1) launch is not after a
  // crash, (2) chrome is not likely set as default browser, (3) the user has
  // not seen a default browser promo too recently, (4) the user is eligible
  // for either the tailored or generic default browser promo.
  return GetApplicationContext()->WasLastShutdownClean() &&
         !IsChromeLikelyDefaultBrowser() && !UserInPromoCooldown() &&
         (IsTailoredPromoEligibleUser(is_signed_in) ||
          IsGeneralPromoEligibleUser(is_signed_in));
}

bool IsTailoredPromoEligibleUser(bool is_signed_in) {
  bool is_made_for_ios_promo_eligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS);
  bool is_all_tabs_promo_eligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs) &&
      is_signed_in;
  bool is_stay_safe_promo_eligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe);
  return !HasUserInteractedWithTailoredFullscreenPromoBefore() &&
         (is_made_for_ios_promo_eligible || is_all_tabs_promo_eligible ||
          is_stay_safe_promo_eligible);
}

bool IsGeneralPromoEligibleUser(bool is_signed_in) {
  bool isGeneralPromoEligibleUser =
      !HasUserInteractedWithFullscreenPromoBefore() &&
      (IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral) ||
       is_signed_in);
  return isGeneralPromoEligibleUser ||
         ShouldShowRemindMeLaterDefaultBrowserFullscreenPromo();
}

bool IsVideoPromoEligibleUser(feature_engagement::Tracker* tracker) {
  if (!IsDefaultBrowserVideoPromoEnabled()) {
    return false;
  }

  if (!tracker ||
      !tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHiOSDefaultBrowserVideoPromoTriggerFeature)) {
    return false;
  }

  return true;
}

void CleanupUnusedStorage() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // TODO(crbug.com/1445240): Remove in M116+.
  [defaults removeObjectForKey:kOpenSettingsActionInteraction];
}
