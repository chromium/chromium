// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_consumer.h"

@class TableViewItem;

// Consumer protocol for Safe Browsing Privacy setting.
@protocol PrivacySafeBrowsingConsumer <ChromeTableViewConsumer>

// Reloads sections. Does nothing if the model is not loaded yet.
- (void)reloadSection;

// Initializes item array for |safeBrowsingItems|.
- (void)setSafeBrowsingItems:(NSArray<TableViewItem*>*)safeBrowsingItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_SAFE_BROWSING_CONSUMER_H_
