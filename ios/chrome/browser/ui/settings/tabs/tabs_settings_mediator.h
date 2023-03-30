// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_table_view_controller_delegate.h"

class PrefService;
@protocol TabsSettingsConsumer;
@protocol TabsSettingsNavigationCommands;

// Mediator for the tabs settings.
@interface TabsSettingsMediator
    : NSObject <TabsSettingsTableViewControllerDelegate>

// Handler used to navigate inside the tabs setting.
@property(nonatomic, weak) id<TabsSettingsNavigationCommands> handler;

// Designated initializer. All the parameters should not be null.
// `localPrefService`: preference service from the application context.
// `consumer`: consumer that will be notified when the data change.
- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                    consumer:(id<TabsSettingsConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops mediating and disconnects from backend models.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_MEDIATOR_H_
