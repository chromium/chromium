// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_UTILS_H_

#import <UIKit/UIKit.h>

// Enum for the different types of default browser modal promo. These are stored
// as values, if adding a new one, make sure to add it at the end.
typedef NS_ENUM(NSUInteger, DefaultPromoType) {
  DefaultPromoTypeGeneral = 0,
  DefaultPromoTypeStaySafe = 1,
  DefaultPromoTypeMadeForIOS = 2,
  DefaultPromoTypeAllTabs = 3
};

// UserDefaults key that saves the last time an HTTP(S) link was sent and opened
// by the app.
extern NSString* const kLastHTTPURLOpenTime;

// The feature parameter to indicate the open links arm.
extern const char kDefaultBrowserFullscreenPromoCTAExperimentOpenLinksParam[];

// The feature parameter to indicate the switch arm.
extern const char kDefaultBrowserFullscreenPromoCTAExperimentSwitchParam[];

// Indicates if the tailored variant "Built for iOS" is enabled. It is not
// mutually exclusive with other tailored promos.
extern const char kDefaultPromoTailoredVariantIOSParam[];

// Indicates if the tailored variant "Stay Safe With Google Chrome" is enabled.
// It is not mutually exclusive with other tailored promos.
extern const char kDefaultPromoTailoredVariantSafeParam[];

// Indicates if the tailored variant "All Your Tabs In One Browser" is enabled.
// It is not mutually exclusive with other tailored promos.
extern const char kDefaultPromoTailoredVariantTabsParam[];

// Indicates the timeout duration for the non-modal promo.
extern const char kDefaultPromoNonModalTimeoutParam[];

// Indicates if the instructions for the non-modal promo are enabled.
extern const char kDefaultPromoNonModalInstructionsParam[];

// Logs the timestamp of user activity that is deemed to be an indication of
// a user that would likely benefit from having Chrome set as their default
// browser. Before logging the current activity, this method will also clear all
// past expired logs for |type| that have happened too far in the past.
void LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoType type);

// Logs the timestamp of a user tap on the "Remind Me Later" button in the
// Fullscreen Promo.
void LogRemindMeLaterPromoActionInteraction();

// Returns true if the user has tapped on the "Remind Me Later" button and the
// delay time threshold has been met.
bool ShouldShowRemindMeLaterDefaultBrowserFullscreenPromo();

// Returns true if the user is in the group that will be shown the Remind Me
// Later button in the fullscreen promo.
bool IsInRemindMeLaterGroup();

// Returns true if the user is in the group that will be shown a modified
// description and "Learn More" text.
bool IsInModifiedStringsGroup();

// Returns true if the user is in the CTA experiment in the open links group.
bool IsInCTAOpenLinksGroup();

// Returns true if the user is in the CTA experiment in the switch group.
bool IsInCTASwitchGroup();

// Returns true if non modals default browser promos are enabled.
bool NonModalPromosEnabled();

// Returns the timeout for non modals.
double NonModalPromosTimeout();

// Returns true if instructions for non modals should be shown.
bool NonModalPromosInstructionsEnabled();

// Returns true if tailored default browser promo variant "Built for iOS" is
// enabled.
bool IOSTailoredPromoEnabled();

// Returns true if tailored default browser promo variant "Stay Safe With Google
// Chrome" is enabled.
bool SafeTailoredPromoEnabled();

// Returns true if tailored default browser promo variant "All Your Tabs In One
// Browser" is enabled.
bool TabsTailoredPromoEnabled();

// Returns true if the user has interacted with the Fullscreen Promo previously.
// Returns false otherwise.
bool HasUserInteractedWithFullscreenPromoBefore();

// Logs that the user has interacted with the Fullscreen Promo.
void LogUserInteractionWithFullscreenPromo();

// Returns true if the last URL open is within the time threshold that would
// indicate Chrome is likely still the default browser. Returns false otherwise.
bool IsChromeLikelyDefaultBrowser();

// Returns true if the past behavior of the user indicates that the user fits
// the categorization that would likely benefit from having Chrome set as their
// default browser for any promo type. Returns false otherwise.
bool IsLikelyInterestedDefaultBrowserUser();

// Returns true if the past behavior of the user indicates that the user fits
// the categorization that would likely benefit from having Chrome set as their
// default browser for the passed |type|. Returns false otherwise.
bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType type);

// Returns the most recent promo the user showed interest in. Defaults to
// DefaultPromoTypeGeneral if no interest is found.
DefaultPromoType MostRecentInterestDefaultPromoType();

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_UTILS_H_
