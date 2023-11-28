// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_CELL_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_CELL_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

@class FaviconView;

// SnippetSearchEngineCell is used by SnippetSearchEngineItem to display search
// engines in a UITableView.
@interface SnippetSearchEngineCell : TableViewCell

// The imageview that is displayed on the leading edge of the cell.  This
// contains a favicon composited on top of an off-white background.
@property(nonatomic, readonly, strong) FaviconView* faviconView;

// The search engine name.
@property(nonatomic, readonly, strong) UILabel* nameLabel;

// The search engine snippet for the description.
@property(nonatomic, readonly, strong) UILabel* snippetLabel;

// Unique identifier that matches with one URLItem.
@property(nonatomic, strong) NSString* cellUniqueIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_CELL_H_
