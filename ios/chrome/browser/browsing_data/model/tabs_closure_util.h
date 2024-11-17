// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_TABS_CLOSURE_UTIL_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_TABS_CLOSURE_UTIL_H_

#import "base/time/time.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state_id.h"

class WebStateList;

namespace tabs_closure_util {

// Map WebStateID to timestamp.
using WebStateIDToTime = std::map<web::WebStateID, base::Time>;

// Extracts the timestamp of the last committed item from `storage`.
base::Time GetLastCommittedTimestampFromStorage(
    web::proto::WebStateStorage storage);

// Returns the tabs in `tabs_to_last_navigation_time` and their timestamp that
// have a last navigation time between `begin_time` and `end_time`. Also
// includes the information of pinned WebStates.
WebStateIDToTime GetTabsInfoForCache(
    const WebStateIDToTime& tabs_to_last_navigation_time,
    base::Time begin_time,
    base::Time end_time);

// Returns the WebStates in `web_state_list` that are between `begin_time` and
// `end_time`. For unrealized webstates, uses the information in
// `cached_tabs_to_close`. Excludes pinned WebStates.
std::set<web::WebStateID> GetTabsToClose(
    WebStateList* web_state_list,
    base::Time begin_time,
    base::Time end_time,
    const WebStateIDToTime& cached_tabs_to_close);

// Returns the TabGroups in `web_state_list` with tabs between begin_time and
// end_time. It also returns the indexes of the tabs in each group that are
// between the begin and end time. For unrealized webstates, uses the
// information in `cached_tabs_to_close`.
std::map<tab_groups::TabGroupId, std::set<int>> GetTabGroupsWithTabsToClose(
    WebStateList* web_state_list,
    base::Time begin_time,
    base::Time end_time,
    const WebStateIDToTime& cached_tabs_to_close);

// Closes all the WebStates in `web_state_list` that are between `begin_time`
// and `end_time`. For unrealized webstates, uses the information in
// `cached_tabs_to_close`. Excludes pinned WebStates.
void CloseTabs(WebStateList* web_state_list,
               base::Time begin_time,
               base::Time end_time,
               const WebStateIDToTime& cached_tabs_to_close,
               bool keep_active_tab);

}  // namespace tabs_closure_util

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_TABS_CLOSURE_UTIL_H_
