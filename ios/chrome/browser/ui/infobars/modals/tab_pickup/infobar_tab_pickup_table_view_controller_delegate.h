// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class InfobarTabPickupTableViewController;

// Delegate for events related to tab pickup modal view controller.
@protocol InfobarTabPickupTableViewControllerDelegate <NSObject>

// Sends the `enabled` state of the tab pickup feature to the model.
- (void)infobarTabPickupTableViewController:
            (InfobarTabPickupTableViewController*)
                infobarTabPickupTableViewController
                         didEnableTabPickup:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_TAB_PICKUP_INFOBAR_TAB_PICKUP_TABLE_VIEW_CONTROLLER_DELEGATE_H_
