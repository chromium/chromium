// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/utils.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/command_line.h"
#import "base/ios/ios_util.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/signin_util.h"

// Key in NSUserDefaults containing an NSDictionary used to store all the
// information.
extern NSString* const kDefaultBrowserUtilsKey;

namespace {

// Key in storage containing an array of dates. Each date correspond to
// a general event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventGeneral = @"lastSignificantUserEvent";

// Key in storage containing an array of dates. Each date correspond to
// a made for iOS event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventMadeForIOS =
    @"lastSignificantUserEventMadeForIOS";

// Key in storage containing an array of dates. Each date correspond to
// an all tabs event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventAllTabs =
    @"lastSignificantUserEventAllTabs";


// Key in storage containing an int indicating the number of times the
// user has interacted with a non-modal promo.
NSString* const kUserInteractedWithNonModalPromoCount =
    @"userInteractedWithNonModalPromoCount";

// Action string for "Appear" event of the promo.
const char kAppearAction[] = "Appear";

// Maximum number of past event timestamps to record.
const size_t kMaxPastTimestampsToRecord = 10;

// Maximum number of past event timestamps to record for trigger criteria
// experiment.
const size_t kMaxPastTimestampsToRecordForTriggerCriteriaExperiment = 50;

// Time threshold before activity timestamps should be removed.
constexpr base::TimeDelta kUserActivityTimestampExpiration = base::Days(21);

// Time threshold for the last URL open before no URL opens likely indicates
// Chrome is no longer the default browser.
constexpr base::TimeDelta kLatestURLOpenForDefaultBrowser = base::Days(21);

// Cool down between fullscreen promos.
constexpr base::TimeDelta kFullscreenPromoCoolDown = base::Days(14);

// Short cool down between promos.
constexpr base::TimeDelta kPromosShortCoolDown = base::Days(3);

// Time threshold for default browser trigger criteria experiment statistics.
constexpr base::TimeDelta kTriggerCriteriaExperimentStatExpiration =
    base::Days(14);

// Returns maximum number of past event timestamps to record.
size_t GetMaxPastTimestampsToRecord() {
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    return kMaxPastTimestampsToRecordForTriggerCriteriaExperiment;
  }
  return kMaxPastTimestampsToRecord;
}

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
  return base::apple::ObjCCast<T>(storage[key]);
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
  NOTREACHED_IN_MIGRATION();
  return nil;
}

// Loads from NSUserDefaults the time of the non-expired events for the
// given key into the given container.
void LoadActiveDatesForKey(NSString* key,
                           base::TimeDelta delay,
                           std::set<base::Time>& dates_set) {
  NSArray* dates = GetObjectFromStorageForKey<NSArray>(key);
  if (!dates) {
    return;
  }

  const base::Time now = base::Time::Now();
  for (NSObject* object : dates) {
    NSDate* date = base::apple::ObjCCast<NSDate>(object);
    if (!date) {
      continue;
    }

    const base::Time time = base::Time::FromNSDate(date);
    if (now - time > delay) {
      continue;
    }

    dates_set.insert(time.LocalMidnight());
  }
}

// Loads from NSUserDefaults the time of the non-expired events for the
// given key.
std::vector<base::Time> LoadActiveTimestampsForKey(NSString* key,
                                                   base::TimeDelta delay) {
  NSArray* dates = GetObjectFromStorageForKey<NSArray>(key);
  if (!dates) {
    return {};
  }

  std::vector<base::Time> times;
  times.reserve(dates.count);

  const base::Time now = base::Time::Now();
  for (NSObject* object : dates) {
    NSDate* date = base::apple::ObjCCast<NSDate>(object);
    if (!date) {
      continue;
    }

    const base::Time time = base::Time::FromNSDate(date);
    if (now - time > delay) {
      continue;
    }

    times.push_back(time);
  }

  return times;
}

