// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/content_settings/core/browser/content_settings_observer.h"
#import "components/content_settings/core/browser/content_settings_registry.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/content_settings/model/content_settings_observer_bridge.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_consumer.h"

@interface SiteSettingsMediator () <ContentSettingsObserving>
@end

@implementation SiteSettingsMediator {
  raw_ptr<HostContentSettingsMap> _settingsMap;
  std::unique_ptr<ContentSettingsObserverBridge> _settingsObserver;
}

- (instancetype)initWithHostContentSettingsMap:
    (HostContentSettingsMap*)settingsMap {
  self = [super init];
  if (self) {
    CHECK(settingsMap);
    _settingsMap = settingsMap;
    _settingsObserver =
        std::make_unique<ContentSettingsObserverBridge>(self, _settingsMap);
  }
  return self;
}

- (void)disconnect {
  _settingsObserver.reset();
  _settingsMap = nullptr;
  _consumer = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<SiteSettingsConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self loadSettings];
  }
}

#pragma mark - ContentSettingsObserving

- (void)contentSettingsMap:(HostContentSettingsMap*)settingsMap
         didChangeForTypes:(ContentSettingsTypeSet)contentTypeSet
            primaryPattern:(const ContentSettingsPattern&)primaryPattern
          secondaryPattern:(const ContentSettingsPattern&)secondaryPattern {
  if (contentTypeSet.Contains(ContentSettingsType::MEDIASTREAM_MIC) ||
      contentTypeSet.Contains(ContentSettingsType::MEDIASTREAM_CAMERA) ||
      contentTypeSet.Contains(ContentSettingsType::GEOLOCATION)) {
    [self loadSettings];
  }
}

#pragma mark - Private

- (void)loadSettings {
  BOOL locationSupported =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          ContentSettingsType::GEOLOCATION) != nullptr;
  [_consumer setLocationCategoryEnabled:locationSupported];

  ContentSetting micSetting = _settingsMap->GetDefaultContentSetting(
      ContentSettingsType::MEDIASTREAM_MIC, /*provider_id=*/nullptr);
  [_consumer setDefaultSetting:micSetting
                       forType:ContentSettingsType::MEDIASTREAM_MIC];

  ContentSetting cameraSetting = _settingsMap->GetDefaultContentSetting(
      ContentSettingsType::MEDIASTREAM_CAMERA, /*provider_id=*/nullptr);
  [_consumer setDefaultSetting:cameraSetting
                       forType:ContentSettingsType::MEDIASTREAM_CAMERA];

  if (locationSupported) {
    ContentSetting locationSetting = _settingsMap->GetDefaultContentSetting(
        ContentSettingsType::GEOLOCATION, /*provider_id=*/nullptr);
    [_consumer setDefaultSetting:locationSetting
                         forType:ContentSettingsType::GEOLOCATION];
  }
}

@end
