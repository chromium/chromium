// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_CELL_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_CELL_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

@class FaviconView;
enum class SnippetState;

// Callback when the snippet is hidden/show.
typedef void (^ChevronToggledBlock)(SnippetState snippet_state);

// SnippetSearchEngineCell is used by SnippetSearchEngineItem to display search
// engines in a UITableView.
@interface SnippetSearchEngineCell : TableViewCell

// Favicon image to display the search engine icon.
@property(nonatomic, strong) UIImage* faviconImage;
// The search engine name.
@property(nonatomic, readonly, strong) UILabel* nameLabel;
// The search engine snippet for the description.
@property(nonatomic, readonly, strong) UILabel* snippetLabel;
// Snippet state (hidden or closed).
@property(nonatomic, assign) SnippetState snippetState;
// YES if the search engine has been chosen by the user.
@property(nonatomic, assign) BOOL checked;
// Called when the chevron is tapped.
@property(nonatomic, copy) ChevronToggledBlock chevronToggledBlock;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_CELL_H_
