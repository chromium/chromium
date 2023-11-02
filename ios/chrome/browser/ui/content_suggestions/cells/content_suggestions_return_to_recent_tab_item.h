// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_RETURN_TO_RECENT_TAB_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_RETURN_TO_RECENT_TAB_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"

@protocol ContentSuggestionsGestureCommands;
@class FaviconAttributes;

// Item containing a Return to Recent Tab Start Surface tile.
@interface ContentSuggestionsReturnToRecentTabItem : NSObject

// Favicon image of the page of the most recent tab.
@property(nonatomic, strong) UIImage* icon;

// Title of the most recent tab tile.
@property(nonatomic, copy) NSString* title;

// Subtitle of the most recent tab tile.
@property(nonatomic, copy) NSString* subtitle;

// Command handler for the accessibility custom actions.
@property(nonatomic, weak) id<ContentSuggestionsGestureCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_RETURN_TO_RECENT_TAB_ITEM_H_
