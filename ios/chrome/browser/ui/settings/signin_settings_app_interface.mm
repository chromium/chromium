// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/signin_settings_app_interface.h"

#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninSettingsAppInterface

+ (void)setSettingsSigninPromoDisplayedCount:(int)displayedCount {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefService = browserState->GetPrefs();
  prefService->SetInteger(prefs::kIosSettingsSigninPromoDisplayedCount,
                          displayedCount);
}

+ (int)settingsSigninPromoDisplayedCount {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* prefService = browserState->GetPrefs();
  return prefService->GetInteger(prefs::kIosSettingsSigninPromoDisplayedCount);
}

@end
