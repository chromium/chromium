// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_UTIL_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_UTIL_H_

#include <memory>
#include <vector>

class Browser;
enum class UrlLoadStrategy;

namespace synced_sessions {
struct DistantTab;
}  // namespace synced_sessions

// Opens all tabs in the given set of tabs in the background.
void OpenDistantTabsInBackground(
    const std::vector<std::unique_ptr<synced_sessions::DistantTab>>& tabs,
    Browser* browser,
    UrlLoadStrategy load_strategy);

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_UTIL_H_
