// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ENGINE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ENGINE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

class GURL;

// SearchEngineItem contains the model data for a TableViewURLCell.
@interface SettingsSearchEngineItem : TableViewItem

// The enabled/disabled state. If disabled, user interaction will be forbidden
// and cell's alpha will be reduced.
@property(nonatomic, assign) BOOL enabled;
// The text for the title.
@property(nonatomic, readwrite, copy) NSString* text;
// The text for the subtitle.
@property(nonatomic, readwrite, copy) NSString* detailText;
// The URL to fetch the favicon. This can be the favicon's URL, or a "fake" web
// page URL created by filling empty query word into the search engine's
// searchable URL template(e.g. "http://www.google.com/?q=").
@property(nonatomic, assign) GURL URL;
// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly, copy) NSString* uniqueIdentifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_SETTINGS_SEARCH_ENGINE_ITEM_H_
