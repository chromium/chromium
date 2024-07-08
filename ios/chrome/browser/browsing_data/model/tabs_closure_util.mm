// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {
bool ShouldCloseTab(base::Time recent_navigation_time,
                    base::Time begin_time,
                    base::Time end_time) {
  CHECK_LE(begin_time, end_time);
  return recent_navigation_time >= begin_time &&
         recent_navigation_time < end_time;
}
}  // namespace

namespace tabs_closure_util {
base::Time GetLastCommittedTimestampFromStorage(
    web::proto::WebStateStorage storage) {
  const auto& navigation_storage = storage.navigation();
  const int index = navigation_storage.last_committed_item_index();
  if (index < 0 || navigation_storage.items_size() <= index) {
    // Invalid last committed item index, return last active time as fallback.
    return web::TimeFromProto(storage.metadata().last_active_time());
  }

  const auto& item_storage = navigation_storage.items(index);
  return web::TimeFromProto(item_storage.timestamp());
}

WebStateIDToTime GetTabsToClose(
    const WebStateIDToTime& tabs_to_last_navigation_time,
    base::Time begin_time,
    base::Time end_time) {
  CHECK_LE(begin_time, end_time);
  WebStateIDToTime tabs_to_close;
  for (auto const& tab_to_time : tabs_to_last_navigation_time) {
    if (ShouldCloseTab(tab_to_time.second, begin_time, end_time)) {
      tabs_to_close.insert({tab_to_time.first, tab_to_time.second});
    }
  }
  return tabs_to_close;
}

void CloseTabs(WebStateList* web_state_list,
               base::Time begin_time,
               base::Time end_time,
               const WebStateIDToTime& cached_tabs_to_close) {
  CHECK(web_state_list);

  std::vector<int> indices_to_close;
  for (int index = 0; index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    web::WebStateID const web_state_id = web_state->GetUniqueIdentifier();

    base::Time last_navigation_time;
    if (web_state->IsRealized()) {
      // While the tabs' information was loaded from disk and this method was
      // invoked (i.e. when tabs' closure was requested), a new tab could have
      // been created or a new navigation could have happened. In either case,
      // the tabs are realized, and as such, we can get the last navigation time
      // directly from the WebState, avoiding using stale data.
      web::NavigationItem* navigation_item =
          web_state->GetNavigationManager()->GetLastCommittedItem();
      last_navigation_time = navigation_item ? navigation_item->GetTimestamp()
                                             : web_state->GetLastActiveTime();
    } else if (auto iter = cached_tabs_to_close.find(web_state_id);
               iter != cached_tabs_to_close.end()) {
      // If the tab is unrealized, avoid realizing it, and use the cached
      // information which is still accurate.
      last_navigation_time = iter->second;
    } else {
      // BYOT and other unrealized WebStates that have no navigation can be
      // created after loading the session from disk. Because they have no
      // navigation, or they would be realized, we will use last_active_time
      // (which is set to creation_time) as a fallback.
      last_navigation_time = web_state->GetLastActiveTime();
    }

    if (ShouldCloseTab(last_navigation_time, begin_time, end_time)) {
      indices_to_close.push_back(index);
    }
  }

  if (indices_to_close.empty()) {
    return;
  }

  auto lock = web_state_list->StartBatchOperation();
  web_state_list->CloseWebStatesAtIndices(
      WebStateList::CLOSE_NO_FLAGS,
      RemovingIndexes(std::move(indices_to_close)));
}
}  // namespace tabs_closure_util
