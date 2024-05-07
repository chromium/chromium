// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_UTIL_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_UTIL_H_

#import <Foundation/Foundation.h>

class PrefService;

// True if content notification promo is enabled with user visible UI.
// `user_signed_in` is true if the user has signed in.
// `default_search_engine` is true if the user is using Google as default search
// engine.
// `pref_service` is the Pref Service.
bool IsContentNotificationPromoEnabled(bool user_signed_in,
                                       bool default_search_engine,
                                       PrefService* pref_service);

// True if content notification provisional is enabled with user visible UI.
// `user_signed_in` is true if the user has signed in.
// `default_search_engine` is true if the user is using Google as default search
// engine.
// `pref_service` is the Pref Service.
bool IsContentNotificationProvisionalEnabled(bool user_signed_in,
                                             bool default_search_engine,
                                             PrefService* pref_service);

// True if content notification promo in Set Up List is enabled with user
// visible UI.
// `user_signed_in` is true if the user has signed in.
// `default_search_engine` is true if the user is using Google as default search
// engine.
// `pref_service` is the Pref Service.
bool IsContentNotificationSetUpListEnabled(bool user_signed_in,
                                           bool default_search_engine,
                                           PrefService* pref_service);

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_UTIL_H_
