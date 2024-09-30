// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/https_upgrade_app_interface.h"

#import "base/time/time.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/https_upgrades/model/https_only_mode_upgrade_tab_helper.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"
#import "ios/chrome/browser/https_upgrades/model/typed_navigation_upgrade_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

@implementation HttpsUpgradeAppInterface

+ (void)setHTTPSPortForTesting:(int)HTTPSPort useFakeHTTPS:(bool)useFakeHTTPS {
  HttpsUpgradeServiceFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile())
      ->SetHttpsPortForTesting(HTTPSPort, useFakeHTTPS);

  HttpsUpgradeServiceFactory::GetForProfile(
      chrome_test_util::GetCurrentIncognitoProfile())
      ->SetHttpsPortForTesting(HTTPSPort, useFakeHTTPS);
}

+ (void)setFallbackHttpPortForTesting:(int)HTTPPort {
  HttpsUpgradeServiceFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile())
      ->SetFallbackHttpPortForTesting(HTTPPort);

  HttpsUpgradeServiceFactory::GetForProfile(
      chrome_test_util::GetCurrentIncognitoProfile())
      ->SetFallbackHttpPortForTesting(HTTPPort);
}

+ (void)setFallbackDelayForTesting:(int)fallbackDelayInMilliseconds {
  HttpsUpgradeServiceFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile())
      ->SetFallbackDelayForTesting(
          base::Milliseconds(fallbackDelayInMilliseconds));

  HttpsUpgradeServiceFactory::GetForProfile(
      chrome_test_util::GetCurrentIncognitoProfile())
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
  ios::HostContentSettingsMapFactory::GetForProfile(
      chrome_test_util::GetOriginalProfile())
      ->ClearSettingsForOneType(ContentSettingsType::HTTP_ALLOWED);
  // Clear the temporary allowlist for incognito.
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  HttpsOnlyModeUpgradeTabHelper::FromWebState(web_state)
      ->ClearAllowlistForTesting();
}

@end
