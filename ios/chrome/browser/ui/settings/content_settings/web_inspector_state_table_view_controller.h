// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/content_settings/web_inspector_state_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol WebInspectorStateTableViewControllerDelegate;

// ViewController for the screen allowing the user to enable Web Inspector
// support.
@interface WebInspectorStateTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol,
                                       WebInspectorStateConsumer>

@property(nonatomic, weak) id<WebInspectorStateTableViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_WEB_INSPECTOR_STATE_TABLE_VIEW_CONTROLLER_H_
