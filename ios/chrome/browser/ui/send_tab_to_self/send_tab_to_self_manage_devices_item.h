// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MANAGE_DEVICES_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MANAGE_DEVICES_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol SendTabToSelfModalDelegate;

// An item displaying a) a link to the page where the user can manage their
// available devices, as well as b) information about the account they are
// using to share (avatar and email).
@interface SendTabToSelfManageDevicesItem : TableViewItem

// Avatar of the account sharing a tab.
@property(nonatomic, strong) UIImage* accountAvatar;
// Email of the account sharing a tab.
@property(nonatomic, strong) NSString* accountEmail;
// Whether to display a link to the list of known devices for this account.
@property(nonatomic, assign) BOOL showManageDevicesLink;
// Delegate to open the link upon click.
@property(nonatomic, weak) id<SendTabToSelfModalDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MANAGE_DEVICES_ITEM_H_
