// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/html_response_provider.h"

#import "ios/web/public/test/http_server/response_provider.h"
#import "url/gurl.h"

HtmlResponseProvider::HtmlResponseProvider()
    : response_provider_impl_(new HtmlResponseProviderImpl()) {}

HtmlResponseProvider::HtmlResponseProvider(
    const std::map<GURL, std::string>& responses)
    : response_provider_impl_(new HtmlResponseProviderImpl(responses)) {}

HtmlResponseProvider::HtmlResponseProvider(
    const std::map<GURL, std::pair<std::string, std::string>>& responses)
    : response_provider_impl_(new HtmlResponseProviderImpl(responses)) {}

HtmlResponseProvider::HtmlResponseProvider(
    const std::map<GURL, HtmlResponseProviderImpl::Response>& responses)
    : response_provider_impl_(new HtmlResponseProviderImpl(responses)) {}

HtmlResponseProvider::~HtmlResponseProvider() {}

bool HtmlResponseProvider::CanHandleRequest(
    const web::ResponseProvider::Request& request) {
  return response_provider_impl_->CanHandleRequest(request);
}

void HtmlResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  response_provider_impl_->GetResponseHeadersAndBody(request, headers,
                                                     response_body);
}
