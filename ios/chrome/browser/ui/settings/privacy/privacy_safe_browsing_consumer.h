// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"

@class TableViewItem;

// Consumer protocol for Safe Browsing Privacy setting.
@protocol PrivacySafeBrowsingConsumer <LegacyChromeTableViewConsumer>

// Reconfigre cells for items. Does nothing if the model is not loaded yet.
- (void)reconfigureCellsForItems;

// Initializes item array for `safeBrowsingItems`.
- (void)setSafeBrowsingItems:(NSArray<TableViewItem*>*)safeBrowsingItems;

// Tells consumer if enterprise is enabled based on pref values in model.
- (void)setEnterpriseEnabled:(BOOL)enterpriseEnabled;

// Select `item`.
- (void)selectItem:(TableViewItem*)item;

// Shows enterprise pop up when info button is pressed in enterprise mode.
- (void)showEnterprisePopUp:(UIButton*)buttonView;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_CONSUMER_H_
