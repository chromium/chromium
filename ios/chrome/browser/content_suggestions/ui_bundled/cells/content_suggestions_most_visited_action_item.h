// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_ITEM_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/content_suggestions_tile_constants.h"

// Item containing a most visited action button.
@interface ContentSuggestionsMostVisitedActionItem : NSObject

// Text for the title of the tile view.
@property(nonatomic, strong, nonnull) NSString* title;

// Image for the icon in the tile view.
@property(nonatomic, strong, nonnull) UIImage* icon;

// The accessibility label of the tile view.  If none is provided, self.title is
// used as the label.
@property(nonatomic, strong, nullable) NSString* accessibilityLabel;

// Reading list count passed to the most visited cell.
@property(nonatomic, assign) NSInteger count;

// Indicate if this suggestion is (temporary) disabled.
@property(nonatomic, assign) BOOL disabled;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ACTION_ITEM_H_
