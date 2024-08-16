// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_IDENTITY_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_IDENTITY_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

// Item to exchange identities between the mediator and the view controller.
@interface IdentityViewItem : NSObject

@property(nonatomic, copy) NSString* userEmail;
@property(nonatomic, copy) NSString* gaiaID;
@property(nonatomic, copy) UIImage* avatar;
@property(nonatomic, copy) NSString* accessibilityIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_ACCOUNTS_IDENTITY_VIEW_ITEM_H_
