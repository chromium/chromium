// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_table_view_controller_delegate.h"

class PrefService;
@protocol TabsSettingsConsumer;
@protocol TabsSettingsNavigationCommands;

// Mediator for the tabs settings.
@interface TabsSettingsMediator
    : NSObject <TabsSettingsTableViewControllerDelegate>

// Handler used to navigate inside the tabs setting.
@property(nonatomic, weak) id<TabsSettingsNavigationCommands> handler;

// Designated initializer. All the parameters should not be null.
// `profilePrefService`: preference service from the profile.
// `consumer`: consumer that will be notified when the data change.
- (instancetype)initWithProfilePrefService:(PrefService*)profilePrefService
                                  consumer:(id<TabsSettingsConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops mediating and disconnects from backend models.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_MEDIATOR_H_
