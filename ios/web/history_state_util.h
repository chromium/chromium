// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_HISTORY_STATE_UTIL_H_
#define IOS_WEB_HISTORY_STATE_UTIL_H_

#include <string>

class GURL;

namespace web {
namespace history_state_util {

// Checks if toUrl is a valid argument to history.pushState() or
// history.replaceState() given the current URL.
bool IsHistoryStateChangeValid(const GURL& currentUrl,
                               const GURL& toUrl);

// Generates the appropriate full URL for a history.pushState() or
// history.replaceState() transition from currentURL to destination, resolved
// against baseURL. `destination` may be a relative URL. Will return an invalid
// URL if the resolved destination, or the transition, is not valid.
GURL GetHistoryStateChangeUrl(const GURL& currentUrl,
                              const GURL& baseUrl,
                              const std::string& destination);

}  // namespace history_state_util
}  // namespace web

#endif  // IOS_WEB_HISTORY_STATE_UTIL_H_
