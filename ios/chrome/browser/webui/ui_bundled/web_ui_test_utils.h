// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_WEB_UI_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_WEB_UI_TEST_UTILS_H_

#include <string>

class GURL;

// Returns the url to the web ui page `host`. `url::SchemeHostPort` can not be
// used when this test is run using EarlGrey2 because the chrome scheme is not
// registered in the test process and `url::SchemeHostPort` will not build an
// invalid URL.
GURL WebUIPageUrlWithHost(const std::string& host);

// Waits for omnibox text to equal (if `exact_match`) or contain (else) `URL`
// and returns true if it was found or false on timeout. Strips trailing URL
// slash if present as the omnibox does not display them.
bool WaitForOmniboxURLString(const std::string& url, bool exact_match = true);

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_WEB_UI_TEST_UTILS_H_
