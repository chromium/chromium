// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_UTIL_H_

#include "base/functional/callback_forward.h"

class Browser;
class ChromeBrowserState;

// Schedules the state of `browser` to be persisted to storage after a delay.
void ScheduleSaveSessionForBrowser(Browser* browser);

// Requests the state of `browser` to be persisted to storage immediately.
void SaveSessionForBrowser(Browser* browser);

// Executes `closure` when all the pending background task of the session
// restoration service bound to `browser_state` are done. The `closure` must
// be safe to be called on any sequence as it will be invoked on a background
// sequence.
void ExecuteClosureWhenSessionServiceBackgroundProcessingDone(
    ChromeBrowserState* browser_state,
    base::OnceClosure closure);

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_UTIL_H_
