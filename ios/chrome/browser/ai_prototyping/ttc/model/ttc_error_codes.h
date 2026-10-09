// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_ERROR_CODES_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_ERROR_CODES_H_

#import <Foundation/Foundation.h>

#import "components/ttc/app/public/error_codes.h"

// Error domain for TTC backend and session errors.
extern NSString* const kTTCErrorDomain;

// Creates an `NSError` in `kTTCErrorDomain` for `error_code`.
NSError* CreateTTCError(ttc::ErrorCode error_code);

// Returns whether `error` is fatal to the session and should terminate it.
bool IsFatalTTCError(NSError* error);

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_ERROR_CODES_H_
