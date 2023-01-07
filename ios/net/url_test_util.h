// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_URL_TEST_UTIL_H_
#define IOS_NET_URL_TEST_UTIL_H_

#include <string>

class GURL;

namespace net {

// Returns the content and frament of |url| concatenated together. For example:
//
//  http://www.example.com/some/content.html?param1=foo#fragment_data
//
//  Returns "www.example.com/some/content.html?param1=foo#fragment_data".
std::string GetContentAndFragmentForUrl(const GURL& url);

}  // namespace net

#endif  // IOS_NET_URL_TEST_UTIL_H_
