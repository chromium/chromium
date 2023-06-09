// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_CENTRAL_ACCOUNT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_CENTRAL_ACCOUNT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Item for the signed-in account, used in account settings page.
@interface TableViewCentralAccountItem : TableViewItem

// Rounded avatarImage used for the account user picture. The value cannot be
// nil. Note: the image doesn't need to be rounded as the cell configs create
// the image rounded corners.
@property(nonatomic, strong) UIImage* avatarImage;
// Cell title displayed in main label. The value can be nil.
// In case the value is nil, the main label will show the email and there will
// be no secondary label.
@property(nonatomic, copy) NSString* name;
// Cell subtitle displayed in secondary label. The value cannot be nil.
@property(nonatomic, copy) NSString* email;

@end

// Cell for central account details with a leading centered avatar
// avatarImageView, followed by the account name label, and an email label.
@interface TableViewCentralAccountCell : TableViewCell

@property(nonatomic, strong, readonly) UIImageView* avatarImageView;
@property(nonatomic, strong, readonly) UILabel* nameLabel;
@property(nonatomic, strong, readonly) UILabel* emailLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_CENTRAL_ACCOUNT_ITEM_H_
