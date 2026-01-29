// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_PINNED_SITE_FORM_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_PINNED_SITE_FORM_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class MostVisitedItem;
@protocol MostVisitedTilesPinnedSiteMutator;

enum class PinnedSiteAction;

/// Modal for user to create or edit a pinned site in the most visited tiles.
@interface PinnedSiteFormViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

/// Mutator object that handles pinned site creation and edits.
@property(nonatomic, weak) id<MostVisitedTilesPinnedSiteMutator> mutator;

/// Initializer for `PinnedSiteFormViewController`. If `action` is `kModify`,
/// `item` should be provided.
- (instancetype)initWithAction:(PinnedSiteAction)action
                       forItem:(MostVisitedItem*)item NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MOST_VISITED_TILES_UI_PINNED_SITE_FORM_VIEW_CONTROLLER_H_
