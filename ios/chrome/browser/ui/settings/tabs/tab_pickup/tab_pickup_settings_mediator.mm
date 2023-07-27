// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabPickupSettingsMediator () <BooleanObserver>
@end

@implementation TabPickupSettingsMediator {
  // Preference service from the application context.
  PrefService* _prefs;
  // Preference value for the tab pickup feature.
  PrefBackedBoolean* _tabPickupEnabledPref;
  // The consumer that will be notified when the data change.
  __weak id<TabPickupSettingsConsumer> _consumer;
}

- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                    consumer:(id<TabPickupSettingsConsumer>)
                                                 consumer {
  self = [super init];
  if (self) {
    CHECK(localPrefService);
    CHECK(consumer);
    CHECK(IsTabPickupEnabled());
    _prefs = localPrefService;
    _consumer = consumer;

    _tabPickupEnabledPref = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:prefs::kTabPickupEnabled];
    _tabPickupEnabledPref.observer = self;

    [_consumer tabPickupStateChanged:_tabPickupEnabledPref.value];
  }
  return self;
}

- (void)disconnect {
  _prefs = nil;
  _consumer = nil;
}

#pragma mark - TabPickupSettingsTableViewControllerDelegate

// Sends the `enabled` state of the tab pickup feature to the model.
- (void)tabPickupSettingsTableViewController:
            (TabPickupSettingsTableViewController*)
                tabPickupSettingsTableViewController
                          didEnableTabPickup:(bool)enabled {
  _prefs->SetBoolean(prefs::kTabPickupEnabled, enabled);
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _tabPickupEnabledPref) {
    [_consumer tabPickupStateChanged:_tabPickupEnabledPref.value];
  }
}

@end
