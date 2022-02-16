// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_PARENT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_PARENT_ITEM_H_

#import <MaterialComponents/MaterialCollectionCells.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_selection_actions.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"

@class ContentSuggestionsMostVisitedActionItem;
@class ContentSuggestionsMostVisitedItem;
@class ContentSuggestionsReturnToRecentTabItem;
@class ContentSuggestionsWhatsNewItem;

// Item containing all the Content Suggestions content.
@interface ContentSuggestionsParentItem : CollectionViewItem <SuggestedContent>

// The configuration for the Return To Recent Tab tile.
@property(nonatomic, strong)
    ContentSuggestionsReturnToRecentTabItem* returnToRecentItem;
// The configuration for the NTP promo view.
@property(nonatomic, strong) ContentSuggestionsWhatsNewItem* whatsNewItem;
// The list of configurations for the Most Visited Tiles to be shown.
@property(nonatomic, strong)
    NSArray<ContentSuggestionsMostVisitedItem*>* mostVisitedItems;
// The list of configurations for the Shortcuts to be shown.
@property(nonatomic, strong)
    NSArray<ContentSuggestionsMostVisitedActionItem*>* shortcutsItems;

// The target for the Most Visited tiles.
@property(nonatomic, weak) id<ContentSuggestionsSelectionActions> tapTarget;

// Provider of menu configurations for the Most Visited tiles.
@property(nonatomic, weak) id<ContentSuggestionsMenuProvider> menuProvider;

@end

// The cell associated with ContentSuggestionsParentItem.
@interface ContentSuggestionsParentCell : MDCCollectionViewCell

// Adds |view| as a subview. If |spacing| is non-zero, a bottom spacing of
// |spacing| will be added below |view|.
- (void)addUIElement:(UIView*)view withCustomBottomSpacing:(CGFloat)spacing;

// Removes all UI elements added by addUIElement:withCustomBottomSpacing:.
- (void)removeContentViews;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_PARENT_ITEM_H_
