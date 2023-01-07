// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_SERVER_UTIL_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_SERVER_UTIL_H_

#include <map>
#include <memory>

#include "url/gurl.h"

namespace web {

class ResponseProvider;

namespace test {

// Sets up a web::test::HttpServer with a simple HtmlResponseProvider. The
// HtmlResponseProvider will use the `responses` map to resolve URLs.
void SetUpSimpleHttpServer(const std::map<GURL, std::string>& responses);

// Sets up a web::test::HttpServer with a single custom provider.
// Takes ownership of the provider.
void SetUpHttpServer(std::unique_ptr<web::ResponseProvider> provider);

// Adds a single custom provider.
// Takes ownership of the provider.
void AddResponseProvider(std::unique_ptr<web::ResponseProvider> provider);
}  // namespace test
}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_SERVER_UTIL_H_
