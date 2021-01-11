// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_

#import <UIKit/UIKit.h>

// UserDefaults key that saves the last time an HTTP(S) link was sent and opened
// by the app.
extern NSString* const kLastHTTPURLOpenTime;

// Logs the timestamp of user activity that is deemed to be an indication of
// a user that would likely benefit from having Chrome set as their default
// browser. Before logging the current activity, this method will also clear all
// past expired logs that have happened too far in the past.
void LogLikelyInterestedDefaultBrowserUserActivity();

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
// default browser. Returns false otherwise.
bool IsLikelyInterestedDefaultBrowserUser();

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_DEFAULT_BROWSER_UTILS_H_
