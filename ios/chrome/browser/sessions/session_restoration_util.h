// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_UTIL_H_

class Browser;

// Schedules the state of `browser` to be persisted to storage after a delay.
void ScheduleSaveSessionForBrowser(Browser* browser);

// Requests the state of `browser` to be persisted to storage immediately.
void SaveSessionForBrowser(Browser* browser);

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_UTIL_H_
