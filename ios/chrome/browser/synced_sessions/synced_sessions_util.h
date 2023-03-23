// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SESSIONS_SYNCED_SESSIONS_UTIL_H_
#define IOS_CHROME_BROWSER_SYNCED_SESSIONS_SYNCED_SESSIONS_UTIL_H_

class UrlLoadingBrowserAgent;
enum class UrlLoadStrategy;

namespace synced_sessions {
struct DistantSession;
struct DistantTab;
}  // namespace synced_sessions

// Opens a tab in the background.
void OpenDistantTabInBackground(const synced_sessions::DistantTab* tab,
                                bool in_incognito,
                                UrlLoadingBrowserAgent* url_loader,
                                UrlLoadStrategy load_strategy);

// Opens all tabs in the given session in the background.
void OpenDistantSessionInBackground(
    const synced_sessions::DistantSession* session,
    bool in_incognito,
    UrlLoadingBrowserAgent* url_loader,
    UrlLoadStrategy load_strategy);

#endif  // IOS_CHROME_BROWSER_SYNCED_SESSIONS_SYNCED_SESSIONS_UTIL_H_
