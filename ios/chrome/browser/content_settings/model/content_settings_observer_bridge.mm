// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_settings/model/content_settings_observer_bridge.h"

#import "base/check.h"

ContentSettingsObserverBridge::ContentSettingsObserverBridge(
    id<ContentSettingsObserving> observer,
    HostContentSettingsMap* settings_map)
    : observer_(observer), settings_map_(settings_map) {
  CHECK(observer_);
  CHECK(settings_map_);
  scoped_observation_.Observe(settings_map_);
}

ContentSettingsObserverBridge::~ContentSettingsObserverBridge() = default;

void ContentSettingsObserverBridge::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  [observer_ contentSettingsMap:settings_map_
              didChangeForTypes:content_type_set
                 primaryPattern:primary_pattern
               secondaryPattern:secondary_pattern];
}
