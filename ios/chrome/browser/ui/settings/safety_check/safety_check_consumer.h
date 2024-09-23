// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"

// Consumer protocol for safety check.
@protocol SafetyCheckConsumer <LegacyChromeTableViewConsumer>

// Initializes the check types section with `items`.
- (void)setCheckItems:(NSArray<TableViewItem*>*)items;

// Initializes the safety check header with `item`.
- (void)setSafetyCheckHeaderItem:(TableViewLinkHeaderFooterItem*)item;

// Initializes the check start section with `item`.
- (void)setCheckStartItem:(TableViewItem*)item;

// Initializes the notification opt-in section with `item`.
- (void)setNotificationsOptInItem:(TableViewItem*)item;

// Initializes the footer with timestamp of last completed run.
- (void)setTimestampFooterItem:(TableViewLinkHeaderFooterItem*)item;

- (void)performBatchTableViewUpdates:(void (^)(void))updates
                          completion:(void (^)(BOOL finished))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_CONSUMER_H_
