// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_ACTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_ACTION_DELEGATE_H_

// Protocol for delegating actions to the owner of the
// ShopCardMediator
@protocol ShopCardActionDelegate

// Opens a URL in an existing Tab if a Tab with the URL exists.
// Otherwise opens the URL in a new Tab.
- (void)openURL:(GURL)url;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SHOP_CARD_COORDINATOR_SHOP_CARD_ACTION_DELEGATE_H_
