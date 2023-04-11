// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller_delegate.h"

class Browser;
@protocol InactiveTabsSettingsConsumer;
class PrefService;

// Mediator for the inactive tabs settings.
@interface InactiveTabsSettingsMediator
    : NSObject <InactiveTabsSettingsTableViewControllerDelegate>

// Designated initializer. All the parameters should not be null.
// `localPrefService`: preference service from the application context.
// `consumer`: consumer that will be notified when the data change.
// `browser`: the current browser.
- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                     browser:(Browser*)browser
                                    consumer:(id<InactiveTabsSettingsConsumer>)
                                                 consumer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops mediating and disconnects from backend models.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_MEDIATOR_H_
