// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

#import "base/ranges/algorithm.h"

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
}  // namespace tabs_closure_util
