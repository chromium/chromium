// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_VIEW_H_

#import <UIKit/UIKit.h>

@protocol SendTabPromoAudience;
@class SendTabPromoItem;

// A view displaying the Send Tab To Self promo module in the Magic Stack.
@interface SendTabPromoView : UIView

// The object that should handle user events.
@property(nonatomic, weak) id<SendTabPromoAudience> audience;

// Default initializer.
- (instancetype)initWithConfig:(SendTabPromoItem*)config
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end
#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SEND_TAB_TO_SELF_UI_SEND_TAB_PROMO_VIEW_H_
