// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_APP_WEB_SHELL_TEST_UTIL_H_
#define IOS_WEB_SHELL_TEST_APP_WEB_SHELL_TEST_UTIL_H_

namespace web {
class WebState;

namespace shell_test_util {

// Gets the current WebState for the web shell.
web::WebState* GetCurrentWebState();

}  // namespace shell_test_util
}  // namespace web

#endif  // IOS_WEB_SHELL_TEST_APP_WEB_SHELL_TEST_UTIL_H_
