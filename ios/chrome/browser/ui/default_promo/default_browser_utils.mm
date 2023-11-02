// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/field_trial_params.h"
#import "base/notreached.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

using base::mac::ObjCCast;

namespace {

// Key for NSUserDefaults containing an array of dates. Each date correspond to
// a general event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventGeneral = @"lastSignificantUserEvent";

// Key for NSUserDefaults containing an array of dates. Each date correspond to
// a stay safe event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventStaySafe =
    @"lastSignificantUserEventStaySafe";

// Key for NSUserDefaults containing an array of dates. Each date correspond to
// a made for iOS event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventMadeForIOS =
    @"lastSignificantUserEventMadeForIOS";

// Key for NSUserDefaults containing an array of dates. Each date correspond to
// an all tabs event of interest for Default Browser Promo modals.
NSString* const kLastSignificantUserEventAllTabs =
    @"lastSignificantUserEventAllTabs";

// Key for NSUserDefaults containing an NSDate indicating the last time a user
// interacted with ANY promo. The string value is kept from when the promos
// first launched to avoid changing the behavior for users that have already
// seen the promo.
NSString* const kLastTimeUserInteractedWithPromo =
    @"lastTimeUserInteractedWithFullscreenPromo";

// Key for NSUserDefaults containing a bool indicating if the user has
// previously interacted with a regular fullscreen promo.
NSString* const kUserHasInteractedWithFullscreenPromo =
    @"userHasInteractedWithFullscreenPromo";

// Key for NSUserDefaults containing a bool indicating if the user has
// previously interacted with a tailored fullscreen promo.
NSString* const kUserHasInteractedWithTailoredFullscreenPromo =
    @"userHasInteractedWithTailoredFullscreenPromo";

// Key for NSUserDefaults containing a bool indicating if the user has
// previously interacted with first run promo.
NSString* const kUserHasInteractedWithFirstRunPromo =
    @"userHasInteractedWithFirstRunPromo";

// Key for NSUserDefaults containing an int indicating the number of times the
// user has interacted with a non-modal promo.
NSString* const kUserInteractedWithNonModalPromoCount =
    @"userInteractedWithNonModalPromoCount";

// Key for NSUserDefaults containing an int indicating the number of times a
// promo has been displayed.
NSString* const kDisplayedPromoCount = @"displayedPromoCount";

NSString* const kRemindMeLaterPromoActionInteraction =
    @"remindMeLaterPromoActionInteraction";

// Key for NSUserDefaults containing a bool indicating if the user tapped on
// button to open settings.
NSString* const kOpenSettingsActionInteraction =
    @"openSettingsActionInteraction";

const char kDefaultBrowserFullscreenPromoExperimentChangeStringsGroupParam[] =
    "show_switch_description";

// Time threshold before activity timestamps should be removed. Currently set to
// 21 days.
const NSTimeInterval kUserActivityTimestampExpiration = 21 * 24 * 60 * 60;
// Time threshold for the last URL open before no URL opens likely indicates
// Chrome is no longer the default browser.
const NSTimeInterval kLatestURLOpenForDefaultBrowser = 21 * 24 * 60 * 60;
// Delay for the user to be reshown the fullscreen promo when the user taps on
// the "Remind Me Later" button. 50 hours.
const NSTimeInterval kRemindMeLaterPresentationDelay = 50 * 60 * 60;

// Cool down between fullscreen promos. Currently set to 14 days.
const NSTimeInterval kFullscreenPromoCoolDown = 14 * 24 * 60 * 60;

// Short cool down between promos. Currently set to 3 days.
const NSTimeInterval kPromosShortCoolDown = 3 * 24 * 60 * 60;

// Helper function to clear all timestamps that occur later than 21 days ago and
// keep it only to 10 timestamps.
NSMutableArray<NSDate*>* SanitizePastUserEvents(
    NSMutableArray<NSDate*>* pastUserEvents) {
  // First, keep the array to 10 items:
  NSInteger count = pastUserEvents.count;
  if (count > 10) {
    [pastUserEvents removeObjectsInRange:NSMakeRange(0, count - 10)];
  }

  // Next, remove items older than a week:
  NSDate* sevenDaysAgoDate =
      [NSDate dateWithTimeIntervalSinceNow:-kUserActivityTimestampExpiration];
  NSUInteger firstUnexpiredIndex = [pastUserEvents
      indexOfObjectPassingTest:^BOOL(NSDate* date, NSUInteger idx, BOOL* stop) {
        return ([date laterDate:sevenDaysAgoDate] == date);
      }];
  if (firstUnexpiredIndex != NSNotFound && firstUnexpiredIndex > 0) {
    [pastUserEvents removeObjectsInRange:NSMakeRange(0, firstUnexpiredIndex)];
  }
  return pastUserEvents;
}

// Helper function get the NSUserDefaults key for a specific promo type.
NSString* NSUserDefaultKeyForType(DefaultPromoType type) {
  switch (type) {
    case DefaultPromoTypeGeneral:
      return kLastSignificantUserEventGeneral;
    case DefaultPromoTypeMadeForIOS:
      return kLastSignificantUserEventMadeForIOS;
    case DefaultPromoTypeAllTabs:
      return kLastSignificantUserEventAllTabs;
    case DefaultPromoTypeStaySafe:
      return kLastSignificantUserEventStaySafe;
  }
  NOTREACHED();
  return nil;
}

// Returns the most recent event for a given promo type or nil if none.
NSDate* MostRecentDateForType(DefaultPromoType type) {
  NSString* key = NSUserDefaultKeyForType(type);
  NSMutableArray<NSDate*>* pastUserEvents =
      [[[NSUserDefaults standardUserDefaults] arrayForKey:key] mutableCopy];
  return pastUserEvents.lastObject;
}
}  // namespace

