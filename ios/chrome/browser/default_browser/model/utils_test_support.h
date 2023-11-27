// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_UTILS_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_UTILS_TEST_SUPPORT_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

// Clear all default browser promo data for testing.
void ClearDefaultBrowserPromoData();

// Sets a timestamp into NSUserDefaults storage under the default browser utils
// key. Replaces the entire dictionary, so any existing entries are erased.
void ResetStorageAndSetTimestampForKey(NSString* key, base::Time timestamp);

// Overwrites the dictionary under the default browser utils key with the
// provided one.
void SetValuesInStorage(NSDictionary<NSString*, NSObject*>* data);

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_UTILS_TEST_SUPPORT_H_
