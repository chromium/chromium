// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_UTILS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_UTILS_H_

#import <UIKit/UIKit.h>

#import "base/feature_list.h"
#import "ios/chrome/browser/default_browser/model/promo_statistics.h"

namespace feature_engagement {
class Tracker;
}
namespace base {
class Time;
class TimeDelta;
}

// Enum for the different types of default browser modal promo. These are stored
// as values, if adding a new one, make sure to add it at the end.
typedef NS_ENUM(NSUInteger, DefaultPromoType) {
  DefaultPromoTypeGeneral = 0,
  DefaultPromoTypeStaySafe = 1,
  DefaultPromoTypeMadeForIOS = 2,
  DefaultPromoTypeAllTabs = 3,
};

// Enum actions for default browser promo UMA metrics. Entries should not be
// renumbered and numeric values should never be reused.
enum class IOSDefaultBrowserPromoAction {
  kActionButton = 0,
  kCancel = 1,
  kRemindMeLater = 2,
  kDismiss = 3,
  kMaxValue = kDismiss,
};

// Enum for the default browser promo UMA histograms. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class DefaultPromoTypeForUMA {
  kGeneral = 0,
  kMadeForIOS = 1,
  kStaySafe = 2,
  kAllTabs = 3,
  kMaxValue = kAllTabs,
};

// Enum actions for the IOS.DefaultBrowserVideoPromo.(Fullscreen || Halfscreen)*
// UMA metrics.
enum class IOSDefaultBrowserVideoPromoAction {
  kPrimaryActionTapped = 0,
  kSecondaryActionTapped = 1,
  kSwipeDown = 2,
  kTertiaryActionTapped = 3,
  kMaxValue = kTertiaryActionTapped,
};

// Visible for testing

// Key in storage containing an NSDate corresponding to the last time
// an HTTP(S) link was sent and opened by the app.
extern NSString* const kLastHTTPURLOpenTime;

// Key in storage containing an NSDate indicating the last time a user
// interacted with a non-modal promo.
extern NSString* const kLastTimeUserInteractedWithNonModalPromo;

// Key in storage containing an NSDate indicating the last time a user
// interacted with ANY full screen promo. The string value is kept from when the
// promos first launched to avoid changing the behavior for users that have
// already seen the promo.
extern NSString* const kLastTimeUserInteractedWithFullscreenPromo;

// Key in storage containing all the recent timestamps of browser cold starts up
// to allowed maximum number of past events.
extern NSString* const kAllTimestampsAppLaunchColdStart;

// Key in storage containing all the recent timestamps of browser warm starts up
// to allowed maximum number of past events.
extern NSString* const kAllTimestampsAppLaunchWarmStart;

// Key in storage containing all the recent timestamps of browser indirect
// starts up to allowed maximum number of past events.
extern NSString* const kAllTimestampsAppLaunchIndirectStart;

// Key in storage containing an array of dates. Each date correspond to
// a stay safe event of interest for Default Browser Promo modals.
extern NSString* const kLastSignificantUserEventStaySafe;

// Key in storage containing an array of dates. Each date correspond to
// a omnibox copy-paste event up to allowed maximum number of past events.
extern NSString* const kOmniboxUseCount;

// Key in storage containing an array of dates. Each date correspond to
// a bookmark or bookmark manager use event up to allowed maximum number of past
// events.
extern NSString* const kBookmarkUseCount;

// Key in storage containing an array of dates. Each date correspond to
// a autofill suggestion use event up to allowed maximum number of past
// events.
extern NSString* const kAutofillUseCount;

// Key in storage containing an array of dates where each date correspond to
// a pinned tab or remote tab use event.
extern NSString* const kSpecialTabsUseCount;

// Param names used for the default browser video promo.
extern const char kVideoFullscreenPromo[];
extern const char kVideoHalfscreenPromo[];
extern const char kDefaultBrowserVideoPromoVariant[];

// Key in storage containing a bool indicating if the user has
// previously interacted with a regular fullscreen promo.
extern NSString* const kUserHasInteractedWithFullscreenPromo;

// Key in storage containing a bool indicating if the user has
// previously interacted with a tailored fullscreen promo.
extern NSString* const kUserHasInteractedWithTailoredFullscreenPromo;

// Key in storage containing a bool indicating if the user has
// previously interacted with first run promo.
extern NSString* const kUserHasInteractedWithFirstRunPromo;

// Key in storage containing an int indicating the number of times a fullscreen
// promo has been displayed.
extern NSString* const kDisplayedFullscreenPromoCount;

// Key in storage containing an int indicating the number of times a generic
// promo has been displayed.
extern NSString* const kGenericPromoInteractionCount;

// Key in storage containing an int indicating the number of times a tailored
// promo has been displayed.
extern NSString* const kTailoredPromoInteractionCount;

// Key in storage containing the timestamp of when trigger criteria experiment
// started.
extern NSString* const kTimestampTriggerCriteriaExperimentStarted;

