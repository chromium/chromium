// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_utils_test_support.h"

#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Visible for testing.
extern NSArray<NSString*>* const kDefaultBrowserUtilsLegacyKeysForTesting;
extern NSString* const kDefaultBrowserUtilsKey;

void ClearDefaultBrowserPromoData() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in kDefaultBrowserUtilsLegacyKeysForTesting) {
    [defaults removeObjectForKey:key];
  }
  [defaults removeObjectForKey:kDefaultBrowserUtilsKey];
}
