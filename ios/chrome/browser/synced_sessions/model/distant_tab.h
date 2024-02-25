// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_DISTANT_TAB_H_
#define IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_DISTANT_TAB_H_

#import <optional>
#import <string>
#import <vector>

#import "base/time/time.h"
#import "components/sessions/core/session_id.h"
#import "url/gurl.h"

namespace synced_sessions {

// Data holder that contains the data of the distant tabs to show in the UI.
struct DistantTab {
  DistantTab();

  DistantTab(const DistantTab&) = delete;
  DistantTab& operator=(const DistantTab&) = delete;

  // Uniquely identifies the distant session this DistantTab belongs to.
  std::string session_tag;
  // Uniquely identifies this tab in its distant session.
  SessionID tab_id;
  // The title of the page shown in this DistantTab.
  std::u16string title;
  // The url shown in this DistantTab.
  GURL virtual_url;
  // Timestamp for when this tab was last activated.
  base::Time last_active_time;
  // Timestamp for when this tab was modified.
  base::Time modified_time;
  // Returns a hash the fields `virtual_url` and `title`.
  // By design, two tabs in the same distant session can have the same
  // `hashOfUserVisibleProperties`.
  size_t hashOfUserVisibleProperties();
};

// Data holder that contains a set of distant tabs to show in the UI.
struct DistantTabsSet {
  DistantTabsSet();
  ~DistantTabsSet();

  DistantTabsSet(const DistantTabsSet&);

  // The tag of the DistantSession which owns the tabs referenced in `tabs`.
  std::string session_tag;
  // A selection of `DistantTab`s from the session with tag `session_tag`. A
  // null value for `filtered_tabs` represents that the session's tabs are
  // not filtered. This shortcut representation prevents having to copy over
  // pointers to each tab within a session when every tab is included.
  std::optional<std::vector<DistantTab*>> filtered_tabs;
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_DISTANT_TAB_H_
