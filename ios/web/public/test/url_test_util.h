// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_URL_TEST_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_URL_TEST_UTIL_H_

#include <string>


class GURL;

namespace web {

// Returns a formatted version of `url` that would be used as the fallback title
// for a page with that URL.
std::u16string GetDisplayTitleForUrl(const GURL& url);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_URL_TEST_UTIL_H_
