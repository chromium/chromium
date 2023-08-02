// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/string_response_provider.h"

namespace web {

StringResponseProvider::StringResponseProvider(const std::string& response_body)
    : response_body_(response_body) {}

bool StringResponseProvider::CanHandleRequest(const Request& request) {
  return true;
}

void StringResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  *headers = GetDefaultResponseHeaders();
  *response_body = response_body_;
}

}  // namespace web
