// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/app/settings_test_util.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

void SetContentSettingsBlockPopups(ContentSetting setting) {
  ios::ChromeBrowserState* browser_state = GetOriginalBrowserState();
  HostContentSettingsMap* settings_map =
      ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state);
  settings_map->SetDefaultContentSetting(ContentSettingsType::POPUPS, setting);
}

}  // namespace chrome_test_util
