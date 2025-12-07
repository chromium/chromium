// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_IDENTITY_ITEM_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_IDENTITY_ITEM_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/views_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewIdentityItem holds the model data for an identity.
@interface TableViewIdentityItem : TableViewItem

// Gaia ID.
@property(nonatomic, assign) GaiaId gaiaID;
// User name.
@property(nonatomic, copy) NSString* name;
// User email.
@property(nonatomic, copy) NSString* email;
// User avatar.
@property(nonatomic, strong) UIImage* avatar;
// If YES, the identity is selected.
@property(nonatomic, assign) BOOL selected;
// If YES, the identity is managed.
@property(nonatomic, assign) BOOL managed;
// Style for the IdentityView.
@property(nonatomic, assign) IdentityViewStyle identityViewStyle;

// Same as `configureCell:withStyler:` with a completion block.
- (void)configureCell:(UITableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler
           completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_IDENTITY_ITEM_H_
