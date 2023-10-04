// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_UTIL_H_
#define IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_UTIL_H_

class UrlLoadingBrowserAgent;
enum class UrlLoadStrategy;

namespace synced_sessions {
struct DistantSession;
struct DistantTab;
}  // namespace synced_sessions

// The default number of tabs that are instantly loaded when a large batch of
// URLs are loaded simultaneously.
int GetDefaultNumberOfTabsToLoadSimultaneously();

// Opens a tab with `url_loader` using `load_strategy`.
void OpenDistantTab(const synced_sessions::DistantTab* tab,
                    bool in_incognito,
                    bool instant_load,
                    UrlLoadingBrowserAgent* url_loader,
                    UrlLoadStrategy load_strategy);

// Opens all tabs in the given session in the background. If
// `first_tab_load_strategy` is ALWAYS_NEW_FOREGROUND_TAB, it would be opened on
// the foreground right after this method is called. All other tabs would be
// opened in the background.
void OpenDistantSessionInBackground(
    const synced_sessions::DistantSession* session,
    bool in_incognito,
    int maximum_instant_load_tabs,
    UrlLoadingBrowserAgent* url_loader,
    UrlLoadStrategy first_tab_load_strategy);

#endif  // IOS_CHROME_BROWSER_SYNCED_SESSIONS_MODEL_SYNCED_SESSIONS_UTIL_H_
