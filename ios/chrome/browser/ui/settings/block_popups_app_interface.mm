// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/block_popups_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BlockPopupsAppInterface

+ (void)setPopupPolicy:(ContentSetting)policy forPattern:(NSString*)pattern {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  ContentSettingsPattern exceptionPattern =
      ContentSettingsPattern::FromString(base::SysNSStringToUTF8(pattern));
  ios::HostContentSettingsMapFactory::GetForBrowserState(browserState)
      ->SetContentSettingCustomScope(
          exceptionPattern, ContentSettingsPattern::Wildcard(),
          ContentSettingsType::POPUPS, std::string(), policy);
}

@end
