// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"

#import "components/sync_sessions/open_tabs_ui_delegate.h"

namespace synced_sessions {

DistantTab::DistantTab() : tab_id(SessionID::InvalidValue()) {}

size_t DistantTab::hashOfUserVisibleProperties() {
  std::stringstream ss;
  ss << title << std::endl << virtual_url.spec();
  return std::hash<std::string>()(ss.str());
}

DistantTabsSet::DistantTabsSet() = default;

DistantTabsSet::~DistantTabsSet() = default;

DistantTabsSet::DistantTabsSet(const DistantTabsSet&) = default;

}  // namespace synced_sessions
