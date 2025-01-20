// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/content_settings/default_page_mode_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@protocol DefaultPageModeTableViewControllerDelegate;

// ViewController for the screen allowing the user to choose the default mode
// (Desktop/Mobile) for loading pages.
@interface DefaultPageModeTableViewController
    : SettingsRootTableViewController <DefaultPageModeConsumer,
                                       SettingsControllerProtocol>

@property(nonatomic, weak) id<DefaultPageModeTableViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_DEFAULT_PAGE_MODE_TABLE_VIEW_CONTROLLER_H_
