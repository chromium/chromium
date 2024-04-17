// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_UTILS_H_

class Browser;
class TabGroup;
@class TabStripItemIdentifier;

namespace web {
class WebState;
}

class WebStateList;

// Returns the `TabStripItemIdentifier` for `web_state`.
TabStripItemIdentifier* CreateTabItemIdentifier(web::WebState* web_state);

// Returns the `TabStripItemIdentifier` for `group`.
TabStripItemIdentifier* CreateGroupItemIdentifier(const TabGroup* group,
                                                  WebStateList* web_state_list);

// Moves group to `destination_browser`, before the tab strip item with
// identifier `next_item_identifier`. If `next_item_identifier` is nil, the
// group will be moved to the end of the WebStateList.
void MoveGroupBeforeTabStripItem(const TabGroup* group,
                                 TabStripItemIdentifier* next_item_identifier,
                                 Browser* destination_browser);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_TAB_STRIP_MEDIATOR_UTILS_H_
