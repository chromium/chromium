// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_STATIC_HTML_VIEW_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_STATIC_HTML_VIEW_TEST_UTIL_H_

#include <string>

namespace web {
class WebState;
}

namespace chrome_test_util {

// Returns true if there is a static html view in the |web_state|, which
// contains |text|. If the text is not present, or there is no static html view
// it returns false.
bool StaticHtmlViewContainingText(web::WebState* web_state, std::string text);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_STATIC_HTML_VIEW_TEST_UTIL_H_
