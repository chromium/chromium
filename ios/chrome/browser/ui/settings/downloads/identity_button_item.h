// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_IDENTITY_BUTTON_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_IDENTITY_BUTTON_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"

typedef NS_ENUM(NSInteger, IdentityButtonControlArrowDirection);

// Table view item to present an IdentityButtonControl in a table view.
@interface IdentityButtonItem : TableViewItem

// IdentityButtonControl parameters.
@property(nonatomic, strong) UIImage* identityAvatar;
@property(nonatomic, copy) NSString* identityName;
@property(nonatomic, copy) NSString* identityEmail;
@property(nonatomic, copy) NSString* identityGaiaID;
@property(nonatomic, assign) IdentityButtonControlArrowDirection arrowDirection;
@property(nonatomic, assign) IdentityViewStyle identityViewStyle;

// Whether the button control is enabled. If set to NO, the button cannot be
// tapped and will appear greyed out.
@property(nonatomic, assign) BOOL enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_DOWNLOADS_IDENTITY_BUTTON_ITEM_H_
