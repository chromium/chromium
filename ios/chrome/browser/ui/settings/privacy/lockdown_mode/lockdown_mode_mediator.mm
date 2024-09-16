// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {}  // namespace

@interface LockdownModeMediator () <BooleanObserver>

// Preference value for the Lockdown Mode feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* lockdownModePreference;

@end

@implementation LockdownModeMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    _lockdownModePreference = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kBrowserLockdownModeEnabled];
    _lockdownModePreference.observer = self;
  }
  return self;
}

- (void)setConsumer:(id<LockdownModeConsumer>)consumer {
  _consumer = consumer;
  BOOL enabled = GetApplicationContext()->GetLocalState()->GetBoolean(
      prefs::kOSLockdownModeEnabled);
  [_consumer setOSLockdownModeEnabled:enabled];
  [_consumer setBrowserLockdownModeEnabled:self.lockdownModePreference.value];
}

- (void)disconnect {
  _lockdownModePreference = nil;
}

#pragma mark - LockdownModeViewControllerDelegate

- (void)didEnableBrowserLockdownMode:(BOOL)enabled {
  self.lockdownModePreference.value = enabled;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self.consumer
      setBrowserLockdownModeEnabled:self.lockdownModePreference.value];
}

@end
