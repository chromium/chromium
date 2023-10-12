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
namespace syncer {
class SyncService;
}

// Enum for the different types of default browser modal promo. These are stored
// as values, if adding a new one, make sure to add it at the end.
typedef NS_ENUM(NSUInteger, DefaultPromoType) {
  DefaultPromoTypeGeneral = 0,
  DefaultPromoTypeStaySafe = 1,
  DefaultPromoTypeMadeForIOS = 2,
  DefaultPromoTypeAllTabs = 3,
  DefaultPromoTypeVideo = 4,
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

// The feature parameter to activate the remind me later button.
extern const char kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam[];

// Visible for testing

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
extern const char kVideoConditionsFullscreenPromo[];
extern const char kVideoConditionsHalfscreenPromo[];
extern const char kGenericConditionsFullscreenPromo[];
extern const char kGenericConditionsHalfscreenPromo[];
extern const char kDefaultBrowserVideoPromoVariant[];

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

// Logs to the FET that the user has pasted a URL into the omnibox if certain
// conditions are met.
void LogToFETUserPastedURLIntoOmnibox(feature_engagement::Tracker* tracker);

// Returns true if the passed default browser badge `feature` should be shown.
// Also makes the necessary calls to the FET for keeping track of usage, as well
// as checking that the correct preconditions are met.
bool ShouldTriggerDefaultBrowserHighlightFeature(
    const base::Feature& feature,
    feature_engagement::Tracker* tracker,
    syncer::SyncService* syncService);

// Returns true if the user is not in the blue dot default browser experiment,
// or if they are in the group with all DB promos enabled.
bool AreDefaultBrowserPromosEnabled();

// Returns true if the user is in the default browser blue dot experiment and in
// one of the blue dot active/enabled groups.
bool IsBlueDotPromoEnabled();

// Returns true if the full screen default browser promos are added to the promo
// manager.
bool IsDefaultBrowserInPromoManagerEnabled();

// Returns true if the default browser video promo is enabled.
bool IsDefaultBrowserVideoPromoEnabled();

// Returns true if the default browser video promo half screen enabled.
bool IsDBVideoPromoHalfscreenEnabled();

// Returns true if the default browser video promo full screen enabled.
bool IsDBVideoPromoFullscreenEnabled();

// Returns true if the default browser video promo full screen with generic
// triggering conditions enabled.
bool IsDBVideoPromoWithGenericFullscreenEnabled();

// Returns true if the default browser video promo half screen with generic
// triggering conditions enabled.
bool IsDBVideoPromoWithGenericHalfscreenEnabled();

// Returns true if the non-modal default browser promo cooldown refactor is
// enabled.
bool IsNonModalDefaultBrowserPromoCooldownRefactorEnabled();

// Returns true if the default browser promo triggering criteria should be
// skipped.
bool ShouldForceDefaultPromoType();

// Returns the promo type (DefaultPromoType) of the default browser promo after
// skipping the triggering criteria.
DefaultPromoType ForceDefaultPromoType();

// Returns true if client is in Default Browser promo trigger criteria
// experiment.
bool IsDefaultBrowserTriggerCriteraExperimentEnabled();

// Returns true if the default browser promo generic tailored experiment is
// enabled.
bool IsDefaultBrowserPromoGenericTailoredTrainEnabled();

// Returns true if the only-generic arm of the default browser promo generic
// tailored experiment is enabled.
bool IsDefaultBrowserPromoOnlyGenericArmTrain();

// Returns true if default Browser full-screen promo should be shown on omnibox
// copy-paste instead of non-modal promo.
bool IsFullScreenPromoOnOmniboxCopyPasteEnabled();

// Returns true if client is in default browser video in settings experiment.
bool IsDefaultBrowserVideoInSettingsEnabled();

// Returns true if the user has interacted with the Fullscreen Promo previously.
// Returns false otherwise.
bool HasUserInteractedWithFullscreenPromoBefore();

// Returns true if the user has interacted with a tailored Fullscreen Promo
// previously. Returns false otherwise.
bool HasUserInteractedWithTailoredFullscreenPromoBefore();

// Returns the number of times the user has seen and interacted with the
// non-modal promo before.
NSInteger UserInteractionWithNonModalPromoCount();

// Logs that one of the fullscreen default browser promos was displayed.
void LogFullscreenDefaultBrowserPromoDisplayed();

// Logs that the user has interacted with the Fullscreen Promo.
void LogUserInteractionWithFullscreenPromo();

// Logs that the user has interacted with a Tailored Fullscreen Promo.
void LogUserInteractionWithTailoredFullscreenPromo();

// Logs that the user has interacted with a Non-Modals Promo.
void LogUserInteractionWithNonModalPromo();

// Logs that the user has interacted with the first run promo.
void LogUserInteractionWithFirstRunPromo(BOOL openedSettings);

// Logs in NSUserDefaults that user copy-pasted in the omnibox.
void LogCopyPasteInOmniboxForDefaultBrowserPromo();

// Logs in NSUserDefaults that user used bookmarks or bookmark manager.
void LogBookmarkUseForDefaultBrowserPromo();

// Logs in NSUserDefaults that user used autofill suggestions
void LogAutofillUseForDefaultBrowserPromo();

// Logs that the user has used remote tabs.
void LogRemoteTabsUsedForDefaultBrowserPromo();

// Logs that the user has used pinned tabs.
void LogPinnedTabsUsedForDefaultBrowserPromo();

// Returns YES if the user has opened the app through first-party intent 2
// times in the last 7 days, but across 2 user sessions (default 6 hours). Also
// records that a new launch has happened if the last one was more than one
// session ago.
bool HasRecentFirstPartyIntentLaunchesAndRecordsCurrentLaunch();

// Returns YES if the user has pasted a valid URL into the omnibox twice in
// the last 7 days and records the current paste.
bool HasRecentValidURLPastesAndRecordsCurrentPaste();

// Returns YES if the last timestamp passed as `eventKey` is part of the current
// user session (default 6 hours). If not, it records the timestamp.
bool HasRecentTimestampForKey(NSString* eventKey);

// Returns true if the last URL open is within the time threshold that would
// indicate Chrome is likely still the default browser. Returns false otherwise.
bool IsChromeLikelyDefaultBrowser();

// Do not use. Only for backward compatibility
// Returns true if the last URL open is within 7 days. Returns false otherwise.
bool IsChromeLikelyDefaultBrowser7Days();

// Returns true if the past behavior of the user indicates that the user fits
// the categorization that would likely benefit from having Chrome set as their
// default browser for the passed `type`. Returns false otherwise.
bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType type);

