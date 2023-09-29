// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_mediator.h"

#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller_delegate.h"

@interface TabPickupSettingsMediator () <BooleanObserver,
                                         SyncObserverModelBridge>

@end

@implementation TabPickupSettingsMediator {
  // Preference service from the application context.
  PrefService* _prefs;
  // Sync service.
  syncer::SyncService* _syncService;
  // Observer for changes to the sync state.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Preference value for the tab pickup feature.
  PrefBackedBoolean* _tabPickupEnabledPref;
  // The consumer that will be notified when the data change.
  __weak id<TabPickupSettingsConsumer> _consumer;
}

- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                 syncService:(syncer::SyncService*)syncService
                                    consumer:(id<TabPickupSettingsConsumer>)
                                                 consumer {
  self = [super init];
  if (self) {
    CHECK(localPrefService);
    CHECK(syncService);
    CHECK(consumer);
    CHECK(IsTabPickupEnabled());
    _prefs = localPrefService;
    _syncService = syncService;
    _consumer = consumer;
    _syncObserverBridge =
        std::make_unique<SyncObserverBridge>(self, _syncService);

    _tabPickupEnabledPref = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:prefs::kTabPickupEnabled];
    _tabPickupEnabledPref.observer = self;

    [_consumer setTabPickupEnabled:_tabPickupEnabledPref.value];
    const bool tabSyncEnabled =
        _syncService->GetUserSettings()->GetSelectedTypes().Has(
            syncer::UserSelectableType::kTabs);
    [_consumer setTabSyncEnabled:tabSyncEnabled];
  }
  return self;
}

- (void)disconnect {
  _prefs = nil;
  _consumer = nil;
  _syncObserverBridge.reset();
}

#pragma mark - TabPickupSettingsTableViewControllerDelegate

// Sends the `enabled` state of the tab pickup feature to the model.
- (void)tabPickupSettingsTableViewController:
            (TabPickupSettingsTableViewController*)
                tabPickupSettingsTableViewController
                          didEnableTabPickup:(bool)enabled {
  _prefs->SetBoolean(prefs::kTabPickupEnabled, enabled);
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  const bool tabSyncEnabled =
      _syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs);
  [_consumer setTabSyncEnabled:tabSyncEnabled];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _tabPickupEnabledPref) {
    [_consumer setTabPickupEnabled:_tabPickupEnabledPref.value];
  }
}

@end
