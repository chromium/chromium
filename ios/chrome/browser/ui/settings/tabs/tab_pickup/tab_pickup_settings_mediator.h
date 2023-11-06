// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller_delegate.h"

class AuthenticationService;
class PrefService;
@protocol TabPickupSettingsConsumer;

namespace syncer {
class SyncService;
}  // namespace syncer

// Mediator for the tab pickup settings.
@interface TabPickupSettingsMediator
    : NSObject <TabPickupSettingsTableViewControllerDelegate>

// Designated initializer. All the parameters should not be null.
// `localPrefService`: preference service from the application context.
// `browserPrefService`: preference service from the browser state.
// `authenticationService` authentication service.
// `syncService` sync service.
// `consumer`: consumer that will be notified when the data change.
- (instancetype)
    initWithUserLocalPrefService:(PrefService*)localPrefService
              browserPrefService:(PrefService*)browserPrefService
           authenticationService:(AuthenticationService*)authenticationService
                     syncService:(syncer::SyncService*)syncService
                        consumer:(id<TabPickupSettingsConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops mediating and disconnects from backend models.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_MEDIATOR_H_