// Stores the time of the last recorded events for `key`.
void StoreTimestampsForKey(NSString* key, std::vector<base::Time> times) {
  NSMutableArray<NSDate*>* dates =
      [[NSMutableArray alloc] initWithCapacity:times.size()];

  // Only record up to maxPastTimestampsToRecord timestamps.
  size_t maxPastTimestampsToRecord = GetMaxPastTimestampsToRecord();
  if (times.size() > maxPastTimestampsToRecord) {
    const size_t count_to_erase = times.size() - maxPastTimestampsToRecord;
    times.erase(times.begin(), times.begin() + count_to_erase);
  }

  for (base::Time time : times) {
    [dates addObject:time.ToNSDate()];
  }

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

// Returns true if there exists a recorded interaction with a non-modal promo
// more recent than the last recorded interaction with a fullscreen promo.
bool IsLastNonModalMoreRecentThanLastFullscreen() {
  NSDate* last_non_modal_interaction = GetObjectFromStorageForKey<NSDate>(
      kLastTimeUserInteractedWithNonModalPromo);
  if (!last_non_modal_interaction) {
    return false;
  }

  NSDate* last_fullscreen_interaction = GetObjectFromStorageForKey<NSDate>(
      kLastTimeUserInteractedWithFullscreenPromo);
  if (!last_fullscreen_interaction) {
    return true;
  }

  NSComparisonResult comparison_result =
      [last_non_modal_interaction compare:last_fullscreen_interaction];

  return comparison_result == NSOrderedDescending;
}

// Copy the NSDate object in NSUserDefaults from the origin key to the
// destination key. Does nothing if the origin key is empty.
void CopyNSDateFromKeyToKey(NSString* originKey, NSString* destinationKey) {
  NSDate* origin_date = GetObjectFromStorageForKey<NSDate>(originKey);
  if (!origin_date) {
    return;
  }

  SetObjectIntoStorageForKey(destinationKey, origin_date);
}

// Returns number of events logged for key occuring less than `delay` in the
// past.
int NumRecordedEventForKeyLessThanDelay(NSString* key, base::TimeDelta delay) {
  return LoadActiveTimestampsForKey(key, delay).size();
}
// `YES` if user interacted with the first run default browser screen.
BOOL HasUserInteractedWithFirstRunPromoBefore() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kUserHasInteractedWithFirstRunPromo);
  return number.boolValue;
}

// Returns the number of time the fullscreen default browser promo has been
// displayed.
NSInteger GenericPromoInteractionCount() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kGenericPromoInteractionCount);
  return number.integerValue;
}

// Returns the number of time the tailored default browser promo has been
// displayed.
NSInteger TailoredPromoInteractionCount() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kTailoredPromoInteractionCount);
  return number.integerValue;
}

// Computes cooldown between fullscreen promos.
base::TimeDelta ComputeCooldown() {
  // `true` if the user is in the short delay group experiment and tap on the
  // "No thanks" button in first run default browser screen. Short cool down
  // should be set only one time, so after the first run promo there is a short
  // cool down before the next promo and after it goes back to normal.
  if (DisplayedFullscreenPromoCount() < 2 &&
      HasUserInteractedWithFirstRunPromoBefore()) {
    return kPromosShortCoolDown;
  }
  return kFullscreenPromoCoolDown;
}

// Returns number of days since user last interacted with one of the promos.
int NumDaysSincePromoInteraction() {
  NSDate* timestamp = GetObjectFromStorageForKey<NSDate>(
      kLastTimeUserInteractedWithFullscreenPromo);

  if (timestamp == nil) {
    return 0;
  }

  int days = (base::Time::Now() - base::Time::FromNSDate(timestamp)).InDays();
  if (days < 0) {
    return 0;
  }

  return days;
}

// Returns number of days in past `kTriggerCriteriaExperimentStatExpiration`
// days when user opened chrome.
int NumActiveDays() {
  std::set<base::Time> active_dates;

  LoadActiveDatesForKey(kAllTimestampsAppLaunchColdStart,
                        kTriggerCriteriaExperimentStatExpiration, active_dates);
  LoadActiveDatesForKey(kAllTimestampsAppLaunchWarmStart,
                        kTriggerCriteriaExperimentStatExpiration, active_dates);
  LoadActiveDatesForKey(kAllTimestampsAppLaunchIndirectStart,
                        kTriggerCriteriaExperimentStatExpiration, active_dates);
  return active_dates.size();
}

// Adds current timestamp in the array of timestamps for the given key.
void StoreCurrentTimestampForKey(NSString* key) {
  std::vector<base::Time> timestamps =
      LoadActiveTimestampsForKey(key, kTriggerCriteriaExperimentStatExpiration);
  timestamps.push_back(base::Time::Now());
  StoreTimestampsForKey(key, timestamps);
}

}  // namespace

NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";
NSString* const kLastTimeUserInteractedWithNonModalPromo =
    @"lastTimeUserInteractedWithNonModalPromo";
NSString* const kLastTimeUserInteractedWithFullscreenPromo =
    @"lastTimeUserInteractedWithFullscreenPromo";
NSString* const kAllTimestampsAppLaunchColdStart =
    @"AllTimestampsAppLaunchColdStart";
NSString* const kAllTimestampsAppLaunchWarmStart =
    @"AllTimestampsAppLaunchWarmStart";
NSString* const kAllTimestampsAppLaunchIndirectStart =
    @"AllTimestampsAppLaunchIndirectStart";
NSString* const kLastSignificantUserEventStaySafe =
    @"lastSignificantUserEventStaySafe";
NSString* const kOmniboxUseCount = @"OmniboxUseCount";
NSString* const kBookmarkUseCount = @"BookmarkUseCount";
NSString* const kAutofillUseCount = @"AutofillUseCount";
NSString* const kSpecialTabsUseCount = @"SpecialTabUseCount";

NSString* const kUserHasInteractedWithFullscreenPromo =
    @"userHasInteractedWithFullscreenPromo";
NSString* const kUserHasInteractedWithTailoredFullscreenPromo =
    @"userHasInteractedWithTailoredFullscreenPromo";
NSString* const kUserHasInteractedWithFirstRunPromo =
    @"userHasInteractedWithFirstRunPromo";
NSString* const kDisplayedFullscreenPromoCount = @"displayedPromoCount";
NSString* const kGenericPromoInteractionCount = @"genericPromoInteractionCount";
NSString* const kTailoredPromoInteractionCount =
    @"tailoredPromoInteractionCount";
constexpr base::TimeDelta kBlueDotPromoDuration = base::Days(15);
constexpr base::TimeDelta kBlueDotPromoReoccurrancePeriod = base::Days(360);

// Migration to FET keys.
NSString* const kFRETimestampMigrationDone = @"fre_timestamp_migration_done";
NSString* const kPromoInterestEventMigrationDone =
    @"promo_interest_event_migration_done";
NSString* const kPromoImpressionsMigrationDone =
    @"promo_impressions_migration_done";
NSString* const kTimestampTriggerCriteriaExperimentStarted =
    @"TimestampTriggerCriteriaExperimentStarted";

std::vector<base::Time> LoadTimestampsForPromoType(DefaultPromoType type) {
  return LoadActiveTimestampsForKey(StorageKeyForDefaultPromoType(type),
                                    kUserActivityTimestampExpiration);
}

void StoreTimestampsForPromoType(DefaultPromoType type,
                                 std::vector<base::Time> times) {
  StoreTimestampsForKey(StorageKeyForDefaultPromoType(type), times);
}

void SetObjectIntoStorageForKey(NSString* key, NSObject* data) {
  UpdateStorageWithDictionary(@{key : data});
}

void LogOpenHTTPURLFromExternalURL() {
  SetObjectIntoStorageForKey(kLastHTTPURLOpenTime, [NSDate date]);
}

void LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoType type) {
  std::vector<base::Time> times = LoadTimestampsForPromoType(type);
  times.push_back(base::Time::Now());

  StoreTimestampsForPromoType(type, std::move(times));
}

void LogToFETDefaultBrowserPromoShown(feature_engagement::Tracker* tracker) {
  // OTR browsers can sometimes pass a null tracker, check for that here.
  if (!tracker) {
    return;
  }
  tracker->NotifyEvent(feature_engagement::events::kDefaultBrowserPromoShown);
}

bool HasDefaultBrowserBlueDotDisplayTimestamp() {
  return !GetApplicationContext()
              ->GetLocalState()
              ->FindPreference(
                  prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay)
              ->IsDefaultValue();
}

