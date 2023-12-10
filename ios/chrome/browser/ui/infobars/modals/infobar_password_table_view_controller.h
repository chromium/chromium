// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_password_modal_consumer.h"

@protocol InfobarPasswordModalDelegate;

// InfobarPasswordTableViewController represents the content for the Passwords
// InfobarModal.
@interface InfobarPasswordTableViewController
    : LegacyChromeTableViewController <InfobarPasswordModalConsumer>

- (instancetype)initWithDelegate:(id<InfobarPasswordModalDelegate>)modalDelegate
                            type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_PASSWORD_TABLE_VIEW_CONTROLLER_H_
