// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTENTS_MODEL_USER_ACTIVITY_COMPATIBILITY_UTIL_H_
#define IOS_CHROME_BROWSER_INTENTS_MODEL_USER_ACTIVITY_COMPATIBILITY_UTIL_H_

#import <Foundation/Foundation.h>

class PrefService;

// Returns YES if the user activity can be handled by the browser.
BOOL ProceedWithUserActivity(NSUserActivity* user_activity,
                             PrefService* pref_service);

#endif  // IOS_CHROME_BROWSER_INTENTS_MODEL_USER_ACTIVITY_COMPATIBILITY_UTIL_H_
