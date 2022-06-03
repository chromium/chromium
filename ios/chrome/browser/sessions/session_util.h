// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_UTIL_H_

#include <memory>
#include <vector>

#include "base/callback.h"

class ChromeBrowserState;

namespace sessions {
class SerializedNavigationEntry;
}

namespace web {
class WebState;
}

// Utility method that allows to access the iOS SessionService from C++ code.
namespace session_util {

// Create a WebState initialized with |browser_state| and serialized navigation.
// The returned WebState has web usage enabled.
std::unique_ptr<web::WebState> CreateWebStateWithNavigationEntries(
    ChromeBrowserState* browser_state,
    int last_committed_item_index,
    const std::vector<sessions::SerializedNavigationEntry>& navigations);

}  // namespace session_util

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_UTIL_H_
