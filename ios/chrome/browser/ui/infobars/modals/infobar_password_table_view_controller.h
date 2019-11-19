// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#import "ios/chrome/browser/infobars/infobar_type.h"

@protocol InfobarPasswordModalDelegate;

// InfobarPasswordTableViewController represents the content for the Passwords
// InfobarModal.
@interface InfobarPasswordTableViewController : ChromeTableViewController

- (instancetype)initWithDelegate:(id<InfobarPasswordModalDelegate>)modalDelegate
                            type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// The username being displayed in the InfobarModal.
@property(nonatomic, copy) NSString* username;
// The masked password being displayed in the InfobarModal.
@property(nonatomic, copy) NSString* maskedPassword;
// The unmasked password for the InfobarModal.
@property(nonatomic, copy) NSString* unmaskedPassword;
// The details text message being displayed in the InfobarModal.
@property(nonatomic, copy) NSString* detailsTextMessage;
// The URL being displayed in the InfobarModal.
@property(nonatomic, copy) NSString* URL;
// The text used for the save/update credentials button.
@property(nonatomic, copy) NSString* saveButtonText;
// The text used for the cancel button.
@property(nonatomic, copy) NSString* cancelButtonText;
// YES if the current set of credentials has already been saved.
@property(nonatomic, assign) BOOL currentCredentialsSaved;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_
