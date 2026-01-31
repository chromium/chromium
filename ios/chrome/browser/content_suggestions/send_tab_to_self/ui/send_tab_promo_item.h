// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_ITEM_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view_configuration.h"

@protocol SendTabPromoAudience;

// Item containing the configurations for the Send Tab Promo Module view.
@interface SendTabPromoItem : StandaloneModuleViewConfiguration

// The object that should handle user events.
@property(nonatomic, weak) id<SendTabPromoAudience> audience;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_ITEM_H_