// Returns the most recent promo the user showed interest in. Defaults to
// DefaultPromoTypeGeneral if no interest is found. If `skipAllTabsPromo` is
// true, this type of promo will be ignored.
DefaultPromoType MostRecentInterestDefaultPromoType(BOOL skipAllTabsPromo);

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

// Returns YES if the app has launched on cold start under
// `kTimestampAppLaunchOnColdStart`.
bool HasAppLaunchedOnColdStartAndRecordsLaunch();

// Return true if the default browser promo should be registered with the promo
// manager to display a default browser promo.
bool ShouldRegisterPromoWithPromoManager(bool is_signed_in,
                                         bool is_omnibox_copy_paste,
                                         feature_engagement::Tracker* tracker);

// Returns true if it was determined that the user is eligible for a
// tailored promo.
bool IsTailoredPromoEligibleUser(bool is_signed_in);

// Returns true if it was determined that the user is eligible for the
// general promo.
bool IsGeneralPromoEligibleUser(bool is_signed_in);

// Returns true if it was determined that the user is eligible for the
// post restore default browser promo.
bool IsPostRestoreDefaultBrowserEligibleUser();

// Return true if it was determined that the user is eligible for the
// video promo.
bool IsVideoPromoEligibleUser(feature_engagement::Tracker* tracker);

// Removes unused data from NSUserDefaults. This method should be periodically
// pruned of cleanups that have been present for multiple milestones.
void CleanupUnusedStorage();

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

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_UTILS_H_
