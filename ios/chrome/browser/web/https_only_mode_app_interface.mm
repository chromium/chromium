// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/https_only_mode_app_interface.h"

#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#include "ios/components/security_interstitials/https_only_mode/https_only_mode_upgrade_tab_helper.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HttpsOnlyModeAppInterface

+ (void)setHTTPSPortForTesting:(int)HTTPSPortForTesting {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state)
      ->SetHttpsPortForTesting(HTTPSPortForTesting);
}

+ (void)setHTTPPortForTesting:(int)HTTPPortForTesting {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state)->SetHttpPortForTesting(
      HTTPPortForTesting);
}

+ (void)useFakeHTTPSForTesting:(bool)useFakeHTTPS {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state)
      ->UseFakeHTTPSForTesting(useFakeHTTPS);
}

@end
