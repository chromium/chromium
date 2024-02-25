// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FIRST_RUN_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FIRST_RUN_TEST_UTIL_H_

#import <UIKit/UIKit.h>

/// Forces time since first run to `number_of_days`.
void ForceFirstRunRecency(NSInteger number_of_days);

/// Removes the first run sentinel and resets the state of first run.
void ResetFirstRunSentinel();

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_FIRST_RUN_TEST_UTIL_H_
