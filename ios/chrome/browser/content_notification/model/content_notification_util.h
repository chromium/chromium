// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_UTIL_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class PrefService;

// True if any type of content notification is enabled with user visible UI.
// This method is a util function for classes which own a profile object
// checking if content notifications are enabled.
// `profile` is the profile.
bool IsContentNotificationEnabled(ProfileIOS* profile);

// True if any type of content notification is registered without user visible
// UI. `profile` is the profile.
bool IsContentNotificationRegistered(ProfileIOS* profile);

// True if content notification promo is enabled with user visible UI.
// `user_signed_in` is true if the user has signed in. `default_search_engine`
// is true if the user is using Google as default search engine. `pref_service`
// is the Pref Service.
bool IsContentNotificationPromoEnabled(bool user_signed_in,
                                       bool default_search_engine,
                                       PrefService* pref_service);

// True if content notification provisional is enabled with user visible UI.
// `user_signed_in` is true if the user has signed in. `default_search_engine`
// is true if the user is using Google as default search engine. `pref_service`
// is the Pref Service.
bool IsContentNotificationProvisionalEnabled(bool user_signed_in,
                                             bool default_search_engine,
                                             PrefService* pref_service);

// True if content notification promo in Set Up List is enabled with user
// visible UI. `user_signed_in` is true if the user has signed in.
// `default_search_engine` is true if the user is using Google as default search
// engine. `pref_service` is the Pref Service.
bool IsContentNotificationSetUpListEnabled(bool user_signed_in,
                                           bool default_search_engine,
                                           PrefService* pref_service);

// True if content notification promo is registered without user visible UI.
// `user_signed_in` is true if the user has signed in. `default_search_engine`
// is true if the user is using Google as default search engine. `pref_service`
// is the Pref Service.
bool IsContentNotificationPromoRegistered(bool user_signed_in,
                                          bool default_search_engine,
                                          PrefService* pref_service);

// True if content notification provisional is registered without user
// visible UI. `user_signed_in` is true if the user has signed in.
// `default_search_engine` is true if the user is using Google as default search
// engine. `pref_service` is the Pref Service.
bool IsContentNotificationProvisionalRegistered(bool user_signed_in,
                                                bool default_search_engine,
                                                PrefService* pref_service);

// True if content notification promo in Set Up List is registered without
// user visible UI. `user_signed_in` is true if the user has signed in.
// `default_search_engine` is true if the user is using Google as default search
// engine. `pref_service` is the Pref Service.
bool IsContentNotificationSetUpListRegistered(bool user_signed_in,
                                              bool default_search_engine,
                                              PrefService* pref_service);

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_UTIL_H_
