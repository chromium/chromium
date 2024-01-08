// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LEGACY_SETTINGS_SEARCH_ENGINE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LEGACY_SETTINGS_SEARCH_ENGINE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/settings/cells/settings_search_engine_item.h"

class GURL;

// LegacySettingsSearchEngineItem contains the model data for a
// TableViewURLCell. This class is deprecated for SettingsSearchEngineItem.
@interface LegacySettingsSearchEngineItem
    : TableViewItem <SettingsSearchEngineItem>

// Identifier to match a URLItem with its URLCell.
@property(nonatomic, readonly, copy) NSString* uniqueIdentifier;
// The URL to fetch the favicon. This can be the favicon's URL, or a "fake" web
// page URL created by filling empty query word into the search engine's
// searchable URL template(e.g. "http://www.google.com/?q=").
@property(nonatomic, assign) GURL URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LEGACY_SETTINGS_SEARCH_ENGINE_ITEM_H_
