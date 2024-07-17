// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_navigation_commands.h"

@interface TabsSettingsMediator () <PrefObserverDelegate>
@end

@implementation TabsSettingsMediator {
  // Preference service from the application context.
  raw_ptr<PrefService> _prefs;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // The consumer that will be notified when the data change.
  __weak id<TabsSettingsConsumer> _consumer;
}

- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                    consumer:
                                        (id<TabsSettingsConsumer>)consumer {
  self = [super init];
  if (self) {
    CHECK(localPrefService);
    CHECK(consumer);
    _prefs = localPrefService;
    _consumer = consumer;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
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
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
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
  }
}

#pragma mark - TabsSettingsTableViewControllerDelegate

- (void)tabsSettingsTableViewControllerDidSelectInactiveTabsSettings:
    (TabsSettingsTableViewController*)tabsSettingsTableViewController {
  base::RecordAction(base::UserMetricsAction("Settings.Tabs.InactiveTabs"));
  [self.handler showInactiveTabsSettings];
}

@end
