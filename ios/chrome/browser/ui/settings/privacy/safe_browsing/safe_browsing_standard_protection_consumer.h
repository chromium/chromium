// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_CONSUMER_H_

#import <UIKit/UIKit.h>

@class TableViewItem;

// Consumer protocol for Safe Browsing Standard Protection view.
@protocol SafeBrowsingStandardProtectionConsumer

// Initializes item array for |safeBrowsingStandardProtectionItems|.
- (void)setSafeBrowsingStandardProtectionItems:
    (NSArray<TableViewItem*>*)safeBrowsingStandardProtectionItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_CONSUMER_H_