void ResetDefaultBrowserBlueDotDisplayTimestampIfNeeded() {
  BOOL has_timestamp = HasDefaultBrowserBlueDotDisplayTimestamp();

  if (!has_timestamp) {
    return;
  }

  base::Time timestamp = GetApplicationContext()->GetLocalState()->GetTime(
      prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay);

  // If more than `kBlueDotPromoReoccurrancePeriod` past since previous blue
  // dot display, user should again become eligible for blue dot promo.
  if (base::Time::Now() - timestamp >= kBlueDotPromoReoccurrancePeriod) {
    GetApplicationContext()->GetLocalState()->ClearPref(
        prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay);
  }
}

void RecordDefaultBrowserBlueDotFirstDisplay() {
  if (!HasDefaultBrowserBlueDotDisplayTimestamp()) {
    GetApplicationContext()->GetLocalState()->SetTime(
        prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay, base::Time::Now());
  }
}

bool ShouldTriggerDefaultBrowserHighlightFeature(
    feature_engagement::Tracker* tracker) {
  if (IsChromeLikelyDefaultBrowser()) {
    return false;
  }

  ResetDefaultBrowserBlueDotDisplayTimestampIfNeeded();

  if (HasDefaultBrowserBlueDotDisplayTimestamp()) {
    base::Time timestamp = GetApplicationContext()->GetLocalState()->GetTime(
        prefs::kIosDefaultBrowserBlueDotPromoFirstDisplay);
    if (base::Time::Now() - timestamp >= kBlueDotPromoDuration) {
      return false;
    }
  }

  // We ask the appropriate FET feature if it should trigger, i.e. if we
  // should show the blue dot promo badge.
  if (tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature)) {
    tracker->Dismissed(
        feature_engagement::kIPHiOSDefaultBrowserOverflowMenuBadgeFeature);
    return true;
  }

  return false;
}

bool IsDefaultBrowserTriggerCriteraExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      feature_engagement::kDefaultBrowserTriggerCriteriaExperiment);
}

void SetTriggerCriteriaExperimentStartTimestamp() {
  SetObjectIntoStorageForKey(kTimestampTriggerCriteriaExperimentStarted,
                             [NSDate date]);
}

bool HasTriggerCriteriaExperimentStarted() {
  NSDate* date = GetObjectFromStorageForKey<NSDate>(
      kTimestampTriggerCriteriaExperimentStarted);
  return date != nil;
}

bool HasTriggerCriteriaExperimentStarted21days() {
  return HasRecordedEventForKeyMoreThanDelay(
      kTimestampTriggerCriteriaExperimentStarted, base::Days(21));
}

bool IsNonModalDefaultBrowserPromoCooldownRefactorEnabled() {
  return base::FeatureList::IsEnabled(
      kNonModalDefaultBrowserPromoCooldownRefactor);
}

bool HasUserInteractedWithFullscreenPromoBefore() {
  if (base::FeatureList::IsEnabled(
          feature_engagement::kDefaultBrowserEligibilitySlidingWindow)) {
    // When the total promo count is 1 it means that user has seen only the FRE
    // promo. The cooldown from FRE will be taken care of in
    // ```ComputeCooldown```. Here we only need to check the timestamp of the
    // last promo if users seen more than FRE.
    return DisplayedFullscreenPromoCount() > 1 &&
           HasRecordedEventForKeyLessThanDelay(
               kLastTimeUserInteractedWithFullscreenPromo,
               base::Days(
                   feature_engagement::
                       kDefaultBrowserEligibilitySlidingWindowParam.Get()));
  }

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

NSInteger DisplayedFullscreenPromoCount() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kDisplayedFullscreenPromoCount);
  return number.integerValue;
}

void LogFullscreenDefaultBrowserPromoDisplayed() {
  const NSInteger displayed_promo_count = DisplayedFullscreenPromoCount();
  NSDictionary<NSString*, NSObject*>* update = @{
    kDisplayedFullscreenPromoCount : @(displayed_promo_count + 1),
  };

  UpdateStorageWithDictionary(update);
}

void LogUserInteractionWithFullscreenPromo() {
  const NSInteger generic_promo_interaction_count =
      GenericPromoInteractionCount();
  NSDictionary<NSString*, NSObject*>* update = @{
    kUserHasInteractedWithFullscreenPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
    kGenericPromoInteractionCount : @(generic_promo_interaction_count + 1),
  };

  UpdateStorageWithDictionary(update);
}

