// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/utils.h"

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
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/settings/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/signin_util.h"

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
// a made for iOS event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventMadeForIOS =
    @"lastSignificantUserEventMadeForIOS";

// Key in storage containing an array of dates. Each date correspond to
// an all tabs event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventAllTabs =
    @"lastSignificantUserEventAllTabs";

// Key in storage containing an array of dates. Each date correspond to
// a video event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventVideo =
    @"lastSignificantUserEventVideo";

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

// Key in storage containing an int indicating the number of times a fullscreen
// promo has been displayed.
NSString* const kDisplayedFullscreenPromoCount = @"displayedPromoCount";

// Key in storage containing an int indicating the number of times a generic
// promo has been displayed.
NSString* const kGenericPromoInteractionCount = @"genericPromoInteractionCount";

// Key in storage containing an int indicating the number of times a tailored
// promo has been displayed.
NSString* const kTailoredPromoInteractionCount =
    @"tailoredPromoInteractionCount";

// TODO(crbug.com/1445218): Remove in M116+.
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

const char kDefaultBrowserPromoForceShowPromo[] =
    "default-browser-promo-force-show-promo";

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

// Maximum time range between first-party app launches to notify the FET.
constexpr base::TimeDelta kMaximumTimeBetweenFirstPartyAppLaunches =
    base::Days(7);

// Maximum time range between app launches on cold start to notify the FET.
constexpr base::TimeDelta kMaximumTimeBetweenAppColdStartLaunches =
    base::Days(7);

// Maximum time range between valid user URL pastes to notify the FET.
constexpr base::TimeDelta kMaximumTimeBetweenValidURLPastes = base::Days(7);

// Time threshold for default browser trigger criteria experiment statistics.
constexpr base::TimeDelta kTriggerCriteriaExperimentStatExpiration =
    base::Days(14);

// List of DefaultPromoType considered by MostRecentInterestDefaultPromoType.
const DefaultPromoType kDefaultPromoTypes[] = {
    DefaultPromoTypeStaySafe,
    DefaultPromoTypeAllTabs,
    DefaultPromoTypeMadeForIOS,
};

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
    case DefaultPromoTypeVideo:
      return kLastSignificantUserEventVideo;
  }
  NOTREACHED();
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

// Loads from NSUserDefaults the time of the non-expired events for the
// given promo type.
std::vector<base::Time> LoadTimestampsForPromoType(DefaultPromoType type) {
  return LoadActiveTimestampsForKey(StorageKeyForDefaultPromoType(type),
                                    kUserActivityTimestampExpiration);
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

// Stores the time of the last recorded events for `type`.
void StoreTimestampsForPromoType(DefaultPromoType type,
                                 std::vector<base::Time> times) {
  StoreTimestampsForKey(StorageKeyForDefaultPromoType(type), times);
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

// Returns the number of times a fullscreen default browser promo has been
// displayed.
NSInteger DisplayedFullscreenPromoCount() {
  NSNumber* number =
      GetObjectFromStorageForKey<NSNumber>(kDisplayedFullscreenPromoCount);
  return number.integerValue;
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

  NSDateComponents* components =
      [NSCalendar.currentCalendar components:NSCalendarUnitDay
                                    fromDate:timestamp
                                      toDate:[NSDate date]
                                     options:0];
  if (!components.day || components.day < 0) {
    return 0;
  }

  return components.day;
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

std::string GetVideoPromoVariant() {
  return base::GetFieldTrialParamValueByFeature(
      kDefaultBrowserVideoPromo, "default_browser_video_promo_variant");
}

}  // namespace

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

const char kVideoConditionsFullscreenPromo[] =
    "video_conditions_fullscreen_promo";
const char kVideoConditionsHalfscreenPromo[] =
    "video_conditions_halfscreen_promo";
const char kGenericConditionsFullscreenPromo[] =
    "generic_conditions_fullscreen_promo";
const char kGenericConditionsHalfscreenPromo[] =
    "generic_conditions_halfscreen_promo";
const char kDefaultBrowserVideoPromoVariant[] =
    "default_browser_video_promo_variant";

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

bool ShouldForceDefaultPromoType() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDefaultBrowserPromoForceShowPromo);
}

