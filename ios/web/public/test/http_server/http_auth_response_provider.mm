// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/http_auth_response_provider.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

HttpAuthResponseProvider::HttpAuthResponseProvider(const GURL& url,
                                                   const std::string& realm,
                                                   const std::string& username,
                                                   const std::string& password)
    : url_(url), realm_(realm), username_(username), password_(password) {}

HttpAuthResponseProvider::~HttpAuthResponseProvider() = default;

bool HttpAuthResponseProvider::CanHandleRequest(const Request& request) {
  return request.url == url_;
}

void HttpAuthResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  if (HeadersHaveValidCredentials(request.headers)) {
    *response_body = page_text();
    *headers = GetDefaultResponseHeaders();
  } else {
    *headers = GetResponseHeaders("", net::HTTP_UNAUTHORIZED);
    (*headers)->AddHeader(
        "WWW-Authenticate",
        base::StringPrintf("Basic realm=\"%s\"", realm_.c_str()));
  }
}

bool HttpAuthResponseProvider::HeadersHaveValidCredentials(
    const net::HttpRequestHeaders& headers) {
  std::string header;
  if (headers.GetHeader(net::HttpRequestHeaders::kAuthorization, &header)) {
    std::string auth =
        base::StringPrintf("%s:%s", username_.c_str(), password_.c_str());
    std::string encoded_auth;
    base::Base64Encode(auth, &encoded_auth);
    return header == base::StringPrintf("Basic %s", encoded_auth.c_str());
  }
  return false;
}

}  // namespace web