void LogUserInteractionWithTailoredFullscreenPromo() {
  const NSInteger tailored_promo_interaction_count =
      TailoredPromoInteractionCount();
  UpdateStorageWithDictionary(@{
    kUserHasInteractedWithTailoredFullscreenPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
    kTailoredPromoInteractionCount : @(tailored_promo_interaction_count + 1),
  });
}

void LogUserInteractionWithNonModalPromo(
    NSInteger currentNonModalPromoInteractionsCount,
    NSInteger currentFullscreenPromoInteractionsCount) {
  if (IsNonModalDefaultBrowserPromoCooldownRefactorEnabled()) {
    UpdateStorageWithDictionary(@{
      kLastTimeUserInteractedWithNonModalPromo : [NSDate date],
      kUserInteractedWithNonModalPromoCount :
          @(currentNonModalPromoInteractionsCount + 1),
    });
  } else {
    UpdateStorageWithDictionary(@{
      kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
      kUserInteractedWithNonModalPromoCount :
          @(currentNonModalPromoInteractionsCount + 1),
      kDisplayedFullscreenPromoCount :
          @(currentFullscreenPromoInteractionsCount + 1),
    });
  }
}

void LogUserInteractionWithFirstRunPromo() {
  const NSInteger displayed_promo_count = DisplayedFullscreenPromoCount();
  UpdateStorageWithDictionary(@{
    kUserHasInteractedWithFirstRunPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
    kDisplayedFullscreenPromoCount : @(displayed_promo_count + 1),
  });
}

void CleanupStorageForTriggerExperiment() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  [defaults removeObjectForKey:kAllTimestampsAppLaunchColdStart];
  [defaults removeObjectForKey:kAllTimestampsAppLaunchWarmStart];
  [defaults removeObjectForKey:kAllTimestampsAppLaunchIndirectStart];
  [defaults removeObjectForKey:kAutofillUseCount];
  [defaults removeObjectForKey:kSpecialTabsUseCount];
  [defaults removeObjectForKey:kOmniboxUseCount];
}

void LogCopyPasteInOmniboxForCriteriaExperiment() {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    CleanupStorageForTriggerExperiment();
    return;
  }
  StoreCurrentTimestampForKey(kOmniboxUseCount);
}

void LogBookmarkUseForCriteriaExperiment() {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    CleanupStorageForTriggerExperiment();
    return;
  }

  StoreCurrentTimestampForKey(kBookmarkUseCount);
}

void LogAutofillUseForCriteriaExperiment() {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    CleanupStorageForTriggerExperiment();
    return;
  }
  StoreCurrentTimestampForKey(kAutofillUseCount);
}

void LogRemoteTabsUseForCriteriaExperiment() {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    CleanupStorageForTriggerExperiment();
    return;
  }

  StoreCurrentTimestampForKey(kSpecialTabsUseCount);
}

bool IsChromeLikelyDefaultBrowserXDays(int days) {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             base::Days(days));
}

bool IsChromeLikelyDefaultBrowser() {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             kLatestURLOpenForDefaultBrowser);
}

bool IsChromeLikelyDefaultBrowser7Days() {
  return HasRecordedEventForKeyLessThanDelay(kLastHTTPURLOpenTime,
                                             base::Days(7));
}

bool IsChromePotentiallyNoLongerDefaultBrowser(int likelyDefaultInterval,
                                               int likelyNotDefaultInterval) {
  bool wasLikelyDefaultBrowser =
      IsChromeLikelyDefaultBrowserXDays(likelyDefaultInterval);
  bool isStillLikelyDefaultBrowser =
      IsChromeLikelyDefaultBrowserXDays(likelyNotDefaultInterval);
  return wasLikelyDefaultBrowser && !isStillLikelyDefaultBrowser;
}

bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType promo_type) {
  std::vector<base::Time> times = LoadTimestampsForPromoType(promo_type);
  return !times.empty();
}

bool UserInFullscreenPromoCooldown() {
  // Sets the last fullscreen promo interaction to the same value as the last
  // non-modal promo interaction if the latter is more recent. This is
  // to allow a smooth transition back from the cooldown period separation
  // between the two promo types, if a rollback is needed.
  if (!IsNonModalDefaultBrowserPromoCooldownRefactorEnabled() &&
      IsLastNonModalMoreRecentThanLastFullscreen()) {
    CopyNSDateFromKeyToKey(kLastTimeUserInteractedWithNonModalPromo,
                           kLastTimeUserInteractedWithFullscreenPromo);
  }

  return HasRecordedEventForKeyLessThanDelay(
      kLastTimeUserInteractedWithFullscreenPromo, ComputeCooldown());
}

