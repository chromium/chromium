// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_FAVICON_CONSUMER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_FAVICON_CONSUMER_H_

#import <UIKit/UIKit.h>

// Protocol for consumers of favicons from the Shop Card module.
@protocol ShopCardFaviconConsumer

// Called when a favicon is completed.
- (void)faviconCompleted:(UIImage*)faviconImage;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_UI_SHOP_CARD_FAVICON_CONSUMER_H_
