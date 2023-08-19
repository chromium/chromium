// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"

#import "base/scoped_observation.h"
#import "components/content_settings/core/browser/content_settings_observer.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"

@interface ContentSettingBackedBoolean ()

// The ID of the setting in `settingsMap`.
@property(nonatomic, readonly) ContentSettingsType settingID;

// Whether the boolean value reflects the state of the preference that backs it,
// or its negation.
@property(nonatomic, assign, getter=isInverted) BOOL inverted;

// Whether this object is the one modifying the content setting. Used to filter
// out changes notifications.
@property(nonatomic, assign) BOOL isModifyingContentSetting;

@end

namespace {

typedef base::ScopedObservation<HostContentSettingsMap,
                                content_settings::Observer>
    ContentSettingsObservation;

class ContentSettingsObserverBridge : public content_settings::Observer {
 public:
  explicit ContentSettingsObserverBridge(ContentSettingBackedBoolean* setting);
  ~ContentSettingsObserverBridge() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

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
    ContentSettingsType content_type) {
  // Ignore when it's the ContentSettingBackedBoolean that is changing the
  // content setting.
  if (setting_.isModifyingContentSetting) {
    return;
  }
  // Unfortunately, because the ContentSettingsPolicyProvider doesn't publish
  // the specific content setting on policy updates, we must refresh on every
  // ContentSettingsType::DEFAULT notification.
  if (content_type != ContentSettingsType::DEFAULT &&
      content_type != setting_.settingID) {
    return;
  }
  // Notify the BooleanObserver.
  [setting_.observer booleanDidChange:setting_];
}

}  // namespace

@implementation ContentSettingBackedBoolean {
  ContentSettingsType _settingID;
  scoped_refptr<HostContentSettingsMap> _settingsMap;
  std::unique_ptr<ContentSettingsObserverBridge> _adaptor;
  std::unique_ptr<ContentSettingsObservation> _content_settings_observer;
}

@synthesize settingID = _settingID;
@synthesize observer = _observer;
@synthesize inverted = _inverted;
@synthesize isModifyingContentSetting = _isModifyingContentSetting;

- (instancetype)initWithHostContentSettingsMap:
                    (HostContentSettingsMap*)settingsMap
                                     settingID:(ContentSettingsType)settingID
                                      inverted:(BOOL)inverted {
  self = [super init];
  if (self) {
    DCHECK(settingsMap);
    _settingID = settingID;
    _settingsMap = settingsMap;
    _inverted = inverted;
    // Listen for changes to the content setting.
    _adaptor.reset(new ContentSettingsObserverBridge(self));
    _content_settings_observer.reset(
        new ContentSettingsObservation(_adaptor.get()));
    _content_settings_observer->Observe(_settingsMap.get());
  }
  return self;
}

- (BOOL)value {
  DCHECK(_settingsMap) << "-value must not be called after -stop";
  ContentSetting setting =
      _settingsMap->GetDefaultContentSetting(_settingID, NULL);
  return self.inverted ^ (setting == CONTENT_SETTING_ALLOW);
}

- (void)setValue:(BOOL)value {
  DCHECK(_settingsMap) << "-setValue: must not be called after -stop";
  ContentSetting setting =
      (self.inverted ^ value) ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  self.isModifyingContentSetting = YES;
  _settingsMap->SetDefaultContentSetting(_settingID, setting);
  self.isModifyingContentSetting = NO;
}

- (void)stop {
  _content_settings_observer.reset();
  _adaptor.reset();
  _settingsMap = nullptr;
}

@end
