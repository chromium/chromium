// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

class GURL;
enum class SnippetState;

// SearchEngineItem contains the model data for a SnippetSearchEngineCell.
@interface SnippetSearchEngineItem : TableViewItem

// The name of the search engine.
@property(nonatomic, copy) NSString* name;
// The text for the search engine snippet.
@property(nonatomic, copy) NSString* snippetDescription;
// The URL to fetch the favicon. This can be the favicon's URL, or a "fake" web
// page URL created by filling empty query word into the search engine's
// searchable URL template(e.g. "http://www.google.com/?q=").
@property(nonatomic, assign) GURL URL;
// Favicon attributes for the search engine.
@property(nonatomic, strong) UIImage* faviconImage;
// Set the snippet state (hidden or closed).
@property(nonatomic, assign) SnippetState snippetState;
// YES if the search engine has been chosen by the user.
@property(nonatomic, assign) BOOL checked;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_CELLS_SNIPPET_SEARCH_ENGINE_ITEM_H_
