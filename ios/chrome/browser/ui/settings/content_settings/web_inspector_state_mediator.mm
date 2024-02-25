// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_consumer.h"

@interface WebInspectorStateMediator () <BooleanObserver>

// The preference that stores whether Web Inspector is enabled.
@property(nonatomic, strong) PrefBackedBoolean* webInspectorPreference;

// The pref service used for accessing user preferences.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

@end

@implementation WebInspectorStateMediator

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    _userPrefService = userPrefService;
    _webInspectorPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kWebInspectorEnabled];
    _webInspectorPreference.observer = self;
  }
  return self;
}

- (void)setConsumer:(id<WebInspectorStateConsumer>)consumer {
  _consumer = consumer;
  [self updateConsumer];
}

- (void)disconnect {
  [self.webInspectorPreference stop];
}

#pragma mark - WebInspectorStateTableViewControllerDelegate

- (void)didEnableWebInspector:(BOOL)enabled {
  self.webInspectorPreference.value = enabled;
  [self updateConsumer];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK_EQ(observableBoolean, _webInspectorPreference);
  [self updateConsumer];
}

#pragma mark - Private

- (void)updateConsumer {
  [self.consumer setWebInspectorEnabled:_webInspectorPreference.value];
}

@end