bool UserInNonModalPromoCooldown() {
  NSDate* last_interaction = GetObjectFromStorageForKey<NSDate>(
      kLastTimeUserInteractedWithNonModalPromo);

  // Sets the last non-modal promo interaction to the same value as last
  // fullscreen promo interaction if no non-modal interaction is found. This is
  // to allow a smooth transition to the cooldown period separation between the
  // two promo types.
  if (!last_interaction) {
    CopyNSDateFromKeyToKey(kLastTimeUserInteractedWithFullscreenPromo,
                           kLastTimeUserInteractedWithNonModalPromo);
  }

  return HasRecordedEventForKeyLessThanDelay(
      kLastTimeUserInteractedWithNonModalPromo,
      base::Days(kNonModalDefaultBrowserPromoCooldownRefactorParam.Get()));
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
    kLastTimeUserInteractedWithFullscreenPromo,
    kLastTimeUserInteractedWithNonModalPromo,
    kUserHasInteractedWithFullscreenPromo,
    kUserHasInteractedWithTailoredFullscreenPromo,
    kUserHasInteractedWithFirstRunPromo,
    kUserInteractedWithNonModalPromoCount,
    kDisplayedFullscreenPromoCount,
    kTailoredPromoInteractionCount,
    kGenericPromoInteractionCount,
    // clang-format on
  ];

  return keysForTesting;
}

int GetNonModalDefaultBrowserPromoImpressionLimit() {
  int limit = kNonModalDefaultBrowserPromoImpressionLimitParam.Get();

  // The histogram only supports up to 10 impressions.
  if (limit > 10) {
    limit = 10;
  }

  return limit;
}

bool IsPostRestoreDefaultBrowserEligibleUser() {
  return IsFirstSessionAfterDeviceRestore() == signin::Tribool::kTrue &&
         IsChromeLikelyDefaultBrowser();
}

DefaultPromoTypeForUMA GetDefaultPromoTypeForUMA(DefaultPromoType type) {
  switch (type) {
    case DefaultPromoTypeGeneral:
      return DefaultPromoTypeForUMA::kGeneral;
    case DefaultPromoTypeMadeForIOS:
      return DefaultPromoTypeForUMA::kMadeForIOS;
    case DefaultPromoTypeStaySafe:
      return DefaultPromoTypeForUMA::kStaySafe;
    case DefaultPromoTypeAllTabs:
      return DefaultPromoTypeForUMA::kAllTabs;
    default:
      NOTREACHED();
  }
}

void LogDefaultBrowserPromoHistogramForAction(
    DefaultPromoType type,
    IOSDefaultBrowserPromoAction action) {
  switch (type) {
    case DefaultPromoTypeGeneral:
      base::UmaHistogramEnumeration("IOS.DefaultBrowserFullscreenPromo",
                                    action);
      break;
    case DefaultPromoTypeAllTabs:
      base::UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoAllTabs", action);
      break;
    case DefaultPromoTypeMadeForIOS:
      base::UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoMadeForIOS", action);
      break;
    case DefaultPromoTypeStaySafe:
      base::UmaHistogramEnumeration(
          "IOS.DefaultBrowserFullscreenTailoredPromoStaySafe", action);
      break;
    default:
      NOTREACHED();
  }
}

const std::string IOSDefaultBrowserPromoActionToString(
    IOSDefaultBrowserPromoAction action) {
  switch (action) {
    case IOSDefaultBrowserPromoAction::kActionButton:
      return "PrimaryAction";
    case IOSDefaultBrowserPromoAction::kCancel:
      return "Cancel";
    case IOSDefaultBrowserPromoAction::kDismiss:
      return "Dismiss";
    case IOSDefaultBrowserPromoAction::kRemindMeLater:
    default:
      NOTREACHED();
  }
}