#pragma mark - Private

// `YES` if user interacted with the first run default browser screen.
BOOL HasUserInteractedWithFirstRunPromoBefore() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kUserHasInteractedWithFirstRunPromo];
}

// Returns the number of time a default browser promo has been displayed.
NSInteger DisplayedPromoCount() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  return [standardDefaults integerForKey:kDisplayedPromoCount];
}

// Adds one to displayed default browser promo count.
void AddOneToDisplayedPromoCount() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  NSInteger currentDisplayedPromoCount =
      [standardDefaults integerForKey:kDisplayedPromoCount];
  [standardDefaults setInteger:currentDisplayedPromoCount + 1
                        forKey:kDisplayedPromoCount];
}

// Computes cool down between promos.
NSTimeInterval ComputeCooldown() {
  // `true` if the user is in the short delay group experiment and tap on the
  // "No thanks" button in first run default browser screen. Short cool down
  // should be set only one time, so after the first run promo there is a short
  // cool down before the next promo and after it goes back to normal.
  if (DisplayedPromoCount() < 2 && HasUserInteractedWithFirstRunPromoBefore() &&
      !HasUserOpenedSettingsFromFirstRunPromo()) {
    return kPromosShortCoolDown;
  }
  return kFullscreenPromoCoolDown;
}

#pragma mark - Public

NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";

const char kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam[] =
    "show_remind_me_later";

void LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoType type) {
  NSString* key = NSUserDefaultKeyForType(type);
  NSDate* date = [NSDate date];
  NSMutableArray<NSDate*>* pastUserEvents =
      [[[NSUserDefaults standardUserDefaults] arrayForKey:key] mutableCopy];
  if (pastUserEvents) {
    pastUserEvents = SanitizePastUserEvents(pastUserEvents);
    [pastUserEvents addObject:date];
  } else {
    pastUserEvents = [@[ date ] mutableCopy];
  }

  [[NSUserDefaults standardUserDefaults] setObject:pastUserEvents forKey:key];
}

void LogRemindMeLaterPromoActionInteraction() {
  DCHECK(IsInRemindMeLaterGroup());
  [[NSUserDefaults standardUserDefaults]
      setObject:[NSDate date]
         forKey:kRemindMeLaterPromoActionInteraction];
}

bool ShouldShowRemindMeLaterDefaultBrowserFullscreenPromo() {
  if (!IsInRemindMeLaterGroup()) {
    return false;
  }
  NSDate* remindMeTimestamp = [[NSUserDefaults standardUserDefaults]
      objectForKey:kRemindMeLaterPromoActionInteraction];
  if (!remindMeTimestamp) {
    return false;
  }
  NSDate* fiftyHoursAgoDate =
      [NSDate dateWithTimeIntervalSinceNow:-kRemindMeLaterPresentationDelay];
  return [remindMeTimestamp laterDate:fiftyHoursAgoDate] == fiftyHoursAgoDate;
}

bool IsInRemindMeLaterGroup() {
  std::string paramValue = base::GetFieldTrialParamValueByFeature(
      kDefaultBrowserFullscreenPromoExperiment,
      kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam);
  return !paramValue.empty();
}

bool IsInModifiedStringsGroup() {
  std::string paramValue = base::GetFieldTrialParamValueByFeature(
      kDefaultBrowserFullscreenPromoExperiment,
      kDefaultBrowserFullscreenPromoExperimentChangeStringsGroupParam);
  return !paramValue.empty();
}

bool NonModalPromosEnabled() {
  // Default browser isn't enabled until iOS 14.0.1, regardless of flag state.
  return base::ios::IsRunningOnOrLater(14, 0, 1);
}

bool HasUserInteractedWithFullscreenPromoBefore() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kUserHasInteractedWithFullscreenPromo];
}

bool HasUserInteractedWithTailoredFullscreenPromoBefore() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kUserHasInteractedWithTailoredFullscreenPromo];
}

BOOL HasUserOpenedSettingsFromFirstRunPromo() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kOpenSettingsActionInteraction];
}

int UserInteractionWithNonModalPromoCount() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  return [standardDefaults integerForKey:kUserInteractedWithNonModalPromoCount];
}

