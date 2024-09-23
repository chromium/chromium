// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/utils_test_support.h"

// Visible for testing.
extern NSString* const kDefaultBrowserUtilsKey;

void ClearDefaultBrowserPromoData() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in DefaultBrowserUtilsLegacyKeysForTesting()) {
    [defaults removeObjectForKey:key];
  }
  [defaults removeObjectForKey:kDefaultBrowserUtilsKey];
}

void ResetStorageAndSetTimestampForKey(NSString* key, base::Time timestamp) {
  NSMutableDictionary<NSString*, NSObject*>* dict =
      [[NSMutableDictionary alloc] init];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  dict[key] = timestamp.ToNSDate();

  [defaults setObject:dict forKey:kDefaultBrowserUtilsKey];
}

void SetValuesInStorage(NSDictionary<NSString*, NSObject*>* dict) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:dict forKey:kDefaultBrowserUtilsKey];
}

void SimulateUserInteractionWithPromos(const base::TimeDelta& timeAgo,
                                       BOOL interactedWithFRE,
                                       int genericCount,
                                       int tailoredCount,
                                       int totalCount) {
  NSDictionary<NSString*, NSObject*>* values = @{
    kUserHasInteractedWithFirstRunPromo :
        [NSNumber numberWithBool:interactedWithFRE],
    kUserHasInteractedWithFullscreenPromo : genericCount > 0 ? @YES : @NO,
    kUserHasInteractedWithTailoredFullscreenPromo : tailoredCount > 0 ? @YES
                                                                      : @NO,
    kLastTimeUserInteractedWithFullscreenPromo : (base::Time::Now() - timeAgo)
        .ToNSDate(),
    kGenericPromoInteractionCount : [NSNumber numberWithInt:genericCount],
    kTailoredPromoInteractionCount : [NSNumber numberWithInt:tailoredCount],
    kDisplayedFullscreenPromoCount : [NSNumber numberWithInt:totalCount]
  };
  SetValuesInStorage(values);
}

void SimulateUserInterestedDefaultBrowserUserActivity(
    DefaultPromoType type,
    const base::TimeDelta& timeAgo) {
  std::vector<base::Time> times = LoadTimestampsForPromoType(type);
  times.push_back(base::Time::Now() - timeAgo);

  StoreTimestampsForPromoType(type, std::move(times));
}
