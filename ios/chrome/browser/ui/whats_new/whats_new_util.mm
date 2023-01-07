// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/whats_new/feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key to store whether a user interacted with What's New from the overflow
// menu.
NSString* const kOverflowMenuEntryKey = @"userHasInteractedWithWhatsNew";

// Time interval of 6 days. This is used to calculate 6 days after FRE to
// trigger What's New Promo.
const NSTimeInterval kSixDays = 6 * 24 * 60 * 60;

// Returns whether today is the 6th and more day after the FRE. This is used to
// decide to register What's New promo in the promo manager or not.
bool IsSixDaysAfterFre() {
  NSDate* startDate = [[NSUserDefaults standardUserDefaults]
      objectForKey:kWhatsNewDaysAfterFre];
  if (!startDate) {
    [[NSUserDefaults standardUserDefaults] setObject:[NSDate date]
                                              forKey:kWhatsNewDaysAfterFre];
    return false;
  }

  NSDate* sixDaysAgoDate = [NSDate dateWithTimeIntervalSinceNow:-kSixDays];
  if ([sixDaysAgoDate compare:startDate] == NSOrderedDescending) {
    return true;
  }
  return false;
}

// Returns whether this launch is the 6th and more launches after the FRE. This
// is used to decide to register What's New promo in the promo manager or not.
bool IsSixLaunchAfterFre() {
  NSInteger num = [[NSUserDefaults standardUserDefaults]
      integerForKey:kWhatsNewLaunchesAfterFre];

  if (num >= 6) {
    return true;
  }

  num++;
  [[NSUserDefaults standardUserDefaults] setInteger:num
                                             forKey:kWhatsNewLaunchesAfterFre];
  return false;
}

// Returns whether What's New promo has been registered in the promo manager.
bool IsWhatsNewPromoRegistered() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kWhatsNewPromoRegistrationKey];
}

}  // namespace

NSString* const kWhatsNewPromoRegistrationKey = @"whatsNewPromoRegistration";

NSString* const kWhatsNewDaysAfterFre = @"whatsNewDaysAfterFre";

NSString* const kWhatsNewLaunchesAfterFre = @"whatsNewLaunchesAfterFre";

bool IsWhatsNewOverflowMenuUsed() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:kOverflowMenuEntryKey];
}

void SetWhatsNewOverflowMenuUsed() {
  if (IsWhatsNewOverflowMenuUsed())
    return;

  [[NSUserDefaults standardUserDefaults] setBool:YES
                                          forKey:kOverflowMenuEntryKey];
}

bool IsWhatsNewEnabled() {
  return base::FeatureList::IsEnabled(kWhatsNewIOS);
}

void setWhatsNewPromoRegistration() {
  [[NSUserDefaults standardUserDefaults] setBool:YES
                                          forKey:kWhatsNewPromoRegistrationKey];
}

bool ShouldRegisterWhatsNewPromo() {
  return !IsWhatsNewPromoRegistered() &&
         (IsSixLaunchAfterFre() || IsSixDaysAfterFre());
}
