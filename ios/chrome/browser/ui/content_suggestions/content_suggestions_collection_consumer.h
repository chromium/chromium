// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_CONSUMER_H_

#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"

@class CollectionViewItem;
@class ContentSuggestionsSectionInformation;
@protocol SuggestedContent;

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;

@protocol ContentSuggestionsCollectionConsumer

// Informs the consumer to reload with `sections` and `items`.
- (void)reloadDataWithSections:
            (NSArray<ContentSuggestionsSectionInformation*>*)sections
                      andItems:(NSMutableDictionary<NSNumber*, NSArray*>*)items;

// Informs the consumer to add `sectionInfo` to the model and call `completion`
// if a section is added. If the section already exists, `completion` will not
// be called.
- (void)addSection:(ContentSuggestionsSectionInformation*)sectionInfo
         withItems:(NSArray<CSCollectionViewItem*>*)items
        completion:(void (^)(void))completion;

// The section corresponding to `sectionInfo` has been invalidated and must be
// cleared now.
- (void)clearSection:(ContentSuggestionsSectionInformation*)sectionInfo;

// Notifies the consumer that the `item` has changed.
- (void)itemHasChanged:(CollectionViewItem<SuggestedContent>*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_CONSUMER_H_
