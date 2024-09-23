// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/data_response_provider.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"

namespace web {

std::unique_ptr<net::test_server::HttpResponse>
DataResponseProvider::GetEmbeddedTestServerResponse(const Request& request) {
  std::string response_body;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  GetResponseHeadersAndBody(request, &response_headers, &response_body);

  std::unique_ptr<net::test_server::BasicHttpResponse> data_response =
      std::make_unique<net::test_server::BasicHttpResponse>();

  data_response->set_code(
      static_cast<net::HttpStatusCode>(response_headers->response_code()));
  data_response->set_content(response_body);

  size_t iter = 0;
  std::string name;
  std::string value;
  while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
    // TODO(crbug.com/40394910): Extract out other names that can't be set by
    // using the `setValue:forAdditionalHeader:` API such as "ETag" etc.
    if (name == "Content-type") {
      data_response->set_content_type(value);
      continue;
    }
    data_response->AddCustomHeader(name, value);
  }
  return std::move(data_response);
}

}  // namespace web
