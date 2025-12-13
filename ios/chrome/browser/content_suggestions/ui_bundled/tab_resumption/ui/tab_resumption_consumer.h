// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONSUMER_H_

#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_data.h"

@class TabResumptionItem;

// Protocol for consumers of data from the Tab Resumption module.
@protocol TabResumptionConsumer

// Update TabResumptionView when ShopCard data is acquired.
- (void)shopCardDataCompleted:(TabResumptionItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_TAB_RESUMPTION_UI_TAB_RESUMPTION_CONSUMER_H_
