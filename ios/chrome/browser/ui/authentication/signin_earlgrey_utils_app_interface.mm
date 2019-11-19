// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/earl_grey/earl_grey_app.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SignInEarlGreyUtilsAppInterface

+ (void)addIdentity:(ChromeIdentity*)identity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);
}

+ (NSString*)primaryAccountGaiaID {
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  CoreAccountInfo info =
      IdentityManagerFactory::GetForBrowserState(browser_state)
          ->GetPrimaryAccountInfo();

  return base::SysUTF8ToNSString(info.gaia);
}

+ (BOOL)isSignedOut {
  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();

  return !IdentityManagerFactory::GetForBrowserState(browser_state)
              ->HasPrimaryAccount();
}

+ (id<GREYMatcher>)identityCellMatcherForEmail:(NSString*)email {
  return grey_allOf(grey_accessibilityID(email),
                    grey_kindOfClass([IdentityChooserCell class]),
                    grey_sufficientlyVisible(), nil);
}

@end
