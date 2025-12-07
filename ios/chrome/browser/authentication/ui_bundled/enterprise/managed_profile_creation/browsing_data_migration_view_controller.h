// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_BROWSING_DATA_MIGRATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_BROWSING_DATA_MIGRATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol BrowsingDataMigrationViewControllerMutator

- (void)updateShouldKeepBrowsingDataSeparate:(BOOL)browsingDataSeparate;

@end

// View controller of managed profile creation screen.
@interface BrowsingDataMigrationViewController : ChromeTableViewController

@property(nonatomic, weak) id<BrowsingDataMigrationViewControllerMutator>
    mutator;

// `browsingDataSeparate` is the default value initially shown to the user
// as the selected value.
- (instancetype)initWithUserEmail:(NSString*)userEmail
             browsingDataSeparate:(BOOL)browsingDataSeparate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_BROWSING_DATA_MIGRATION_VIEW_CONTROLLER_H_
