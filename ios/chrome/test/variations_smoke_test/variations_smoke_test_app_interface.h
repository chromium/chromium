// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_VARIATIONS_SMOKE_TEST_VARIATIONS_SMOKE_TEST_APP_INTERFACE_H_
#define IOS_CHROME_TEST_VARIATIONS_SMOKE_TEST_VARIATIONS_SMOKE_TEST_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for variations smoke test.
@interface VariationsSmokeTestAppInterface : NSObject

// Non-empty variations seed signature & compressed seed appears in Local State
// prefs.
+ (BOOL)variationsSeedInLocalStatePrefs;

// Returns true when variations seed last fetch time appears in Local State
// prefs and the fetch time is after current app process start time.
+ (BOOL)variationsSeedFetchedInCurrentLaunch;

// Lands pending writes of Local State Prefs to disk.
+ (void)localStatePrefsCommitPendingWrite;

@end

#endif  // IOS_CHROME_TEST_VARIATIONS_SMOKE_TEST_VARIATIONS_SMOKE_TEST_APP_INTERFACE_H_
