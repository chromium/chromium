// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_RANKING_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_RANKING_MODEL_DELEGATE_H_

@class MagicStackRankingModel;
@class MagicStackModule;

// Delegate for notifying changes to the ranking.
@protocol MagicStackRankingModelDelegate

// Indicates that the latest `rank` has been received.
- (void)magicStackRankingModel:(MagicStackRankingModel*)model
      didGetLatestRankingOrder:(NSArray<MagicStackModule*>*)rank;

// Indicates that `item` should be inserted at `index`.
- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                 didInsertItem:(MagicStackModule*)item
                       atIndex:(NSUInteger)index;

// Indicates that `item` should replace `oldItem`.
- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                didReplaceItem:(MagicStackModule*)oldItem
                      withItem:(MagicStackModule*)item;

// Indicates that `item` should be removed.
- (void)magicStackRankingModel:(MagicStackRankingModel*)model
                 didRemoveItem:(MagicStackModule*)item;

// Indicates that `item` should be reconfigured.
- (void)magicStackRankingModel:(MagicStackRankingModel*)model
            didReconfigureItem:(MagicStackModule*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_RANKING_MODEL_DELEGATE_H_
