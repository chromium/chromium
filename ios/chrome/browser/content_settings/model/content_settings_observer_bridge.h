// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_CONTENT_SETTINGS_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_CONTENT_SETTINGS_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/content_settings/core/browser/content_settings_observer.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/content_settings_types.h"

// Protocol for Objective-C objects observing `HostContentSettingsMap` changes.
@protocol ContentSettingsObserving <NSObject>

// Notifies the observer that content settings have changed for the given
// types and patterns.
- (void)contentSettingsMap:(HostContentSettingsMap*)settingsMap
         didChangeForTypes:(ContentSettingsTypeSet)contentTypeSet
            primaryPattern:(const ContentSettingsPattern&)primaryPattern
          secondaryPattern:(const ContentSettingsPattern&)secondaryPattern;

@end

// C++ observer bridge that observes a `HostContentSettingsMap` and forwards
// notifications to an Objective-C observer.
class ContentSettingsObserverBridge : public content_settings::Observer {
 public:
  ContentSettingsObserverBridge(id<ContentSettingsObserving> observer,
                                HostContentSettingsMap* settings_map);

  ContentSettingsObserverBridge(const ContentSettingsObserverBridge&) = delete;
  ContentSettingsObserverBridge& operator=(
      const ContentSettingsObserverBridge&) = delete;

  ~ContentSettingsObserverBridge() override;

  // content_settings::Observer implementation:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

 private:
  __weak id<ContentSettingsObserving> observer_ = nil;
  raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_CONTENT_SETTINGS_MODEL_CONTENT_SETTINGS_OBSERVER_BRIDGE_H_
