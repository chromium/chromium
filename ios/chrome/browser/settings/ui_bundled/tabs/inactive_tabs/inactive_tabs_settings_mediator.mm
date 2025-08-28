// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/tabs/inactive_tabs/inactive_tabs_settings_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/inactive_tabs/inactive_tabs_settings_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"

@interface InactiveTabsSettingsMediator () <PrefObserverDelegate>
@end

@implementation InactiveTabsSettingsMediator {
  // Preference service from the profile.
  raw_ptr<PrefService> _prefs;
  // The consumer that will be notified when the data change.
  __weak id<InactiveTabsSettingsConsumer> _consumer;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithProfilePrefService:(PrefService*)profilePrefService
                                  consumer:(id<InactiveTabsSettingsConsumer>)
                                               consumer {
  self = [super init];
  if (self) {
    DCHECK(profilePrefService);
    DCHECK(consumer);
    _prefs = profilePrefService;
    _consumer = consumer;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    // Register to observe any changes on pref backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kInactiveTabsTimeThreshold, &_prefChangeRegistrar);

    // Use InactiveTabsTimeThreshold() instead of reading the pref value
    // directly as this function also manages the flag and the default value.
    int currentThreshold = IsInactiveTabsExplicitlyDisabledByUser(_prefs)
                               ? kInactiveTabsDisabledByUser
                               : InactiveTabsTimeThreshold(_prefs).InDays();
    [_consumer updateCheckedStateWithDaysThreshold:currentThreshold];
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefs = nil;
  _consumer = nil;
}

#pragma mark - InactiveTabsSettingsTableViewControllerDelegate

- (void)inactiveTabsSettingsTableViewController:
            (InactiveTabsSettingsTableViewController*)
                inactiveTabsSettingsTableViewController
                 didSelectInactiveDaysThreshold:(int)threshold {
  int previousThreshold = _prefs->GetInteger(prefs::kInactiveTabsTimeThreshold);
  if (previousThreshold == threshold) {
    return;
  }
  // Update the pref. The InactiveTabsService will take care of updating all web
  // state lists.
  _prefs->SetInteger(prefs::kInactiveTabsTimeThreshold, threshold);
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    [_consumer updateCheckedStateWithDaysThreshold:
                   _prefs->GetInteger(prefs::kInactiveTabsTimeThreshold)];
  }
}

@end
