// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/permissions/permissions_consumer.h"

@protocol InfobarModalDelegate;
@protocol InfobarModalPresentationHandler;
@protocol PermissionsDelegate;

// InfobarPermissionsTableViewController represents the content for the
// Permissionss InfobarModal.
API_AVAILABLE(ios(15.0))
@interface InfobarPermissionsTableViewController
    : LegacyChromeTableViewController <PermissionsConsumer>

- (instancetype)initWithDelegate:
    (id<InfobarModalDelegate, PermissionsDelegate>)modalDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Handler used to resize the modal.
@property(nonatomic, weak) id<InfobarModalPresentationHandler>
    presentationHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PERMISSIONS_INFOBAR_PERMISSIONS_TABLE_VIEW_CONTROLLER_H_
