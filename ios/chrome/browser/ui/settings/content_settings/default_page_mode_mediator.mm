// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_mediator.h"

#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_consumer.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"

@interface DefaultPageModeMediator () <BooleanObserver>

@property(nonatomic, strong) ContentSettingBackedBoolean* requestDesktopSetting;

@property(nonatomic, assign) feature_engagement::Tracker* tracker;

@end

@implementation DefaultPageModeMediator

- (instancetype)initWithSettingsMap:(HostContentSettingsMap*)settingsMap
                            tracker:(feature_engagement::Tracker*)tracker {
  self = [super init];
  if (self) {
    _requestDesktopSetting = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:ContentSettingsType::REQUEST_DESKTOP_SITE
                              inverted:NO];
    _tracker = tracker;
    [_requestDesktopSetting setObserver:self];
  }
  return self;
}

- (void)setConsumer:(id<DefaultPageModeConsumer>)consumer {
  if (_consumer == consumer)
    return;

  _consumer = consumer;
  [self updateConsumer];
}

#pragma mark - DefaultPageModeTableViewControllerDelegate

- (void)didSelectMode:(DefaultPageMode)selectedMode {
  BOOL newValue = selectedMode == DefaultPageModeDesktop;
  if (self.requestDesktopSetting.value == newValue)
    return;

  self.tracker->NotifyEvent(feature_engagement::events::kDefaultSiteViewUsed);

  self.requestDesktopSetting.value = newValue;
  [self updateConsumer];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, self.requestDesktopSetting);
  [self updateConsumer];
}

#pragma mark - Private

- (DefaultPageMode)defaultMode {
  return self.requestDesktopSetting.value ? DefaultPageModeDesktop
                                          : DefaultPageModeMobile;
}

- (void)updateConsumer {
  [self.consumer setDefaultPageMode:[self defaultMode]];
}

@end
