// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_modal_consumer.h"

@protocol InfobarTranslateModalDelegate;

// InfobarTranslateTableViewController represents the content for the Translate
// InfobarModal.
@interface InfobarTranslateTableViewController
    : ChromeTableViewController <InfobarTranslateModalConsumer>

- (instancetype)initWithDelegate:
    (id<InfobarTranslateModalDelegate>)modalDelegate NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_TABLE_VIEW_CONTROLLER_H_
