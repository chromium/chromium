// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/whats_new/constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Time interval of 6 days. This is used to calculate 6 days after FRE to
// trigger What's New Promo.
const NSTimeInterval kSixDays = 6 * 24 * 60 * 60;

// Returns whether today is the 6th and more day after the FRE. This is used to
// decide to register What's New promo in the promo manager or not.
bool IsSixDaysAfterFre() {
  // TODO(crbug.com/1462404): Clean up unused user defaults and find a better
  // solution to update existing user defaults for future versions of What's
  // New.
  NSString* const daysAfterFre = IsWhatsNewM116Enabled()
                                     ? kWhatsNewM116DaysAfterFre
                                     : kWhatsNewDaysAfterFre;
  NSDate* startDate =
      [[NSUserDefaults standardUserDefaults] objectForKey:daysAfterFre];
  if (!startDate) {
    [[NSUserDefaults standardUserDefaults] setObject:[NSDate date]
                                              forKey:daysAfterFre];
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
  // TODO(crbug.com/1462404): Clean up unused user defaults and find a better
  // solution to update existing user defaults for future versions of What's
  // New.
  NSString* const launchesAfterFre = IsWhatsNewM116Enabled()
                                         ? kWhatsNewM116LaunchesAfterFre
                                         : kWhatsNewLaunchesAfterFre;

  NSInteger num =
      [[NSUserDefaults standardUserDefaults] integerForKey:launchesAfterFre];

  if (num >= 6) {
    return true;
  }

  num++;
  [[NSUserDefaults standardUserDefaults] setInteger:num
                                             forKey:launchesAfterFre];
  return false;
}

// Returns whether What's New promo has been registered in the promo manager.
bool IsWhatsNewPromoRegistered() {
  if (IsWhatsNewM116Enabled()) {
    return [[NSUserDefaults standardUserDefaults]
        boolForKey:kWhatsNewM116PromoRegistrationKey];
  }

  // TODO(crbug.com/1462404): Clean up unused user defaults and find a better
  // solution to update existing user defaults for future versions of What's
  // New.
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kWhatsNewPromoRegistrationKey];
}

}  // namespace

BASE_FEATURE(kWhatsNewIOSM116,
             "WhatsNewIOSM116",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool WasWhatsNewUsed() {
  if (IsWhatsNewM116Enabled()) {
    return [[NSUserDefaults standardUserDefaults]
        boolForKey:kWhatsNewM116UsageEntryKey];
  }

  // TODO(crbug.com/1462404): Clean up unused user defaults and find a better
  // solution to update existing user defaults for future versions of What's
  // New.
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:kWhatsNewUsageEntryKey];
}

void SetWhatsNewUsed(PromosManager* promosManager) {
  if (WasWhatsNewUsed()) {
    return;
  }

  // Deregister What's New promo.
  DCHECK(promosManager);
  promosManager->DeregisterPromo(promos_manager::Promo::WhatsNew);

  if (IsWhatsNewM116Enabled()) {
    [[NSUserDefaults standardUserDefaults] setBool:YES
                                            forKey:kWhatsNewM116UsageEntryKey];
  } else {
    [[NSUserDefaults standardUserDefaults] setBool:YES
                                            forKey:kWhatsNewUsageEntryKey];
  }
}

void setWhatsNewPromoRegistration() {
  if (IsWhatsNewM116Enabled()) {
    [[NSUserDefaults standardUserDefaults]
        setBool:YES
         forKey:kWhatsNewM116PromoRegistrationKey];
    return;
  }

  [[NSUserDefaults standardUserDefaults] setBool:YES
                                          forKey:kWhatsNewPromoRegistrationKey];
}

bool ShouldRegisterWhatsNewPromo() {
  return !IsWhatsNewPromoRegistered() &&
         (IsSixLaunchAfterFre() || IsSixDaysAfterFre());
}

bool IsWhatsNewM116Enabled() {
  return base::FeatureList::IsEnabled(kWhatsNewIOSM116);
}

const char* WhatsNewTypeToString(WhatsNewType type) {
  switch (type) {
    case WhatsNewType::kSearchTabs:
      return "SearchTabs";
    case WhatsNewType::kNewOverflowMenu:
      return "NewOverflowMenu";
    case WhatsNewType::kSharedHighlighting:
      return "SharedHighlighting";
    case WhatsNewType::kAddPasswordManually:
      return "AddPasswordManually";
    case WhatsNewType::kUseChromeByDefault:
      return "UseChromeByDefault";
    case WhatsNewType::kPasswordsInOtherApps:
      return "PasswordsInOtherApps";
    case WhatsNewType::kAutofill:
      return "Autofill";
    case WhatsNewType::kIncognitoTabsFromOtherApps:
      return "IncognitoTabsFromOtherApps";
    case WhatsNewType::kIncognitoLock:
      return "IncognitoLock";
    case WhatsNewType::kCalendarEvent:
      return "CalendarEvent";
    case WhatsNewType::kChromeActions:
      return "ChromeActions";
    case WhatsNewType::kMiniMaps:
      return "MiniMaps";
    default:
      return nil;
  };
}

const char* WhatsNewTypeToStringM116(WhatsNewType type) {
  switch (type) {
    case WhatsNewType::kIncognitoTabsFromOtherApps:
      return "IncognitoTabsFromOtherApps";
    case WhatsNewType::kIncognitoLock:
      return "IncognitoLock";
    case WhatsNewType::kCalendarEvent:
      return "CalendarEvent";
    case WhatsNewType::kChromeActions:
      return "ChromeActions";
    case WhatsNewType::kMiniMaps:
      return "MiniMaps";
    default:
      return nil;
  };
}
