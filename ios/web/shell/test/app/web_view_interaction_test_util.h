// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_APP_WEB_VIEW_INTERACTION_TEST_UTIL_H_
#define IOS_WEB_SHELL_TEST_APP_WEB_VIEW_INTERACTION_TEST_UTIL_H_

#include <string>

namespace web {
namespace shell_test_util {

// Attempts to tap the element with `element_id` in the current WebState
// using a JavaScript click() event.
void TapWebViewElementWithId(const std::string& element_id);

}  // namespace shell_test_util
}  // namespace web

#endif  // IOS_WEB_SHELL_TEST_APP_WEB_VIEW_INTERACTION_TEST_UTIL_H_
