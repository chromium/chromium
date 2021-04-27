// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

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
// interacted with ANY fullscreen promo.
NSString* const kLastTimeUserInteractedWithFullscreenPromo =
    @"lastTimeUserInteractedWithFullscreenPromo";

// Key for NSUserDefaults containing a bool indicating if the user has
// previously interacted with a regular fullscreen promo.
NSString* const kUserHasInteractedWithFullscreenPromo =
    @"userHasInteractedWithFullscreenPromo";

// Key for NSUserDefaults containing a bool indicating if the user has
// previously interacted with a tailored fullscreen promo.
NSString* const kUserHasInteractedWithTailoredFullscreenPromo =
    @"userHasInteractedWithTailoredFullscreenPromo";

NSString* const kRemindMeLaterPromoActionInteraction =
    @"remindMeLaterPromoActionInteraction";

const char kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam[] =
    "show_remind_me_later";

const char kDefaultBrowserFullscreenPromoExperimentChangeStringsGroupParam[] =
    "show_switch_description";

// Time threshold before activity timestamps should be removed. Currently set to
// seven days.
const NSTimeInterval kUserActivityTimestampExpiration = 7 * 24 * 60 * 60;
// Time threshold for the last URL open before no URL opens likely indicates
// Chrome is no longer the default browser.
const NSTimeInterval kLatestURLOpenForDefaultBrowser = 7 * 24 * 60 * 60;
// Delay for the user to be reshown the fullscreen promo when the user taps on
// the "Remind Me Later" button. 50 hours.
const NSTimeInterval kRemindMeLaterPresentationDelay = 50 * 60 * 60;

// Cool down between fullscreen promos. Currently set to 14 days.
const NSTimeInterval kFullscreenPromoCoolDown = 14 * 24 * 60 * 60;

// Helper function to clear all timestamps that occur later than 7 days ago and
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
}

#pragma mark - Public

NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";

const char kDefaultBrowserFullscreenPromoCTAExperimentOpenLinksParam[] =
    "show_open_links_title";

const char kDefaultBrowserFullscreenPromoCTAExperimentSwitchParam[] =
    "show_switch_title";

const char kDefaultPromoNonModalTimeoutParam[] = "timeout";

const char kDefaultPromoNonModalInstructionsParam[] = "instructions_enabled";

const char kDefaultPromoTailoredVariantIOSParam[] = "variant_ios_enabled";

const char kDefaultPromoTailoredVariantSafeParam[] = "variant_safe_enabled";

const char kDefaultPromoTailoredVariantTabsParam[] = "variant_tabs_enabled";

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

bool IsInCTAOpenLinksGroup() {
  std::string field_trial_param = base::GetFieldTrialParamValueByFeature(
      kDefaultBrowserFullscreenPromoCTAExperiment,
      kDefaultBrowserFullscreenPromoCTAExperimentOpenLinksParam);
  return field_trial_param == "true";
}

bool IsInCTASwitchGroup() {
  std::string field_trial_param = base::GetFieldTrialParamValueByFeature(
      kDefaultBrowserFullscreenPromoCTAExperiment,
      kDefaultBrowserFullscreenPromoCTAExperimentSwitchParam);
  return field_trial_param == "true";
}

bool NonModalPromosEnabled() {
  return base::FeatureList::IsEnabled(kDefaultPromoNonModal);
}

double NonModalPromosTimeout() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kDefaultPromoNonModal, kDefaultPromoNonModalTimeoutParam, 15);
}

bool NonModalPromosInstructionsEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kDefaultPromoNonModal, kDefaultPromoNonModalInstructionsParam, false);
}

bool IOSTailoredPromoEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kDefaultPromoTailored, kDefaultPromoTailoredVariantIOSParam, false);
}

bool SafeTailoredPromoEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kDefaultPromoTailored, kDefaultPromoTailoredVariantSafeParam, false);
}

bool TabsTailoredPromoEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kDefaultPromoTailored, kDefaultPromoTailoredVariantTabsParam, false);
}

bool HasUserInteractedWithFullscreenPromoBefore() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kUserHasInteractedWithFullscreenPromo];
}

bool HasUserInteractedWithTailoredFullscreenPromoBefore() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kUserHasInteractedWithTailoredFullscreenPromo];
}

void LogUserInteractionWithFullscreenPromo() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:YES forKey:kUserHasInteractedWithFullscreenPromo];
  [standardDefaults setObject:[NSDate date]
                       forKey:kLastTimeUserInteractedWithFullscreenPromo];

  if (IsInRemindMeLaterGroup()) {
    // Clear any possible Remind Me Later timestamp saved.
    [standardDefaults removeObjectForKey:kRemindMeLaterPromoActionInteraction];
  }
}

void LogUserInteractionWithTailoredFullscreenPromo() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults setBool:YES
                     forKey:kUserHasInteractedWithTailoredFullscreenPromo];
  [standardDefaults setObject:[NSDate date]
                       forKey:kLastTimeUserInteractedWithFullscreenPromo];
}

bool IsChromeLikelyDefaultBrowser() {
  NSDate* lastURLOpen =
      [[NSUserDefaults standardUserDefaults] objectForKey:kLastHTTPURLOpenTime];
  if (!lastURLOpen) {
    return false;
  }

  NSDate* sevenDaysAgoDate =
      [NSDate dateWithTimeIntervalSinceNow:-kLatestURLOpenForDefaultBrowser];
  if ([lastURLOpen laterDate:sevenDaysAgoDate] == sevenDaysAgoDate) {
    return false;
  }
  return true;
}

bool IsLikelyInterestedDefaultBrowserUser(DefaultPromoType type) {
  if (type == DefaultPromoTypeAllTabs && !TabsTailoredPromoEnabled()) {
    return NO;
  }
  if (type == DefaultPromoTypeStaySafe && !SafeTailoredPromoEnabled()) {
    return NO;
  }
  if (type == DefaultPromoTypeMadeForIOS && !IOSTailoredPromoEnabled()) {
    return NO;
  }
  NSString* key = NSUserDefaultKeyForType(type);
  NSMutableArray<NSDate*>* pastUserEvents =
      [[[NSUserDefaults standardUserDefaults] arrayForKey:key] mutableCopy];
  pastUserEvents = SanitizePastUserEvents(pastUserEvents);
  return [pastUserEvents count] > 0 && base::ios::IsRunningOnIOS14OrLater();
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

BOOL UserInFullscreenPromoCooldown() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  NSDate* lastFullscreenInteraction = ObjCCast<NSDate>([standardDefaults
      objectForKey:kLastTimeUserInteractedWithFullscreenPromo]);
  if (lastFullscreenInteraction) {
    NSDate* coolDownDate =
        [NSDate dateWithTimeIntervalSinceNow:-kFullscreenPromoCoolDown];
    if ([coolDownDate laterDate:lastFullscreenInteraction] ==
        lastFullscreenInteraction) {
      return YES;
    }
  }
  return NO;
}
