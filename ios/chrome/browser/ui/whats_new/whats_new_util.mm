// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/whats_new/constants.h"

namespace {

// Clean up user defaults.
// TODO(crbug.com/40274920): Safe to remove in M123+.
void CleanUpWhatsNewUserDefaults() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  [defaults removeObjectForKey:kWhatsNewM116PromoRegistrationKey];
  [defaults removeObjectForKey:kWhatsNewPromoRegistrationKey];
  [defaults removeObjectForKey:kWhatsNewDaysAfterFre];
  [defaults removeObjectForKey:kWhatsNewLaunchesAfterFre];
  [defaults removeObjectForKey:kWhatsNewM116DaysAfterFre];
  [defaults removeObjectForKey:kWhatsNewM116LaunchesAfterFre];
}

}  // namespace

// For users who have already viewed What's New M116, the ensures that the promo
// is not triggered again until the next version of What's New.
// Note that we no longer write userDefault.
bool WasWhatsNewUsed() {
  CleanUpWhatsNewUserDefaults();

  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kWhatsNewM116UsageEntryKey];
}

// Please do not modify this method. The content is updated by script. For more
// info, please see `tools/whats_new`.
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
    case WhatsNewType::kLensSearch:
      return "LensSearch";
    case WhatsNewType::kBottomOmnibox:
      return "BottomOmnibox";
    case WhatsNewType::kESB:
      return "ESB";
    case WhatsNewType::kPWMWidget:
      return "PWMWidget";
    case WhatsNewType::kPinnedTabs:
      return "PinnedTabs";
    case WhatsNewType::kInactiveTabs:
      return "InactiveTabs";
    case WhatsNewType::kError:
      return nil;
  };
}
