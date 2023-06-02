// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_UTILS_H_

#import <UIKit/UIKit.h>

#import "base/feature_list.h"

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
  DefaultPromoTypeAllTabs = 3
};

namespace {

// Enum actions for the IOS.DefaultBrowserFullscreenPromo* UMA metrics. Entries
// should not be renumbered and numeric values should never be reused.
enum class IOSDefaultBrowserFullscreenPromoAction {
  kActionButton = 0,
  kCancel = 1,
  kRemindMeLater = 2,
  kMaxValue = kRemindMeLater,
};

}  // namespace

// The feature parameter to activate the remind me later button.
extern const char kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam[];

// Logs the timestamp of opening an HTTP(S) link sent and opened by the app.
void LogOpenHTTPURLFromExternalURL();

// Logs the timestamp of user activity that is deemed to be an indication of
// a user that would likely benefit from having Chrome set as their default
// browser. Before logging the current activity, this method will also clear all
// past expired logs for `type` that have happened too far in the past.
void LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoType type);

// Logs the timestamp of a user tap on the "Remind Me Later" button in the
// Fullscreen Promo.
void LogRemindMeLaterPromoActionInteraction();

// Logs to the FET that a default browser promo has been shown.
void LogToFETDefaultBrowserPromoShown(feature_engagement::Tracker* tracker);

// Logs to the FET that the user has pasted a URL into the omnibox if certain
// conditions are met.
void LogToFETUserPastedURLIntoOmnibox(feature_engagement::Tracker* tracker);

// Returns true if the user has tapped on the "Remind Me Later" button and the
// delay time threshold has been met.
bool ShouldShowRemindMeLaterDefaultBrowserFullscreenPromo();

// Returns true if the passed default browser badge `feature` should be shown.
// Also makes the necessary calls to the FET for keeping track of usage, as well
// as checking that the correct preconditions are met.
bool ShouldTriggerDefaultBrowserHighlightFeature(
    const base::Feature& feature,
    feature_engagement::Tracker* tracker,
    syncer::SyncService* syncService);

// Returns true if the user is in the group that will be shown the Remind Me
// Later button in the fullscreen promo.
bool IsInRemindMeLaterGroup();

// Returns true if the user is in the group that will be shown a modified
// description and "Learn More" text.
bool IsInModifiedStringsGroup();

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

// Returns true if the default browser video promo full screen enabled.
bool IsDefaultBrowserVideoPromoFullscreenEnabled();

// Returns true if the user is in the CTA experiment in the open links group.
bool IsInCTAOpenLinksGroup();

// Returns true if the user is in the CTA experiment in the switch group.
bool IsInCTASwitchGroup();

// Returns true if the user has interacted with the Fullscreen Promo previously.
// Returns false otherwise.
bool HasUserInteractedWithFullscreenPromoBefore();

// Returns true if the user has interacted with a tailored Fullscreen Promo
// previously. Returns false otherwise.
bool HasUserInteractedWithTailoredFullscreenPromoBefore();

// Returns the number of times the user has seen and interacted with the
// non-modal promo before.
NSInteger UserInteractionWithNonModalPromoCount();

// Logs that the user has interacted with the Fullscreen Promo.
void LogUserInteractionWithFullscreenPromo();

// Logs that the user has interacted with a Tailored Fullscreen Promo.
void LogUserInteractionWithTailoredFullscreenPromo();

// Logs that the user has interacted with a Non-Modals Promo.
void LogUserInteractionWithNonModalPromo();

// Logs that the user has interacted with the first run promo.
void LogUserInteractionWithFirstRunPromo(BOOL openedSettings);

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

// Return YES if the user has seen a promo recently, and shouldn't
// see another one.
bool UserInPromoCooldown();

// List of all key used to store data in NSUserDefaults. Still used as key
// in the NSDictionary stored under `kBrowserDefaultsKey`.
const NSArray<NSString*>* DefaultBrowserUtilsLegacyKeysForTesting();

// Returns YES if the app has launched on cold start under
// `kTimestampAppLaunchOnColdStart`.
bool HasAppLaunchedOnColdStartAndRecordsLaunch();

// Return true if the default browser promo should be registered with the promo
// manager to display a default browser promo.
bool ShouldRegisterPromoWithPromoManager(bool is_signed_in);

// Returns true if it was determined that the user is eligible for a
// tailored promo.
bool IsTailoredPromoEligibleUser(bool is_signed_in);

// Returns true if it was determined that the user is eligible for the
// general promo.
bool IsGeneralPromoEligibleUser(bool is_signed_in);

// Return true if it was determined that the user is eligible for the
// video promo.
bool IsVideoPromoEligibleUser(feature_engagement::Tracker* tracker);

// Removes unused data from NSUserDefaults. This method should be periodically
// pruned of cleanups that have been present for multiple milestones.
void CleanupUnusedStorage();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_UTILS_H_
