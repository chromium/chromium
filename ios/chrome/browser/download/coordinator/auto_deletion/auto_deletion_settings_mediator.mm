// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_settings_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"
#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_settings_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

@interface AutoDeletionSettingsMediator () <BooleanObserver>
@end

@implementation AutoDeletionSettingsMediator {
  // The ApplicationContext::LocalState PrefService.
  raw_ptr<PrefService> _localState;
  // PrefBackedBoolean that tracks whether the user has Download Auto-deletion
  // enabled.
  PrefBackedBoolean* _autoDeletionEnabled;
}

- (instancetype)initWithLocalState:(PrefService*)localState {
  self = [super init];
  if (self) {
    _localState = localState;
    _autoDeletionEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_localState
                   prefName:prefs::kDownloadAutoDeletionEnabled];
    [_autoDeletionEnabled setObserver:self];
  }
  return self;
}

- (void)setAutoDeletionConsumer:(id<AutoDeletionSettingsConsumer>)consumer {
  _autoDeletionConsumer = consumer;
  BOOL status = _localState->GetBoolean(prefs::kDownloadAutoDeletionEnabled);
  [consumer setAutoDeletionEnabled:status];
}

#pragma mark - Public

- (void)disconnect {
  _localState = nullptr;
  [_autoDeletionEnabled stop];
  [_autoDeletionEnabled setObserver:nil];
  _autoDeletionEnabled = nullptr;
}

#pragma mark - AutoDeletionSettingsMutator

- (void)setDownloadAutoDeletionPermissionStatus:(BOOL)status {
  _localState->SetBoolean(prefs::kDownloadAutoDeletionEnabled, status);

  // Untracks the files scheduled for deletion when the user disables the
  // feature.
  if (!status) {
    GetApplicationContext()->GetAutoDeletionService()->Clear();
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _autoDeletionEnabled) {
    // Update the Download Auto-deletion feature switch ui.
    PrefService* localState = GetApplicationContext()->GetLocalState();
    BOOL isAutoDeletionEnabled =
        localState->GetBoolean(prefs::kDownloadAutoDeletionEnabled);
    [self.autoDeletionConsumer setAutoDeletionEnabled:isAutoDeletionEnabled];
  }
}

@end
