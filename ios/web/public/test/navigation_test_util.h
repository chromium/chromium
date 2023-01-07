// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_NAVIGATION_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_NAVIGATION_TEST_UTIL_H_

#include "url/gurl.h"

namespace web {

class WebState;

namespace test {

// Loads `url` in `web_state` with transition of type ui::PAGE_TRANSITION_TYPED.
void LoadUrl(WebState* web_state, const GURL& url);

// Returns true if the current page in the current WebState finishes loading
// within a timeout.
[[nodiscard]] bool WaitForPageToFinishLoading(WebState* web_state);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_NAVIGATION_TEST_UTIL_H_
