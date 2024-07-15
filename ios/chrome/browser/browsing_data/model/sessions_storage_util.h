// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SESSIONS_STORAGE_UTIL_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SESSIONS_STORAGE_UTIL_H_

#include <set>
#include <string>

namespace sessions_storage_util {

// Mark the sessions with `session_ids` for their files to be removed from the
// disk at some point later.
void MarkSessionsForRemoval(std::set<std::string> session_ids);

// Get the list of session ids for the sessions that was marked for removal.
std::set<std::string> GetDiscardedSessions();

// Empties the list of sessions that are marked for removal.
void ResetDiscardedSessions();

}  // namespace sessions_storage_util

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SESSIONS_STORAGE_UTIL_H_