// Specifies how long blue dot occurrence should last.
extern base::TimeDelta const kBlueDotPromoDuration;

// Specifies how often blue dot should reoccur.
extern base::TimeDelta const kBlueDotPromoReoccurrancePeriod;

// Loads from NSUserDefaults the time of the non-expired events for the
// given promo type.
std::vector<base::Time> LoadTimestampsForPromoType(DefaultPromoType type);

// Stores the time of the last recorded events for `type`.
void StoreTimestampsForPromoType(DefaultPromoType type,
                                 std::vector<base::Time> times);

// Helper function to set `data` for `key` into the storage object.
void SetObjectIntoStorageForKey(NSString* key, NSObject* data);

// Logs the timestamp of opening an HTTP(S) link sent and opened by the app.
void LogOpenHTTPURLFromExternalURL();

// Logs the timestamp of user activity that is deemed to be an indication of
// a user that would likely benefit from having Chrome set as their default
// browser. Before logging the current activity, this method will also clear all
// past expired logs for `type` that have happened too far in the past.
void LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoType type);

// Logs to the FET that a default browser promo has been shown.
void LogToFETDefaultBrowserPromoShown(feature_engagement::Tracker* tracker);

// Returns whether blue dot display timestamp has already been set.
bool HasDefaultBrowserBlueDotDisplayTimestamp();

// Resets  blue dot display timestamp to its default value when needed.
void ResetDefaultBrowserBlueDotDisplayTimestampIfNeeded();

// Set the current timestamp as blue dot first display timestamp if this was the
// first instance.
void RecordDefaultBrowserBlueDotFirstDisplay();

// Returns true if the default browser blue dot should be shown.
bool ShouldTriggerDefaultBrowserHighlightFeature(
    feature_engagement::Tracker* tracker);

// Returns true if the non-modal default browser promo cooldown refactor is
// enabled.
bool IsNonModalDefaultBrowserPromoCooldownRefactorEnabled();

// Returns true if client is in Default Browser promo trigger criteria
// experiment.
bool IsDefaultBrowserTriggerCriteraExperimentEnabled();

// Sets trigger criteria experiment start timestamp to now.
void SetTriggerCriteriaExperimentStartTimestamp();

// Returns true if trigger criteria experiment has been started.
bool HasTriggerCriteriaExperimentStarted();

// Returns true if trigger criteria experiment has been started for at least 21
// days.
bool HasTriggerCriteriaExperimentStarted21days();

// Returns true if the default browser promo generic tailored experiment is
// enabled.
bool IsDefaultBrowserPromoGenericTailoredTrainEnabled();

// Returns true if the only-generic arm of the default browser promo generic
// tailored experiment is enabled.
bool IsDefaultBrowserPromoOnlyGenericArmTrain();

// Returns true if the user has interacted with the Fullscreen Promo previously.
// Returns false otherwise.
bool HasUserInteractedWithFullscreenPromoBefore();

// Returns true if the user has interacted with a tailored Fullscreen Promo
// previously. Returns false otherwise.
bool HasUserInteractedWithTailoredFullscreenPromoBefore();

// Returns the number of times the user has seen and interacted with the
// non-modal promo before.
NSInteger UserInteractionWithNonModalPromoCount();

// Returns the number of times a fullscreen default browser promo has been
// displayed.
NSInteger DisplayedFullscreenPromoCount();

// Logs that one of the fullscreen default browser promos was displayed.
void LogFullscreenDefaultBrowserPromoDisplayed();

// Logs that the user has interacted with the Fullscreen Promo.
void LogUserInteractionWithFullscreenPromo();

// Logs that the user has interacted with a Tailored Fullscreen Promo.
void LogUserInteractionWithTailoredFullscreenPromo();

// Logs that the user has interacted with a non-modal promo. The expected
// parameters are the current counts, because they will be incremented by 1 and
// then saved to NSUserDefaults. If kNonModalDefaultBrowserPromoCooldownRefactor
// is disabled, kDisplayedFullscreenPromoCount will also be incremented by 1.
void LogUserInteractionWithNonModalPromo(
    NSInteger currentNonModalPromoInteractionsCount,
    NSInteger currentFullscreenPromoInteractionsCount);

// Logs that the user has interacted with the first run promo.
void LogUserInteractionWithFirstRunPromo();

// Logs in NSUserDefaults that user copy-pasted in the omnibox.
void LogCopyPasteInOmniboxForCriteriaExperiment();

// Logs in NSUserDefaults that user used bookmarks or bookmark manager.
void LogBookmarkUseForCriteriaExperiment();

// Logs in NSUserDefaults that user used autofill suggestions
void LogAutofillUseForCriteriaExperiment();

// Logs that the user has used remote tabs.
void LogRemoteTabsUseForCriteriaExperiment();

// Returns true if the last URL open is within the specified number of `days`
// which would indicate Chrome is likely still the default browser. Returns
// false otherwise.
bool IsChromeLikelyDefaultBrowserXDays(int days);