void RecordPromoStatsToUMAForActionString(PromoStatistics* promo_stats,
                                          const std::string& action_str) {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    return;
  }
  std::string histogram_prefix =
      base::StrCat({"IOS.DefaultBrowserPromo.", action_str});

  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".PromoDisplayCount"}),
      promo_stats.promoDisplayCount);
  base::UmaHistogramCounts1000(
      base::StrCat({histogram_prefix, ".LastPromoInteractionNumDays"}),
      promo_stats.numDaysSinceLastPromo);
  base::UmaHistogramCounts1000(
      base::StrCat({histogram_prefix, ".ChromeColdStartCount"}),
      promo_stats.chromeColdStartCount);
  base::UmaHistogramCounts1000(
      base::StrCat({histogram_prefix, ".ChromeWarmStartCount"}),
      promo_stats.chromeWarmStartCount);
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".ChromeIndirectStartCount"}),
      promo_stats.chromeIndirectStartCount);
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".PasswordManagerUseCount"}),
      promo_stats.passwordManagerUseCount);
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".OmniboxClipboardUseCount"}),
      promo_stats.omniboxClipboardUseCount);
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".BookmarkUseCount"}),
      promo_stats.bookmarkUseCount);
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".AutofllUseCount"}),
      promo_stats.autofillUseCount);
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".SpecialTabsUseCount"}),
      promo_stats.specialTabsUseCount);
}

PromoStatistics* CalculatePromoStatistics() {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    return nil;
  }

  PromoStatistics* promo_stats = [[PromoStatistics alloc] init];
  promo_stats.promoDisplayCount = DisplayedFullscreenPromoCount();
  promo_stats.numDaysSinceLastPromo = NumDaysSincePromoInteraction();
  promo_stats.chromeColdStartCount = NumRecordedEventForKeyLessThanDelay(
      kAllTimestampsAppLaunchColdStart,
      kTriggerCriteriaExperimentStatExpiration);
  promo_stats.chromeWarmStartCount = NumRecordedEventForKeyLessThanDelay(
      kAllTimestampsAppLaunchWarmStart,
      kTriggerCriteriaExperimentStatExpiration);
  promo_stats.chromeIndirectStartCount = NumRecordedEventForKeyLessThanDelay(
      kAllTimestampsAppLaunchIndirectStart,
      kTriggerCriteriaExperimentStatExpiration);
  promo_stats.activeDayCount = NumActiveDays();
  promo_stats.passwordManagerUseCount = NumRecordedEventForKeyLessThanDelay(
      kLastSignificantUserEventStaySafe,
      kTriggerCriteriaExperimentStatExpiration);
  promo_stats.omniboxClipboardUseCount = NumRecordedEventForKeyLessThanDelay(
      kOmniboxUseCount, kTriggerCriteriaExperimentStatExpiration);
  promo_stats.bookmarkUseCount = NumRecordedEventForKeyLessThanDelay(
      kBookmarkUseCount, kTriggerCriteriaExperimentStatExpiration);
  promo_stats.autofillUseCount = NumRecordedEventForKeyLessThanDelay(
      kAutofillUseCount, kTriggerCriteriaExperimentStatExpiration);
  promo_stats.specialTabsUseCount = NumRecordedEventForKeyLessThanDelay(
      kSpecialTabsUseCount, kTriggerCriteriaExperimentStatExpiration);
  return promo_stats;
}

void RecordPromoStatsToUMAForAction(PromoStatistics* promo_stats,
                                    IOSDefaultBrowserPromoAction action) {
  RecordPromoStatsToUMAForActionString(
      promo_stats, IOSDefaultBrowserPromoActionToString(action));
}

void RecordPromoStatsToUMAForAppear(PromoStatistics* promo_stats) {
  RecordPromoStatsToUMAForActionString(promo_stats, kAppearAction);
}

void RecordPromoDisplayStatsToUMA() {
  base::UmaHistogramCounts1000(
      "IOS.DefaultBrowserPromo.DaysSinceLastPromoInteraction",
      NumDaysSincePromoInteraction());
  base::UmaHistogramCounts100(
      "IOS.DefaultBrowserPromo.GenericPromoDisplayCount",
      GenericPromoInteractionCount());
  base::UmaHistogramCounts100(
      "IOS.DefaultBrowserPromo.TailoredPromoDisplayCount",
      TailoredPromoInteractionCount());
}

