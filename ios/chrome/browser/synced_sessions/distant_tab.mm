// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_sessions/distant_tab.h"

#import "components/sync_sessions/open_tabs_ui_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
