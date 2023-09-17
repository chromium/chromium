// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/settings_test_util.h"

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace chrome_test_util {

void SetContentSettingsBlockPopups(ContentSetting setting) {
  ChromeBrowserState* browser_state = GetOriginalBrowserState();
  HostContentSettingsMap* settings_map =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state);
  settings_map->SetDefaultContentSetting(ContentSettingsType::POPUPS, setting);
}

}  // namespace chrome_test_util
