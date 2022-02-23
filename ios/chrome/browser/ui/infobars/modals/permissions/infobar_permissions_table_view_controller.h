// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/infobars/modals/permissions/infobar_permissions_modal_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol InfobarPermissionsModalDelegate;

// InfobarPermissionsTableViewController represents the content for the
// Permissionss InfobarModal.
API_AVAILABLE(ios(15.0))
@interface InfobarPermissionsTableViewController
    : ChromeTableViewController <InfobarPermissionsModalConsumer>

- (instancetype)initWithDelegate:
    (id<InfobarPermissionsModalDelegate>)modalDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_
