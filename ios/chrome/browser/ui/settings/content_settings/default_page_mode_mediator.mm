// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_mediator.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_consumer.h"
#import "ios/chrome/browser/ui/settings/utils/content_setting_backed_boolean.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultPageModeMediator () <BooleanObserver>

@property(nonatomic, strong) ContentSettingBackedBoolean* requestDesktopSetting;

@end

@implementation DefaultPageModeMediator

- (instancetype)initWithSettingsMap:(HostContentSettingsMap*)settingsMap {
  self = [super init];
  if (self) {
    _requestDesktopSetting = [[ContentSettingBackedBoolean alloc]
        initWithHostContentSettingsMap:settingsMap
                             settingID:ContentSettingsType::REQUEST_DESKTOP_SITE
                              inverted:NO];
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

- (void)setDefaultMode:(DefaultPageMode)defaultMode {
  BOOL newValue = defaultMode == DefaultPageModeDesktop;
  if (self.requestDesktopSetting.value == newValue)
    return;

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
