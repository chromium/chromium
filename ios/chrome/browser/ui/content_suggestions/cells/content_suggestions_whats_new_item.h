// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_WHATS_NEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_WHATS_NEW_ITEM_H_

#import <UIKit/UIKit.h>

// Item to display what is new in the ContentSuggestions.
@interface ContentSuggestionsWhatsNewItem : NSObject

// Icon for the promo.
@property(nonatomic, strong, nullable) UIImage* icon;
// Text describing what is new.
@property(nonatomic, copy, nullable) NSString* text;

+ (nonnull NSString*)accessibilityIdentifier;

@end
#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_WHATS_NEW_ITEM_H_