DefaultPromoType ForceDefaultPromoType() {
  DCHECK(ShouldForceDefaultPromoType());
  std::string type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kDefaultBrowserPromoForceShowPromo);
  int default_promo_type = 0;
  if (base::StringToInt(type, &default_promo_type)) {
    switch (default_promo_type) {
      case DefaultPromoTypeGeneral:
      case DefaultPromoTypeStaySafe:
      case DefaultPromoTypeMadeForIOS:
      case DefaultPromoTypeAllTabs:
      case DefaultPromoTypeVideo:
        return static_cast<DefaultPromoType>(default_promo_type);
    }
  }

  return DefaultPromoType::DefaultPromoTypeGeneral;
}

bool IsDefaultBrowserTriggerCriteraExperimentEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserTriggerCriteriaExperiment);
}

bool IsDefaultBrowserPromoGenericTailoredTrainEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserGenericTailoredPromoTrain);
}

bool IsDefaultBrowserPromoOnlyGenericArmTrain() {
  return kDefaultBrowserPromoGenericTailoredParam.Get() ==
         DefaultBrowserPromoGenericTailoredArm::kOnlyGeneric;
}

bool IsFullScreenPromoOnOmniboxCopyPasteEnabled() {
  return base::FeatureList::IsEnabled(kFullScreenPromoOnOmniboxCopyPaste);
}

bool IsDBVideoPromoHalfscreenEnabled() {
  return GetVideoPromoVariant().compare(kVideoConditionsHalfscreenPromo) == 0;
}

bool IsDBVideoPromoFullscreenEnabled() {
  return GetVideoPromoVariant().compare(kVideoConditionsFullscreenPromo) == 0;
}

bool IsDBVideoPromoWithGenericFullscreenEnabled() {
  return GetVideoPromoVariant().compare(kGenericConditionsFullscreenPromo) == 0;
}

bool IsDBVideoPromoWithGenericHalfscreenEnabled() {
  return GetVideoPromoVariant().compare(kGenericConditionsHalfscreenPromo) == 0;
}

bool IsNonModalDefaultBrowserPromoCooldownRefactorEnabled() {
  return base::FeatureList::IsEnabled(
      kNonModalDefaultBrowserPromoCooldownRefactor);
}

bool IsDefaultBrowserVideoInSettingsEnabled() {
  return base::FeatureList::IsEnabled(kDefaultBrowserVideoInSettings);
}