void LogUserInteractionWithFullscreenPromo() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:YES forKey:kUserHasInteractedWithFullscreenPromo];
  [standardDefaults setObject:[NSDate date]
                       forKey:kLastTimeUserInteractedWithPromo];

  if (IsInRemindMeLaterGroup()) {
    // Clear any possible Remind Me Later timestamp saved.
    [standardDefaults removeObjectForKey:kRemindMeLaterPromoActionInteraction];
  }
  AddOneToDisplayedPromoCount();
}

void LogUserInteractionWithTailoredFullscreenPromo() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:YES
                     forKey:kUserHasInteractedWithTailoredFullscreenPromo];
  [standardDefaults setObject:[NSDate date]
                       forKey:kLastTimeUserInteractedWithPromo];
  AddOneToDisplayedPromoCount();
}

void LogUserInteractionWithNonModalPromo() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  int currentInteractionCount =
      [standardDefaults integerForKey:kUserInteractedWithNonModalPromoCount];
  [standardDefaults setInteger:currentInteractionCount + 1
                        forKey:kUserInteractedWithNonModalPromoCount];
  [standardDefaults setObject:[NSDate date]
                       forKey:kLastTimeUserInteractedWithPromo];
  AddOneToDisplayedPromoCount();
}

void LogUserInteractionWithFirstRunPromo(BOOL openedSettings) {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:YES forKey:kUserHasInteractedWithFirstRunPromo];
  [standardDefaults setObject:[NSDate date]
                       forKey:kLastTimeUserInteractedWithPromo];
  [standardDefaults setBool:openedSettings
                     forKey:kOpenSettingsActionInteraction];
  AddOneToDisplayedPromoCount();
}

bool IsChromeLikelyDefaultBrowser7Days() {
  NSDate* lastURLOpen =
      [[NSUserDefaults standardUserDefaults] objectForKey:kLastHTTPURLOpenTime];
  if (!lastURLOpen) {
    return false;
  }
  NSTimeInterval sevenDays = 7 * 24 * 60 * 60;
  NSDate* sevenDaysAgoDate = [NSDate dateWithTimeIntervalSinceNow:-sevenDays];
  if ([lastURLOpen laterDate:sevenDaysAgoDate] == sevenDaysAgoDate) {
    return false;
  }
  return true;
}

bool IsChromeLikelyDefaultBrowser() {
  NSDate* lastURLOpen =
      [[NSUserDefaults standardUserDefaults] objectForKey:kLastHTTPURLOpenTime];
  if (!lastURLOpen) {
    return false;
  }

  NSDate* lookBackDate =
      [NSDate dateWithTimeIntervalSinceNow:-kLatestURLOpenForDefaultBrowser];
  if ([lastURLOpen laterDate:lookBackDate] == lookBackDate) {
    return false;
  }
  return true;
}

bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType type) {
  NSString* key = NSUserDefaultKeyForType(type);
  NSMutableArray<NSDate*>* pastUserEvents =
      [[[NSUserDefaults standardUserDefaults] arrayForKey:key] mutableCopy];
  pastUserEvents = SanitizePastUserEvents(pastUserEvents);
  return [pastUserEvents count] > 0;
}

DefaultPromoType MostRecentInterestDefaultPromoType(BOOL skipAllTabsPromoType) {
  DefaultPromoType mostRecentType = DefaultPromoTypeGeneral;
  NSDate* mostRecentDate = [NSDate distantPast];
  NSArray* promoTypes = @[
    @(DefaultPromoTypeStaySafe), @(DefaultPromoTypeAllTabs),
    @(DefaultPromoTypeMadeForIOS)
  ];

  for (NSNumber* wrappedType in promoTypes) {
    DefaultPromoType type =
        static_cast<DefaultPromoType>(wrappedType.unsignedIntegerValue);

    // Since DefaultPromoTypeAllTabs has extra requirements (user signed in),
    // it needs to be skipped if those are not met.
    if (type == DefaultPromoTypeAllTabs && skipAllTabsPromoType) {
      continue;
    }
    if (IsLikelyInterestedDefaultBrowserUser(type)) {
      NSDate* interestDate = MostRecentDateForType(type);
      if (interestDate &&
          [interestDate laterDate:mostRecentDate] == interestDate) {
        mostRecentDate = interestDate;
        mostRecentType = type;
      }
    }
  }
  return mostRecentType;
}

BOOL UserInPromoCooldown() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  NSDate* lastFullscreenInteraction = ObjCCast<NSDate>(
      [standardDefaults objectForKey:kLastTimeUserInteractedWithPromo]);
  if (lastFullscreenInteraction) {
    NSDate* coolDownDate =
        [NSDate dateWithTimeIntervalSinceNow:-ComputeCooldown()];
    if ([coolDownDate laterDate:lastFullscreenInteraction] ==
        lastFullscreenInteraction) {
      return YES;
    }
  }
  return NO;
}
