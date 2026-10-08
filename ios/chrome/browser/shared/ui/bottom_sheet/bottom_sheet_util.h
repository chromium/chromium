// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_BOTTOM_SHEET_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_BOTTOM_SHEET_UTIL_H_

#include <string>

class GURL;

// Formats `url` for display in a localized bottom sheet string: shows the host
// (and non-default port), omitting the HTTP/HTTPS scheme, the path, and a
// trivial "www." subdomain. IDN hosts with strong RTL characters are shown as
// punycode, and the result is wrapped in LTR directional formatting marks
// (U+202A...U+202C) so that it always renders left-to-right.
std::u16string FormatUrlForBottomSheetDisplay(const GURL& url);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_BOTTOM_SHEET_UTIL_H_
