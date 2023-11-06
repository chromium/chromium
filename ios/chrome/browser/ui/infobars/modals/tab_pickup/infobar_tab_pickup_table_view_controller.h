// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/tab_pickup/infobar_tab_pickup_consumer.h"

@protocol InfobarModalDelegate;
@protocol InfobarTabPickupTableViewControllerDelegate;

// Controller for the UI that allows the user to update tab pickup modal.
@interface InfobarTabPickupTableViewController
    : LegacyChromeTableViewController <InfobarTabPickupConsumer>

// The delegate receives events related to this view controller.
@property(nonatomic, weak)
    id<InfobarModalDelegate, InfobarTabPickupTableViewControllerDelegate>
        delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_TABLE_VIEW_CONTROLLER_H_
