// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_

#import "base/ios/block_types.h"

enum class ContentSuggestionsModuleType;
@class ContentSuggestionsMostVisitedActionItem;
@class ContentSuggestionsMostVisitedItem;
@class ContentSuggestionsReturnToRecentTabItem;
@class ContentSuggestionsWhatsNewItem;
enum class SetUpListItemType;
@class SetUpListItemViewData;
@class QuerySuggestionConfig;

// Supports adding/removing/updating UI elements to the ContentSuggestions
// UIViewController.
@protocol ContentSuggestionsConsumer

// Indicates to the consumer to present the Return to Recent Tab tile with
// `config`.
- (void)showReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config;

// Indicates to the consumer to update the Return to Recent Tab tile with
// `config`.
- (void)updateReturnToRecentTabTileWithConfig:
    (ContentSuggestionsReturnToRecentTabItem*)config;

// Indicates to the consumer to hide the Return to Recent Tab tile.
- (void)hideReturnToRecentTabTile;

// Indicates to the consumer the current Most Visited tiles to show with
// `configs`.
- (void)setMostVisitedTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedItem*>*)configs;

// Indicates to the consumer the current Shortcuts tiles to show with `configs`.
- (void)setShortcutTilesWithConfigs:
    (NSArray<ContentSuggestionsMostVisitedActionItem*>*)configs;

// Indicates to the consumer that the given `config` has updated data.
- (void)updateShortcutTileConfig:
    (ContentSuggestionsMostVisitedActionItem*)config;

// Indicates to the consumer update the Most Visited tile associated with
// `config`.
- (void)updateMostVisitedTileConfig:(ContentSuggestionsMostVisitedItem*)config;

// Indicates to the consumer to set the Magic Stack module order as listed in
// `order`.
- (void)setMagicStackOrder:(NSArray<NSNumber*>*)order;

// Indicates to the consumer to scroll to the next module because `moduleType`
// is completed.
- (void)scrollToNextMagicStackModuleForCompletedModule:
    (ContentSuggestionsModuleType)moduleType;

// Indicates to the consumer to display the SetUpList - a list of
// tasks that a new user may want to complete.
- (void)showSetUpListWithItems:(NSArray<SetUpListItemViewData*>*)items;

// Marks a Set Up List item complete with an animation and updated appearance.
// Calls the `completion` block when the animation is finished.
- (void)markSetUpListItemComplete:(SetUpListItemType)type
                       completion:(ProceduralBlock)completion;

// Hides the Set Up List, if it is currently visible. The given `animations`
// block will be called as part of the animation that hides the Set Up list, to
// allow other things to be animated at the same time.
- (void)hideSetUpListWithAnimations:(ProceduralBlock)animations;

// Shows the "All Set" screen which indicates to the user that all items are
// complete. Calls `animations` to allow other things to be simultaneously
// animated.
- (void)showSetUpListDoneWithAnimations:(ProceduralBlock)animations;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSUMER_H_
