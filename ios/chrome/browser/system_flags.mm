// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file can be empty. Its purpose is to contain the relatively short lived
// definitions required for experimental flags.

#include "ios/chrome/browser/system_flags.h"

#import <Foundation/Foundation.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kAlternateDiscoverFeedServerURL =
    @"AlternateDiscoverFeedServerURL";
NSString* const kDisableDCHECKCrashes = @"DisableDCHECKCrashes";
NSString* const kEnableStartupCrash = @"EnableStartupCrash";
NSString* const kFirstRunForceEnabled = @"FirstRunForceEnabled";
NSString* const kOriginServerHost = @"AlternateOriginServerHost";
NSString* const kWhatsNewPromoStatus = @"WhatsNewPromoStatus";
NSString* const kClearApplicationGroup = @"ClearApplicationGroup";
const base::Feature kEnableThirdPartyKeyboardWorkaround{
    "EnableThirdPartyKeyboardWorkaround", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

namespace experimental_flags {

bool AlwaysDisplayFirstRun() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:kFirstRunForceEnabled];
}

NSString* GetOriginServerHost() {
  return [[NSUserDefaults standardUserDefaults] stringForKey:kOriginServerHost];
}

WhatsNewPromoStatus GetWhatsNewPromoStatus() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSInteger status = [defaults integerForKey:kWhatsNewPromoStatus];
  // If |status| is set to a value greater than or equal to the count of items
  // defined in WhatsNewPromoStatus, set it to |WHATS_NEW_DEFAULT| and correct
  // the value in NSUserDefaults. This error case can happen when a user
  // upgrades to a version with fewer Whats New Promo settings.
  if (status >= static_cast<NSInteger>(WHATS_NEW_PROMO_STATUS_COUNT)) {
    status = static_cast<NSInteger>(WHATS_NEW_DEFAULT);
    [defaults setInteger:status forKey:kWhatsNewPromoStatus];
  }
  return static_cast<WhatsNewPromoStatus>(status);
}

NSString* GetAlternateDiscoverFeedServerURL() {
  return [[NSUserDefaults standardUserDefaults]
      stringForKey:kAlternateDiscoverFeedServerURL];
}

bool ShouldResetNoticeCardOnFeedStart() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"ResetNoticeCard"];
}

bool ShouldResetFirstFollowCount() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:@"ResetFirstFollow"];
}

void DidResetFirstFollowCount() {
  [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"ResetFirstFollow"];
}

bool ShouldAlwaysShowFirstFollow() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"AlwaysShowFirstFollow"];
}

bool ShouldAlwaysShowFollowIPH() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:@"AlwaysShowFollowIPH"];
}

bool IsMemoryDebuggingEnabled() {
// Always return true for Chromium builds, but check the user default for
// official builds because memory debugging should never be enabled on stable.
#if BUILDFLAG(CHROMIUM_BRANDING)
  return true;
#else
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:@"EnableMemoryDebugging"];
#endif  // BUILDFLAG(CHROMIUM_BRANDING)
}

bool IsStartupCrashEnabled() {
  return [[NSUserDefaults standardUserDefaults] boolForKey:kEnableStartupCrash];
}

bool AreDCHECKCrashesDisabled() {
  return
      [[NSUserDefaults standardUserDefaults] boolForKey:kDisableDCHECKCrashes];
}

bool MustClearApplicationGroupSandbox() {
  bool value =
      [[NSUserDefaults standardUserDefaults] boolForKey:kClearApplicationGroup];
  [[NSUserDefaults standardUserDefaults] setBool:NO
                                          forKey:kClearApplicationGroup];
  return value;
}

bool IsThirdPartyKeyboardWorkaroundEnabled() {
  // Check if the experimental flag is forced on or off.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableThirdPartyKeyboardWorkaround)) {
    return true;
  } else if (command_line->HasSwitch(
                 switches::kDisableThirdPartyKeyboardWorkaround)) {
    return false;
  }

  // Check if the Finch experiment is turned on.
  return base::FeatureList::IsEnabled(kEnableThirdPartyKeyboardWorkaround);
}

}  // namespace experimental_flags
