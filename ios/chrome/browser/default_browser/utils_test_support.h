// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_UTILS_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_UTILS_TEST_SUPPORT_H_

#import <Foundation/Foundation.h>

// Clear all default browser promo data for testing.
void ClearDefaultBrowserPromoData();

// Sets an object into NSUserDefaults storage under the default browser utils
// key.
void SetObjectInStorageForKey(NSString* key, NSObject* data);

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_UTILS_TEST_SUPPORT_H_