bool HasUserInteractedWithFullscreenPromoBefore() {
  if (base::FeatureList::IsEnabled(
          feature_engagement::kDefaultBrowserEligibilitySlidingWindow)) {
    return HasRecordedEventForKeyLessThanDelay(
        kLastTimeUserInteractedWithFullscreenPromo,
        base::Days(
            feature_engagement::kDefaultBrowserEligibilitySlidingWindowParam
                .Get()));
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

void LogUserInteractionWithNonModalPromo() {
  const NSInteger interaction_count = UserInteractionWithNonModalPromoCount();

  if (IsNonModalDefaultBrowserPromoCooldownRefactorEnabled()) {
    UpdateStorageWithDictionary(@{
      kLastTimeUserInteractedWithNonModalPromo : [NSDate date],
      kUserInteractedWithNonModalPromoCount : @(interaction_count + 1),
    });
  } else {
    const NSInteger displayed_promo_count = DisplayedFullscreenPromoCount();
    UpdateStorageWithDictionary(@{
      kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
      kUserInteractedWithNonModalPromoCount : @(interaction_count + 1),
      kDisplayedFullscreenPromoCount : @(displayed_promo_count + 1),
    });
  }
}

void LogUserInteractionWithFirstRunPromo(BOOL openedSettings) {
  const NSInteger displayed_promo_count = DisplayedFullscreenPromoCount();
  UpdateStorageWithDictionary(@{
    kUserHasInteractedWithFirstRunPromo : @YES,
    kLastTimeUserInteractedWithFullscreenPromo : [NSDate date],
    kDisplayedFullscreenPromoCount : @(displayed_promo_count + 1),
  });
}

void LogCopyPasteInOmniboxForDefaultBrowserPromo() {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  StoreCurrentTimestampForKey(kOmniboxUseCount);
}

void LogBookmarkUseForDefaultBrowserPromo() {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  StoreCurrentTimestampForKey(kBookmarkUseCount);
}

void LogAutofillUseForDefaultBrowserPromo() {
  StoreCurrentTimestampForKey(kAutofillUseCount);
}

void LogRemoteTabsUsedForDefaultBrowserPromo() {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  StoreCurrentTimestampForKey(kSpecialTabsUseCount);
}

void LogPinnedTabsUsedForDefaultBrowserPromo() {
  StoreCurrentTimestampForKey(kSpecialTabsUseCount);
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
    kRemindMeLaterPromoActionInteraction,
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

bool ShouldRegisterPromoWithPromoManager(bool is_signed_in,
                                         bool is_omnibox_copy_paste,
                                         feature_engagement::Tracker* tracker) {
  if (ShouldForceDefaultPromoType()) {
    return YES;
  }

  // Consider showing the default browser promo if (1) launch is not after a
  // crash, (2) chrome is not likely set as default browser.
  if (!GetApplicationContext()->WasLastShutdownClean() ||
      IsChromeLikelyDefaultBrowser()) {
    return NO;
  }

  // Consider showing full-screen promo on omnibox copy-paste event iff
  // corresponding experiment is enabled.
  if (IsFullScreenPromoOnOmniboxCopyPasteEnabled() != is_omnibox_copy_paste) {
    return NO;
  }

  // If in trigger criteria experiment, then show default browser promo skipping
  // further checks.
  if (IsDefaultBrowserTriggerCriteraExperimentEnabled()) {
    return YES;
  }

  // Consider showing the default browser promo if (1) the user has not seen a
  // default browser promo too recently, (2) the user is eligible for either the
  // tailored or generic default browser promo.
  return !UserInFullscreenPromoCooldown() &&
         (IsTailoredPromoEligibleUser(is_signed_in) ||
          IsGeneralPromoEligibleUser(is_signed_in) ||
          IsVideoPromoEligibleUser(tracker));
}

bool IsTailoredPromoEligibleUser(bool is_signed_in) {
  bool is_all_tabs_promo_eligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs) &&
      is_signed_in;
  bool is_eligible =
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS) ||
      is_all_tabs_promo_eligible ||
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe);

  if (!is_eligible) {
    return false;
  }

  // When the default browser promo generic and tailored train experiment is
  // enabled, allow the generic and tailored promos to be shown at least twice.
  if (IsDefaultBrowserPromoGenericTailoredTrainEnabled()) {
    return TailoredPromoInteractionCount() < 2 &&
           GenericPromoInteractionCount() < 2;
  }

  return !HasUserInteractedWithTailoredFullscreenPromoBefore();
}

bool IsGeneralPromoEligibleUser(bool is_signed_in) {
  // When the default browser promo generic and tailored train experiment is
  // enabled, the generic default browser promo will only be shown when the user
  // is eligible for a tailored promo.
  if (IsDefaultBrowserPromoGenericTailoredTrainEnabled()) {
    return false;
  }

  return !HasUserInteractedWithFullscreenPromoBefore() &&
         (IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral) ||
          is_signed_in);
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

bool IsPostRestoreDefaultBrowserEligibleUser() {
  return IsFirstSessionAfterDeviceRestore() == signin::Tribool::kTrue &&
         IsChromeLikelyDefaultBrowser();
}

void CleanupUnusedStorage() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // TODO(crbug.com/1445240): Remove in M116+.
  [defaults removeObjectForKey:kOpenSettingsActionInteraction];
  // TODO(crbug.com/1445218): Remove in M116+.
  [defaults removeObjectForKey:kRemindMeLaterPromoActionInteraction];
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
      NOTREACHED_NORETURN();
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
      NOTREACHED_NORETURN();
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
      NOTREACHED_NORETURN();
  }
}

void CleanupStorageForTriggerExperiment() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  [defaults removeObjectForKey:kAllTimestampsAppLaunchColdStart];
  [defaults removeObjectForKey:kAllTimestampsAppLaunchWarmStart];
  [defaults removeObjectForKey:kAllTimestampsAppLaunchIndirectStart];
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
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".LastPromoInteractionNumDays"}),
      promo_stats.numDaysSinceLastPromo);
  base::UmaHistogramCounts100(
      base::StrCat({histogram_prefix, ".ChromeColdStartCount"}),
      promo_stats.chromeColdStartCount);
  base::UmaHistogramCounts100(
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
  base::UmaHistogramCounts100(
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
