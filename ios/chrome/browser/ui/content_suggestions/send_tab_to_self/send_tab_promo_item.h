// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_ITEM_H_

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

// Item containing the configurations for the Send Tab Promo Module view.
@interface SendTabPromoItem : MagicStackModule

// The favicon image of the tab, if any.
@property(nonatomic, strong) UIImage* faviconImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_SEND_TAB_PROMO_ITEM_H_
