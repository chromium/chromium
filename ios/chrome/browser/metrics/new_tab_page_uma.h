// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_NEW_TAB_PAGE_UMA_H_
#define IOS_CHROME_BROWSER_METRICS_NEW_TAB_PAGE_UMA_H_

#include "ui/base/page_transition_types.h"

class ChromeBrowserState;
class GURL;
namespace web {
class WebState;
}

// This namespace provides various helpers around handling NTP Uma calls.
namespace new_tab_page_uma {

enum ActionType {
  ACTION_SEARCHED_USING_OMNIBOX,
  ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE,
  ACTION_NAVIGATED_USING_OMNIBOX,
  ACTION_OPENED_MOST_VISITED_ENTRY,
  ACTION_OPENED_RECENTLY_CLOSED_ENTRY,
  ACTION_OPENED_BOOKMARK,
  ACTION_OPENED_FOREIGN_SESSION,
  ACTION_OPENED_DOODLE,
  ACTION_OPENED_READING_LIST_ENTRY,
  ACTION_OPENED_SUGGESTION,
  ACTION_OPENED_LEARN_MORE,
  ACTION_OPENED_PROMO,
  ACTION_OPENED_HISTORY_ENTRY,
  ACTION_NAVIGATED_USING_VOICE_SEARCH,
  NUM_ACTION_TYPES,
};

void RecordAction(ChromeBrowserState* browser_state,
                  web::WebState* web_state,
                  ActionType action);

void RecordActionFromOmnibox(ChromeBrowserState* browser_state,
                             web::WebState* web_state,
                             const GURL& url,
                             ui::PageTransition transition,
                             bool is_expecting_voice_search);

}  // namespace new_tab_page_uma

#endif  // IOS_CHROME_BROWSER_METRICS_NEW_TAB_PAGE_UMA_H_
