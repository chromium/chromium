// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_UTIL_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_UTIL_H_

#include "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"

class UrlLoadingBrowserAgent;
enum class UrlLoadStrategy;

// Opens all tabs in the given set of tabs in the background.
void OpenDistantTabsInBackground(const synced_sessions::DistantTabVector& tabs,
                                 bool in_incognito,
                                 UrlLoadingBrowserAgent* url_loader,
                                 UrlLoadStrategy load_strategy);

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_SYNCED_SESSIONS_UTIL_H_
