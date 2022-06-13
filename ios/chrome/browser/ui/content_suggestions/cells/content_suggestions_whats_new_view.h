// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_WHATS_NEW_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_WHATS_NEW_VIEW_H_

#import <UIKit/UIKit.h>

@class ContentSuggestionsWhatsNewItem;

// View for Notification Promo.
@interface ContentSuggestionsWhatsNewView : UIView

// Initializes and configures the view with `config`.
// TODO(crbug.com/1285378): Make this designated initializer after feature
// launch.
- (instancetype)initWithConfiguration:(ContentSuggestionsWhatsNewItem*)config;

// Image displaying the favicon.
@property(nonatomic, strong) UIImageView* iconView;

// Label containing the text of the promo.
@property(nonatomic, strong) UILabel* promoLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_WHATS_NEW_VIEW_H_
