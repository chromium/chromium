// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_favicon_data_source.h"

@protocol PasswordIssuesPresenter;

// Screen with a list of compromised credentials.
@interface PasswordIssuesTableViewController
    : SettingsRootTableViewController <PasswordIssuesConsumer>

@property(nonatomic, weak) id<PasswordIssuesPresenter> presenter;

// Data source for favicon images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_TABLE_VIEW_CONTROLLER_H_
