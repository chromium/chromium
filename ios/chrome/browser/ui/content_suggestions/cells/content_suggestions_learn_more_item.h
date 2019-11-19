// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_LEARN_MORE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_LEARN_MORE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"

// Item displaying the text showing a label inviting the user to ask for
// information about the suggested content.
@interface ContentSuggestionsLearnMoreItem
    : CollectionViewItem<SuggestedContent>

// Returns the text to be displayed by the cell.
- (nonnull NSString*)text;

@end

// Associated cell, displaying the text to know more about suggested content.
@interface ContentSuggestionsLearnMoreCell : MDCCollectionViewCell

// Returns the height needed by a cell contained in |width| to display |text|.
+ (CGFloat)heightForWidth:(CGFloat)width withText:(nonnull NSString*)text;

// Sets the |text| to be displayed by this cell.
- (void)setText:(nonnull NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_LEARN_MORE_ITEM_H_
