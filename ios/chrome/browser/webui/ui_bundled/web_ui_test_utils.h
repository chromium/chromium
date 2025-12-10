// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_WEB_UI_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_WEB_UI_TEST_UTILS_H_

#include <string_view>

class GURL;

// Returns the url to the web ui page `host`. `url::SchemeHostPort` can not be
// used when this test is run using EarlGrey2 because the chrome scheme is not
// registered in the test process and `url::SchemeHostPort` will not build an
// invalid URL.
GURL WebUIPageUrlWithHost(std::string_view host);

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_WEB_UI_TEST_UTILS_H_
