// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_BLANK_PAGE_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_BLANK_PAGE_RESPONSE_PROVIDER_H_

#include <memory>

class GURL;

namespace web {

class ResponseProvider;

namespace test {

// Creates a ResponseProvider that returns an empty HTML document for
// `url`.
std::unique_ptr<ResponseProvider> CreateBlankPageResponseProvider(
    const GURL& url);

}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_BLANK_PAGE_RESPONSE_PROVIDER_H_
