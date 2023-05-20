// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/signin_settings_app_interface.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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
