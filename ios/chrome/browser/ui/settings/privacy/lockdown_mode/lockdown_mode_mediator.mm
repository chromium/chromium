// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_mediator.h"

#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {}  // namespace

@interface LockdownModeMediator () <BooleanObserver>

// User pref service.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Preference value for the Lockdown Mode feature.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* lockdownModePreference;

@end

@implementation LockdownModeMediator

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    _userPrefService = userPrefService;
    _lockdownModePreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kBrowserLockdownModeEnabled];
    _lockdownModePreference.observer = self;
  }
  return self;
}

- (void)setConsumer:(id<LockdownModeConsumer>)consumer {
  _consumer = consumer;
  [_consumer setOSLockdownModeEnabled:self.userPrefService->GetBoolean(
                                          prefs::kOSLockdownModeEnabled)];
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
