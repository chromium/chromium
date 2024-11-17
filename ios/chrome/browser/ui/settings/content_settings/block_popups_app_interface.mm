// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/block_popups_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation BlockPopupsAppInterface

+ (void)setPopupPolicy:(ContentSetting)policy forPattern:(NSString*)pattern {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();

  ContentSettingsPattern exceptionPattern =
      ContentSettingsPattern::FromString(base::SysNSStringToUTF8(pattern));
  ios::HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingCustomScope(exceptionPattern,
                                     ContentSettingsPattern::Wildcard(),
                                     ContentSettingsType::POPUPS, policy);
}

@end
