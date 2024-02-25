// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_VARIATIONS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_VARIATIONS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// The app interface for variations tests.
@interface VariationsAppInterface : NSObject

// Clears all local state variations prefs.
+ (void)clearVariationsPrefs;

// Returns true if there is a field trial for the test seed study.
+ (BOOL)fieldTrialExistsForTestSeed;

// Returns true if the variations safe seed pref is set.
+ (BOOL)hasSafeSeed;

// Sets a test safe seed and signature pair.
+ (void)setTestSafeSeedAndSignature;

// Sets a regular (i.e., non-safe) seed and signature pair which cause a crash
// on startup.
+ (void)setCrashingRegularSeedAndSignature;

// Returns the value of the variations crash streak pref.
+ (int)crashStreak;

// Sets the variations crash streak pref to `value`.
+ (void)setCrashValue:(int)value;

// Returns the value of the variations failed fetch streak pref.
+ (int)failedFetchStreak;

// Sets the variations fetch failure pref to `value`.
+ (void)setFetchFailureValue:(int)value;

@end

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_VARIATIONS_APP_INTERFACE_H_
