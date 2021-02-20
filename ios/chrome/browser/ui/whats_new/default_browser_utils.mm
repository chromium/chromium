// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/default_browser_utils.h"

#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/metrics/field_trial.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <UIKit/UIKit.h>

namespace {
NSString* const kLastSignificantUserEvent = @"lastSignificantUserEvent";

NSString* const kUserHasInteractedWithFullscreenPromo =
    @"userHasInteractedWithFullscreenPromo";

NSString* const kRemindMeLaterPromoActionInteraction =
    @"remindMeLaterPromoActionInteraction";

const char kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam[] =
    "show_remind_me_later";

const char kDefaultBrowserFullscreenPromoExperimentChangeStringsGroupParam[] =
    "show_switch_description";

const char kDefaultBrowserFullscreenPromoCTAExperimentOpenLinksArm[] =
    "OpenLinks";
const char kDefaultBrowserFullscreenPromoCTAExperimentSwitchArm[] = "Switch";

// Time threshold before activity timestamps should be removed. Currently set to
// seven days.
const NSTimeInterval kUserActivityTimestampExpiration = 7 * 24 * 60 * 60;
// Time threshold for the last URL open before no URL opens likely indicates
// Chrome is no longer the default browser.
const NSTimeInterval kLatestURLOpenForDefaultBrowser = 7 * 24 * 60 * 60;
// Delay for the user to be reshown the fullscreen promo when the user taps on
// the "Remind Me Later" button. 50 hours.
const NSTimeInterval kRemindMeLaterPresentationDelay = 50 * 60 * 60;
}

NSString* const kLastHTTPURLOpenTime = @"lastHTTPURLOpenTime";

// Helper function to clear all timestamps that occur later than 7 days ago.
NSMutableArray<NSDate*>* SanitizePastUserEvents(
    NSMutableArray<NSDate*>* pastUserEvents) {
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

void LogLikelyInterestedDefaultBrowserUserActivity() {
  NSMutableArray<NSDate*>* pastUserEvents =
      [[[NSUserDefaults standardUserDefaults]
          arrayForKey:kLastSignificantUserEvent] mutableCopy];
  if (pastUserEvents) {
    pastUserEvents = SanitizePastUserEvents(pastUserEvents);
    [pastUserEvents addObject:[NSDate date]];
  } else {
    pastUserEvents = [NSMutableArray arrayWithObject:[NSDate date]];
  }

  [[NSUserDefaults standardUserDefaults] setObject:pastUserEvents
                                            forKey:kLastSignificantUserEvent];
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
  return base::FeatureList::IsEnabled(
             kDefaultBrowserFullscreenPromoCTAExperiment) &&
         base::FeatureList::GetFieldTrial(
             kDefaultBrowserFullscreenPromoCTAExperiment)
                 ->group_name() ==
             kDefaultBrowserFullscreenPromoCTAExperimentOpenLinksArm;
}

bool IsInCTASwitchGroup() {
  return base::FeatureList::IsEnabled(
             kDefaultBrowserFullscreenPromoCTAExperiment) &&
         base::FeatureList::GetFieldTrial(
             kDefaultBrowserFullscreenPromoCTAExperiment)
                 ->group_name() ==
             kDefaultBrowserFullscreenPromoCTAExperimentSwitchArm;
}

bool HasUserInteractedWithFullscreenPromoBefore() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kUserHasInteractedWithFullscreenPromo];
}

void LogUserInteractionWithFullscreenPromo() {
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:kUserHasInteractedWithFullscreenPromo];

  if (IsInRemindMeLaterGroup()) {
    // Clear any possible Remind Me Later timestamp saved.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kRemindMeLaterPromoActionInteraction];
  }
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

bool IsLikelyInterestedDefaultBrowserUser() {
  NSMutableArray<NSDate*>* pastUserEvents =
      [[[NSUserDefaults standardUserDefaults]
          arrayForKey:kLastSignificantUserEvent] mutableCopy];
  pastUserEvents = SanitizePastUserEvents(pastUserEvents);
  return [pastUserEvents count] > 1 && base::ios::IsRunningOnIOS14OrLater();
}
