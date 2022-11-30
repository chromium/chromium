// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/infobars/modals/infobar_reading_list_modal_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol InfobarReadingListModalDelegate;

// InfobarReadingListTableViewController represents the content for the Add to
// Reading List InfobarModal.
@interface InfobarReadingListTableViewController
    : ChromeTableViewController <InfobarReadingListModalConsumer>

- (instancetype)initWithDelegate:
    (id<InfobarReadingListModalDelegate>)modalDelegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_READING_LIST_TABLE_VIEW_CONTROLLER_H_
