// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"

#include "base/scoped_observer.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSettingBackedBoolean ()

// The ID of the setting in |settingsMap|.
@property(nonatomic, readonly) ContentSettingsType settingID;

// Whether the boolean value reflects the state of the preference that backs it,
// or its negation.
@property(nonatomic, assign, getter=isInverted) BOOL inverted;

// Whether this object is the one modifying the content setting. Used to filter
// out changes notifications.
@property(nonatomic, assign) BOOL isModifyingContentSetting;

@end

namespace {

typedef ScopedObserver<HostContentSettingsMap, content_settings::Observer>
    ContentSettingsObserver;

class ContentSettingsObserverBridge : public content_settings::Observer {
 public:
  explicit ContentSettingsObserverBridge(ContentSettingBackedBoolean* setting);
  ~ContentSettingsObserverBridge() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

 private:
  ContentSettingBackedBoolean* setting_;  // weak
};

ContentSettingsObserverBridge::ContentSettingsObserverBridge(
    ContentSettingBackedBoolean* setting)
    : setting_(setting) {}

ContentSettingsObserverBridge::~ContentSettingsObserverBridge() {}

void ContentSettingsObserverBridge::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  // Ignore when it's the ContentSettingBackedBoolean that is changing the
  // content setting.
  if (setting_.isModifyingContentSetting) {
    return;
  }
  const ContentSettingsDetails settings_details(
      primary_pattern, secondary_pattern, content_type, resource_identifier);
  ContentSettingsType settingID = settings_details.type();
  // Unfortunately, because the ContentSettingsPolicyProvider doesn't publish
  // the specific content setting on policy updates, we must refresh on every
  // ContentSettingsType::DEFAULT notification.
  if (settingID != ContentSettingsType::DEFAULT &&
      settingID != setting_.settingID) {
    return;
  }
  // Notify the BooleanObserver.
  [setting_.observer booleanDidChange:setting_];
}

}  // namespace

@implementation ContentSettingBackedBoolean {
  ContentSettingsType settingID_;
  scoped_refptr<HostContentSettingsMap> settingsMap_;
  std::unique_ptr<ContentSettingsObserverBridge> adaptor_;
  std::unique_ptr<ContentSettingsObserver> content_settings_observer_;
}

@synthesize settingID = settingID_;
@synthesize observer = observer_;
@synthesize inverted = inverted_;
@synthesize isModifyingContentSetting = isModifyingContentSetting_;

- (id)initWithHostContentSettingsMap:(HostContentSettingsMap*)settingsMap
                           settingID:(ContentSettingsType)settingID
                            inverted:(BOOL)inverted {
  self = [super init];
  if (self) {
    settingID_ = settingID;
    settingsMap_ = settingsMap;
    inverted_ = inverted;
    // Listen for changes to the content setting.
    adaptor_.reset(new ContentSettingsObserverBridge(self));
    content_settings_observer_.reset(
        new ContentSettingsObserver(adaptor_.get()));
    content_settings_observer_->Add(settingsMap);
  }
  return self;
}

- (BOOL)value {
  ContentSetting setting =
      settingsMap_->GetDefaultContentSetting(settingID_, NULL);
  return self.inverted ^ (setting == CONTENT_SETTING_ALLOW);
}

- (void)setValue:(BOOL)value {
  ContentSetting setting =
      (self.inverted ^ value) ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  self.isModifyingContentSetting = YES;
  settingsMap_->SetDefaultContentSetting(settingID_, setting);
  self.isModifyingContentSetting = NO;
}

@end
