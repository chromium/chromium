// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_mediator.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_navigation_commands.h"

@interface TabsSettingsMediator () <PrefObserverDelegate,
                                    SyncObserverModelBridge>
@end

@implementation TabsSettingsMediator {
  // Preference service from the application context.
  PrefService* _prefs;
  // Sync service.
  syncer::SyncService* _syncService;
  // Observer for changes to the sync state.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // The consumer that will be notified when the data change.
  __weak id<TabsSettingsConsumer> _consumer;
}

- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                 syncService:(syncer::SyncService*)syncService
                                    consumer:
                                        (id<TabsSettingsConsumer>)consumer {
  self = [super init];
  if (self) {
    CHECK(localPrefService);
    CHECK(syncService);
    CHECK(consumer);
    _prefs = localPrefService;
    _syncService = syncService;
    _consumer = consumer;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    _syncObserverBridge =
        std::make_unique<SyncObserverBridge>(self, _syncService);
    if (IsInactiveTabsAvailable()) {
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kInactiveTabsTimeThreshold, &_prefChangeRegistrar);

      // Use InactiveTabsTimeThreshold() instead of reading the pref value
      // directly as this function also manage flag and default value.
      int currentThreshold = IsInactiveTabsExplictlyDisabledByUser()
                                 ? kInactiveTabsDisabledByUser
                                 : InactiveTabsTimeThreshold().InDays();
      [_consumer setInactiveTabsTimeThreshold:currentThreshold];
    }

    if (IsTabPickupEnabled()) {
      _prefObserverBridge->ObserveChangesForPreference(prefs::kTabPickupEnabled,
                                                       &_prefChangeRegistrar);
      [_consumer setTabPickupEnabled:!IsTabPickupDisabledByUser() &&
                                     _syncService->IsSyncFeatureEnabled()];
    }
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _syncObserverBridge.reset();
  _prefs = nil;
  _consumer = nil;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    CHECK(IsInactiveTabsAvailable());
    [_consumer
        setInactiveTabsTimeThreshold:_prefs->GetInteger(
                                         prefs::kInactiveTabsTimeThreshold)];
  } else if (preferenceName == prefs::kTabPickupEnabled) {
    CHECK(IsTabPickupEnabled());
    [_consumer
        setTabPickupEnabled:_prefs->GetBoolean(prefs::kTabPickupEnabled) &&
                            _syncService->IsSyncFeatureEnabled()];
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [_consumer setTabPickupEnabled:_prefs->GetBoolean(prefs::kTabPickupEnabled) &&
                                 _syncService->IsSyncFeatureEnabled()];
}

#pragma mark - TabsSettingsTableViewControllerDelegate

- (void)tabsSettingsTableViewControllerDidSelectInactiveTabsSettings:
    (TabsSettingsTableViewController*)tabsSettingsTableViewController {
  base::RecordAction(base::UserMetricsAction("Settings.Tabs.InactiveTabs"));
  [self.handler showInactiveTabsSettings];
}

- (void)tabsSettingsTableViewControllerDidSelectTabPickupSettings:
    (TabsSettingsTableViewController*)tabsSettingsTableViewController {
  base::RecordAction(base::UserMetricsAction("Settings.Tabs.TabPickup"));
  [self.handler showTabPickupSettings];
}

@end
