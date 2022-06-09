// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/https_only_mode_app_interface.h"

#include "base/time/time.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/https_upgrades/https_only_mode_upgrade_tab_helper.h"
#include "ios/chrome/browser/https_upgrades/https_upgrade_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HttpsOnlyModeAppInterface

+ (void)setHTTPSPortForTesting:(int)HTTPSPort useFakeHTTPS:(bool)useFakeHTTPS {
  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetHttpsPortForTesting(HTTPSPort, useFakeHTTPS);

  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetCurrentIncognitoBrowserState())
      ->SetHttpsPortForTesting(HTTPSPort, useFakeHTTPS);
}

+ (void)setFallbackDelayForTesting:(int)fallbackDelayInMilliseconds {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state)
      ->SetFallbackDelayForTesting(
          base::Milliseconds(fallbackDelayInMilliseconds));
}

+ (BOOL)isTimerRunning {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  return HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state)
      ->IsTimerRunningForTesting();
}

+ (void)clearAllowlist {
  // Clear the persistent allowlist.
  HostContentSettingsMap::PatternSourcePredicate pattern_filter;
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->ClearSettingsForOneType(ContentSettingsType::HTTP_ALLOWED);
  // Clear the temporary allowlist for incognito.
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state)
      ->ClearAllowlistForTesting();
}

@end