void LogBrowserLaunched(bool is_cold_start) {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    CleanupStorageForTriggerExperiment();
    return;
  }

  NSString* key = is_cold_start ? kAllTimestampsAppLaunchColdStart
                                : kAllTimestampsAppLaunchWarmStart;
  StoreCurrentTimestampForKey(key);
}

void LogBrowserIndirectlylaunched() {
  if (!IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    CleanupStorageForTriggerExperiment();
    return;
  }

  StoreCurrentTimestampForKey(kAllTimestampsAppLaunchIndirectStart);
}

// Migration to FET

base::Time GetDefaultBrowserFREPromoTimestampIfLast() {
  // Get FRE promo timestamp. It is the last seen timestamp if user has seen
  // only 1 promo. If user has seen more promos, then we assume that FRE
  // happened far past enough for it not be important.
  if (HasUserInteractedWithFirstRunPromoBefore() &&
      DisplayedFullscreenPromoCount() == 1) {
    NSDate* timestamp = GetObjectFromStorageForKey<NSDate>(
        kLastTimeUserInteractedWithFullscreenPromo);
    if (timestamp != nil) {
      return base::Time::FromNSDate(timestamp);
    }
  }

  return base::Time::UnixEpoch();
}

base::Time GetGenericDefaultBrowserPromoTimestamp() {
  // Get the latest promo timestamp if user has seen the generic promo before
  // even when the generic promo is not the latest promo. This is the best we
  // can get considering the actual timestamp is overwritten.
  NSNumber* number = GetObjectFromStorageForKey<NSNumber>(
      kUserHasInteractedWithFullscreenPromo);
  if (number.boolValue) {
    NSDate* timestamp = GetObjectFromStorageForKey<NSDate>(
        kLastTimeUserInteractedWithFullscreenPromo);
    if (timestamp != nil) {
      return base::Time::FromNSDate(timestamp);
    }
  }

  return base::Time::UnixEpoch();
}

base::Time GetTailoredDefaultBrowserPromoTimestamp() {
  // Get the latest promo timestamp if user has seen the tailored promo before
  // even when the tailored promo is not the latest promo. This is the best we
  // can get considering the actual timestamp is overwritten.
  if (HasUserInteractedWithTailoredFullscreenPromoBefore()) {
    NSDate* timestamp = GetObjectFromStorageForKey<NSDate>(
        kLastTimeUserInteractedWithFullscreenPromo);
    if (timestamp != nil) {
      return base::Time::FromNSDate(timestamp);
    }
  }

  return base::Time::UnixEpoch();
}

void LogFRETimestampMigrationDone() {
  NSDictionary<NSString*, NSObject*>* update =
      @{kFRETimestampMigrationDone : @YES};
  UpdateStorageWithDictionary(update);
}

BOOL FRETimestampMigrationDone() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kFRETimestampMigrationDone);
  return number.boolValue;
}

void LogPromoInterestEventMigrationDone() {
  NSDictionary<NSString*, NSObject*>* update =
      @{kPromoInterestEventMigrationDone : @YES};
  UpdateStorageWithDictionary(update);
}

BOOL IsPromoInterestEventMigrationDone() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kPromoInterestEventMigrationDone);
  return number.boolValue;
}

void LogPromoImpressionsMigrationDone() {
  NSDictionary<NSString*, NSObject*>* update =
      @{kPromoImpressionsMigrationDone : @YES};
  UpdateStorageWithDictionary(update);
}

BOOL IsPromoImpressionsMigrationDone() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kPromoImpressionsMigrationDone);
  return number.boolValue;
}

void RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction action) {
  GetApplicationContext()->GetLocalState()->SetInteger(
      prefs::kIosDefaultBrowserPromoLastAction, static_cast<int>(action));
}

std::optional<IOSDefaultBrowserPromoAction> DefaultBrowserPromoLastAction() {
  const PrefService::Preference* last_action =
      GetApplicationContext()->GetLocalState()->FindPreference(
          prefs::kIosDefaultBrowserPromoLastAction);
  if (last_action->IsDefaultValue()) {
    return std::nullopt;
  }
  int last_action_int = last_action->GetValue()->GetInt();
  return static_cast<IOSDefaultBrowserPromoAction>(last_action_int);
}
