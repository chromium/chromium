// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_EARL_GREY_JS_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_EARL_GREY_JS_TEST_UTIL_H_

namespace web {

class WebState;

// Waits until the Window ID has been injected and the page is thus ready to
// respond to JavaScript injection. Returns false on timeout or if an
// unrecoverable error (such as no web view) occurs.
[[nodiscard]] bool WaitUntilWindowIdInjected(WebState* web_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_EARL_GREY_JS_TEST_UTIL_H_
