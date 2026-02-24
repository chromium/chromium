// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_UI_QUICK_DELETE_OTHER_DATA_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_UI_QUICK_DELETE_OTHER_DATA_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol QuickDeleteOtherDataCommands;

// Provides the table view for "Quick Delete Other Data" page.
@interface QuickDeleteOtherDataViewController
    : ChromeTableViewController <QuickDeleteOtherDataConsumer>

// Action handler for the local QuickDeleteOtherDataCommands.
@property(nonatomic, weak) id<QuickDeleteOtherDataCommands>
    quickDeleteOtherDataHandler;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_UI_QUICK_DELETE_OTHER_DATA_VIEW_CONTROLLER_H_
