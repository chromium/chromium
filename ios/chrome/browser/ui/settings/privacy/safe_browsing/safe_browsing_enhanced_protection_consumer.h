// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_consumer.h"

@class TableViewItem;

// Consumer protocol for Safe Browsing Enhanced Protection view.
@protocol SafeBrowsingEnhancedProtectionConsumer

// Initializes item array for `safeBrowsingEnhancedProtectionItems`.
- (void)setSafeBrowsingEnhancedProtectionItems:
    (NSArray<TableViewItem*>*)safeBrowsingEnhancedProtectionItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_CONSUMER_H_
