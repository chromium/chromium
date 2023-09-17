// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/https_upgrade_app_interface.h"

#import "base/time/time.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/https_upgrades/https_only_mode_upgrade_tab_helper.h"
#import "ios/chrome/browser/https_upgrades/https_upgrade_service_factory.h"
#import "ios/chrome/browser/https_upgrades/typed_navigation_upgrade_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

@implementation HttpsUpgradeAppInterface

+ (void)setHTTPSPortForTesting:(int)HTTPSPort useFakeHTTPS:(bool)useFakeHTTPS {
  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetHttpsPortForTesting(HTTPSPort, useFakeHTTPS);

  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetCurrentIncognitoBrowserState())
      ->SetHttpsPortForTesting(HTTPSPort, useFakeHTTPS);
}

+ (void)setFallbackHttpPortForTesting:(int)HTTPPort {
  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetFallbackHttpPortForTesting(HTTPPort);

  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetCurrentIncognitoBrowserState())
      ->SetFallbackHttpPortForTesting(HTTPPort);
}

+ (void)setFallbackDelayForTesting:(int)fallbackDelayInMilliseconds {
  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->SetFallbackDelayForTesting(
          base::Milliseconds(fallbackDelayInMilliseconds));

  HttpsUpgradeServiceFactory::GetForBrowserState(
      chrome_test_util::GetCurrentIncognitoBrowserState())
      ->SetFallbackDelayForTesting(
          base::Milliseconds(fallbackDelayInMilliseconds));
}

+ (BOOL)isHttpsOnlyModeTimerRunning {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  HttpsOnlyModeUpgradeTabHelper* helper =
      HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state);
  return helper && helper->IsTimerRunningForTesting();
}

+ (BOOL)isOmniboxUpgradeTimerRunning {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  TypedNavigationUpgradeTabHelper* helper =
      TypedNavigationUpgradeTabHelper::FromWebState(web_state);
  return helper && helper->IsTimerRunningForTesting();
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
