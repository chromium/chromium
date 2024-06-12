// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_IDENTITY_ITEM_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_IDENTITY_ITEM_CONFIGURATOR_H_

#import <UIKit/UIKit.h>

@class TableViewIdentityItem;

// This class configures TableViewIdentityItem instances.
@interface AccountPickerSelectionScreenIdentityItemConfigurator : NSObject

@property(nonatomic, strong) NSString* gaiaID;
@property(nonatomic, strong) NSString* name;
@property(nonatomic, strong) NSString* email;
@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) BOOL selected;

- (void)configureIdentityChooser:(TableViewIdentityItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_IDENTITY_ITEM_CONFIGURATOR_H_
