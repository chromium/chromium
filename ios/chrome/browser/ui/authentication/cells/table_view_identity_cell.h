// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_IDENTITY_CELL_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_IDENTITY_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"

@class IdentityView;

// Cell to display an user identity or the "Add Account…" button.
@interface TableViewIdentityCell : TableViewCell

// Initializes TableViewIdentityCell instance.
- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier
    NS_DESIGNATED_INITIALIZER;

// -[TableViewIdentityCell initWithStyle:reuseIdentifier:] should be used.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Configures the cell with the title, subtitle and image. If `checked` is YES,
// the cell displays a checkmark.
- (void)configureCellWithTitle:(NSString*)title
                      subtitle:(NSString*)subtitle
                         image:(UIImage*)image
                       checked:(BOOL)checked
             identityViewStyle:(IdentityViewStyle)identityViewStyle
                    titleColor:(UIColor*)titleColor;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CELLS_TABLE_VIEW_IDENTITY_CELL_H_
