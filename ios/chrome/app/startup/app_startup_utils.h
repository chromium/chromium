// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_APP_STARTUP_UTILS_H_
#define IOS_CHROME_APP_STARTUP_APP_STARTUP_UTILS_H_

#import <Foundation/Foundation.h>

// Checks if the caller app is a first party app.
bool IsCallerAppFirstParty(NSString* caller_app_id);

// Checks if the caller app is allowed for the youtube incognito experiment.
bool IsCallerAppAllowListed(NSString* caller_app_id);

#endif  // IOS_CHROME_APP_STARTUP_APP_STARTUP_UTILS_H_
