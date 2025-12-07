// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_navigation_commands.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"

@interface TabsSettingsMediator () <BooleanObserver, PrefObserverDelegate>
@end

@implementation TabsSettingsMediator {
  // Preference service from the profile.
  raw_ptr<PrefService> _prefs;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Pref tracking if automatically open tab groups from other devices.
  PrefBackedBoolean* _automaticallyOpenTabGroupsEnabled;
  // The consumer that will be notified when the data change.
  __weak id<TabsSettingsConsumer> _consumer;
}

- (instancetype)initWithProfilePrefService:(PrefService*)profilePrefService
                                  consumer:(id<TabsSettingsConsumer>)consumer {
  self = [super init];
  if (self) {
    CHECK(profilePrefService);
    CHECK(consumer);
    _prefs = profilePrefService;
    _consumer = consumer;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kInactiveTabsTimeThreshold, &_prefChangeRegistrar);

    // Use InactiveTabsTimeThreshold() instead of reading the pref value
    // directly as this function also manage flag and default value.
    int currentThreshold = IsInactiveTabsExplicitlyDisabledByUser(_prefs)
                               ? kInactiveTabsDisabledByUser
                               : InactiveTabsTimeThreshold(_prefs).InDays();
    [_consumer setInactiveTabsTimeThreshold:currentThreshold];

    if (IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled()) {
      // Observe changes to the automatically open tab groups preference.
      _automaticallyOpenTabGroupsEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_prefs
                     prefName:prefs::kAutomaticallyOpenTabGroupsEnabled];
      [_automaticallyOpenTabGroupsEnabled setObserver:self];
      BOOL openTabGroups = _automaticallyOpenTabGroupsEnabled.value;
      [_consumer setAutomaticallyOpenTabGroupsEnabled:openTabGroups];
    }
  }
  return self;
}

- (void)disconnect {
  // Stop observable prefs.
  [_automaticallyOpenTabGroupsEnabled stop];
  _automaticallyOpenTabGroupsEnabled.observer = nil;
  _automaticallyOpenTabGroupsEnabled = nil;

  // Remove pref changes registrations.
  _prefChangeRegistrar.RemoveAll();

  // Remove observer bridge.
  _prefObserverBridge.reset();

  // Clear C++ ivars.
  _prefs = nil;
  _consumer = nil;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled() &&
      observableBoolean == _automaticallyOpenTabGroupsEnabled) {
    [_consumer setAutomaticallyOpenTabGroupsEnabled:observableBoolean.value];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
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

- (void)tabsSettingsTableViewController:
            (TabsSettingsTableViewController*)tabsSettingsTableViewController
             didUpdateAutoOpenTabGroups:(BOOL)autoOpenTabGroups {
  CHECK(IsAutoOpenRemoteTabGroupsSettingsFeatureEnabled());
  _automaticallyOpenTabGroupsEnabled.value = autoOpenTabGroups;
}

@end
