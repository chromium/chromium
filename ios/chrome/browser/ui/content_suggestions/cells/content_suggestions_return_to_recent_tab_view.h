// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_RETURN_TO_RECENT_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_RETURN_TO_RECENT_TAB_VIEW_H_

#import <UIKit/UIKit.h>

@class ContentSuggestionsReturnToRecentTabItem;

// View for the Return To Recent Tab tile.
@interface ContentSuggestionsReturnToRecentTabView : UIView

// Initializes and configures the view with `config`.
- (instancetype)initWithConfiguration:
    (ContentSuggestionsReturnToRecentTabItem*)config;

// Favicon image.
@property(nonatomic, strong) UIImageView* iconImageView;

// Title of the most recent tab tile.
@property(nonatomic, strong, readonly) UILabel* titleLabel;

// Subtitle of the most recent tab tile.
@property(nonatomic, strong, readonly) UILabel* subtitleLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_RETURN_TO_RECENT_TAB_VIEW_H_
