// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns true if the `recent_navigation_time` is between [`begin_time`,
// `end_time`[. False, otherwise.
bool ShouldCloseTab(base::Time recent_navigation_time,
                    base::Time begin_time,
                    base::Time end_time) {
  CHECK_LE(begin_time, end_time);
  return recent_navigation_time >= begin_time &&
         recent_navigation_time < end_time;
}

// Returns the last navigation timestamp of `web_state`. For an unrealized
// WebState, uses the information in `cached_tabs_to_close`.
base::Time GetWebStateLastNavigationTime(
    web::WebState* web_state,
    const tabs_closure_util::WebStateIDToTime& cached_tabs_to_close) {
  web::WebStateID const web_state_id = web_state->GetUniqueIdentifier();

  if (web_state->IsRealized()) {
    // While the tabs' information was loaded from disk and this method was
    // invoked (i.e. when tabs' closure was requested), a new tab could have
    // been created or a new navigation could have happened. In either case,
    // the tabs are realized, and as such, we can get the last navigation time
    // directly from the WebState, avoiding using stale data.
    web::NavigationItem* navigation_item =
        web_state->GetNavigationManager()->GetLastCommittedItem();
    return navigation_item ? navigation_item->GetTimestamp()
                           : web_state->GetLastActiveTime();
  }

  if (auto iter = cached_tabs_to_close.find(web_state_id);
      iter != cached_tabs_to_close.end()) {
    // If the tab is unrealized, avoid realizing it, and use the cached
    // information which is still accurate.
    return iter->second;
  }

  // BYOT and other unrealized WebStates that have no navigation can be
  // created after loading the session from disk. Because they have no
  // navigation, or they would be realized, we will use last_active_time
  // (which is set to creation_time) as a fallback.
  return web_state->GetLastActiveTime();
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

WebStateIDToTime GetTabsInfoForCache(
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

std::set<web::WebStateID> GetTabsToClose(
    WebStateList* web_state_list,
    base::Time begin_time,
    base::Time end_time,
    const WebStateIDToTime& cached_tabs_to_close) {
  CHECK(web_state_list);

  std::set<web::WebStateID> webstates_to_close;
  // Exclude web states that are pinned.
  for (int index = web_state_list->pinned_tabs_count();
       index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    base::Time last_navigation_time =
        GetWebStateLastNavigationTime(web_state, cached_tabs_to_close);

    if (ShouldCloseTab(last_navigation_time, begin_time, end_time)) {
      webstates_to_close.insert(web_state->GetUniqueIdentifier());
    }
  }
  return webstates_to_close;
}

std::map<tab_groups::TabGroupId, std::set<int>> GetTabGroupsWithTabsToClose(
    WebStateList* web_state_list,
    base::Time begin_time,
    base::Time end_time,
    const WebStateIDToTime& cached_tabs_to_close) {
  CHECK(web_state_list);

  std::map<tab_groups::TabGroupId, std::set<int>> groups_with_tabs_to_close;
  for (const TabGroup* tab_group : web_state_list->GetGroups()) {
    std::set<int> webstate_indexes;
    for (int index : tab_group->range()) {
      base::Time last_navigation_time = GetWebStateLastNavigationTime(
          web_state_list->GetWebStateAt(index), cached_tabs_to_close);

      if (ShouldCloseTab(last_navigation_time, begin_time, end_time)) {
        webstate_indexes.insert(index);
      }
    }

    if (!webstate_indexes.empty()) {
      groups_with_tabs_to_close.insert(
          {tab_group->tab_group_id(), webstate_indexes});
    }
  }

  return groups_with_tabs_to_close;
}

void CloseTabs(WebStateList* web_state_list,
               base::Time begin_time,
               base::Time end_time,
               const WebStateIDToTime& cached_tabs_to_close,
               bool keep_active_tab) {
  CHECK(web_state_list);

  std::set<web::WebStateID> web_state_ids_to_close = GetTabsToClose(
      web_state_list, begin_time, end_time, cached_tabs_to_close);

  if (web_state_ids_to_close.empty()) {
    return;
  }

  std::vector<int> indices_to_close;
  // Exclude web states that are pinned.
  for (int index = web_state_list->pinned_tabs_count();
       index < web_state_list->count(); ++index) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    if (keep_active_tab && index == web_state_list->active_index()) {
      continue;
    }

    if (web_state_ids_to_close.contains(web_state->GetUniqueIdentifier())) {
      indices_to_close.push_back(index);
    }

    if (indices_to_close.size() == web_state_ids_to_close.size()) {
      break;
    }
  }

  auto lock = web_state_list->StartBatchOperation();
  web_state_list->CloseWebStatesAtIndices(
      WebStateList::CLOSE_TABS_CLEANUP,
      RemovingIndexes(std::move(indices_to_close)));
}
}  // namespace tabs_closure_util
