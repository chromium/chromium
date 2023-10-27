// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_PROVIDER_H_

namespace web {
class WebStateID;
}  // namespace web

// Protocol allowing to get the selected items from the grid.
@protocol GridItemProvider

// The IDs of the selected items.
- (std::set<web::WebStateID>)selectedItemIDsForEditing;
// The IDs of the selected items that can be shared.
- (std::set<web::WebStateID>)selectedShareableItemIDsForEditing;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_ITEM_PROVIDER_H_
