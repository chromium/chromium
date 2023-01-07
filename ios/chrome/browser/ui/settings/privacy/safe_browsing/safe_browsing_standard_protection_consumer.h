// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_CONSUMER_H_

#import <UIKit/UIKit.h>

@class SafeBrowsingHeaderItem;
@class TableViewItem;

// Consumer protocol for Safe Browsing Standard Protection view.
@protocol SafeBrowsingStandardProtectionConsumer

// Reload cells for items. Does nothing if the model is not loaded yet.
- (void)reloadCellsForItems;

// Initializes item array for `safeBrowsingStandardProtectionItems`.
- (void)setSafeBrowsingStandardProtectionItems:
    (NSArray<TableViewItem*>*)safeBrowsingStandardProtectionItems;

// Initializes section header related to the shield icon.
- (void)setShieldIconHeader:(SafeBrowsingHeaderItem*)shieldIconHeader;

// Initializes section header related to the metric icon.
- (void)setMetricIconHeader:(SafeBrowsingHeaderItem*)metricIconHeader;

@end
#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_CONSUMER_H_
