// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_table_view_controller_delegate.h"

@protocol WebInspectorStateConsumer;
class PrefService;

// Mediator for the screen allowing the user to enable Web Inspector support.
@interface WebInspectorStateMediator
    : NSObject <WebInspectorStateTableViewControllerDelegate>

// Initializes the mediator with `userPrefService` for accessing the user's
// Web Inspector preference.
- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnect the mediator.
- (void)disconnect;

@property(nonatomic, weak) id<WebInspectorStateConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_MEDIATOR_H_