// Returns true if the last URL open is within the time threshold that would
// indicate Chrome is likely still the default browser. Returns false otherwise.
bool IsChromeLikelyDefaultBrowser();

// Do not use. Only for backward compatibility
// Returns true if the last URL open is within 7 days. Returns false otherwise.
bool IsChromeLikelyDefaultBrowser7Days();

// Returns true if Chrome was likely the default browser in the last
// `likelyDefaultInterval` days but not in the last `likelyNotDefaultInterval`
// days.
bool IsChromePotentiallyNoLongerDefaultBrowser(int likelyDefaultInterval,
                                               int likelyNotDefaultInterval);

// Returns true if the past behavior of the user indicates that the user fits
// the categorization that would likely benefit from having Chrome set as their
// default browser for the passed `type`. Returns false otherwise.
bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType type);

// Return YES if the user has seen a full screen promo recently, and shouldn't
// see another one.
bool UserInFullscreenPromoCooldown();

// Returns YES if the user has seen a non-modal promo recently, and shouldn't
// see another one.
bool UserInNonModalPromoCooldown();

// List of all key used to store data in NSUserDefaults. Still used as key
// in the NSDictionary stored under `kBrowserDefaultsKey`.
const NSArray<NSString*>* DefaultBrowserUtilsLegacyKeysForTesting();

// Returns the impression limit for the non-modal default browser promo.
int GetNonModalDefaultBrowserPromoImpressionLimit();

// Returns true if it was determined that the user is eligible for the
// post restore default browser promo.
bool IsPostRestoreDefaultBrowserEligibleUser();

// Converts Default browser promo type NSEnum to an enum that can be used by
// UMA.
DefaultPromoTypeForUMA GetDefaultPromoTypeForUMA(DefaultPromoType type);

// Log given default browser promo action to the UMA histogram coorespnding to
// the given promo type.
void LogDefaultBrowserPromoHistogramForAction(
    DefaultPromoType type,
    IOSDefaultBrowserPromoAction action);

// Below collect and compute data to record for an experiment. It is potentially
// a lot of data but this is planned as a short and small experiment.

// Returns string representation of the enum value.
const std::string IOSDefaultBrowserPromoActionToString(
    IOSDefaultBrowserPromoAction action);

// Returns PromoStatistics object with all properties calculated.
PromoStatistics* CalculatePromoStatistics();

// Records given promo stats for given action into UMA histograms.
void RecordPromoStatsToUMAForAction(PromoStatistics* promo_stats,
                                    IOSDefaultBrowserPromoAction action);

// Records given promo stats for "Appear" action into UMA histograms.
void RecordPromoStatsToUMAForAppear(PromoStatistics* promo_stats);

// Records stats related to promo display to UMA histograms.
void RecordPromoDisplayStatsToUMA();

// Logs browser launched for default browser promo trigger criteria experiment
// stats to NSUserDefaults. `LogBrowserIndirectlylaunched` and
// `LogBrowserLaunched` will have overlap.
void LogBrowserLaunched(bool is_cold_start);

// Log browser started indirectly(by widget or external url) for default browser
// promo experiment stats to NSUserDefaults. `LogBrowserIndirectlylaunched` and
// `LogBrowserLaunched` will have overlap.
void LogBrowserIndirectlylaunched();

// Migration to FET.

// Returns Default Browser FRE promo timestamp if it was the last default
// browser promo user seen. Otherwise, returns unix epoch.
base::Time GetDefaultBrowserFREPromoTimestampIfLast();

// Returns generic Default Browser timestamp if user seen a generic promo
// before. Otherwise, returns unix epoch.
base::Time GetGenericDefaultBrowserPromoTimestamp();

// Returns tailored Default Browser timestamp if user seen a tailored promo
// before. Otherwise, returns unix epoch.
base::Time GetTailoredDefaultBrowserPromoTimestamp();

// Log to UserDefaults FRE timestamp migration is done.
void LogFRETimestampMigrationDone();

// Returns whether FRE timestamp migratin is done.
BOOL FRETimestampMigrationDone();

// Log to UserDefaults promo interest event migration is done.
void LogPromoInterestEventMigrationDone();

// Returns whether promo interest event migratin is done.
BOOL IsPromoInterestEventMigrationDone();

// Log to UserDefaults promo impressions migration is done.
void LogPromoImpressionsMigrationDone();

// Returns whether promo impressions migratin is done.
BOOL IsPromoImpressionsMigrationDone();

// Records the last action the user took when a Default Browser Promo was
// presented.
void RecordDefaultBrowserPromoLastAction(IOSDefaultBrowserPromoAction action);

// Returns the last action, if any, that the user took when a Default Browser
// Promo was presented.
std::optional<IOSDefaultBrowserPromoAction> DefaultBrowserPromoLastAction();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_UTILS_H_
