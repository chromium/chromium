// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/utils_test_support.h"

#import "ios/chrome/browser/default_browser/utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Visible for testing.
extern NSString* const kDefaultBrowserUtilsKey;

void ClearDefaultBrowserPromoData() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in DefaultBrowserUtilsLegacyKeysForTesting()) {
    [defaults removeObjectForKey:key];
  }
  [defaults removeObjectForKey:kDefaultBrowserUtilsKey];
}

void SetObjectInStorageForKey(NSString* key, NSObject* data) {
  NSMutableDictionary<NSString*, NSObject*>* dict =
      [[NSMutableDictionary alloc] init];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  dict[key] = data;

  [defaults setObject:dict forKey:kDefaultBrowserUtilsKey];
}
