// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SUGGESTED_CONTENT_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SUGGESTED_CONTENT_H_

#import <UIKit/UIKit.h>

@class CollectionViewItem;
@class ContentSuggestionIdentifier;
@protocol SuggestedContent;

// Behavior shared by the items in ContentSuggestions.
@protocol SuggestedContent

// Identifier for this content.
@property(nonatomic, strong, nullable)
    ContentSuggestionIdentifier* suggestionIdentifier;
// Whether the metrics for this suggestion have been recorded.
@property(nonatomic, assign) BOOL metricsRecorded;

// The height needed by a cell configured by this item, for a `width`.
- (CGFloat)cellHeightForWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_SUGGESTED_CONTENT_H_
