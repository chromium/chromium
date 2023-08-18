// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/delayed_response_provider.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "net/test/embedded_test_server/http_response.h"

namespace web {

DelayedResponseProvider::DelayedResponseProvider(
    std::unique_ptr<web::ResponseProvider> delayed_provider,
    base::TimeDelta delay)
    : web::ResponseProvider(),
      delayed_provider_(std::move(delayed_provider)),
      delay_(delay) {}

DelayedResponseProvider::~DelayedResponseProvider() {}

bool DelayedResponseProvider::CanHandleRequest(const Request& request) {
  return delayed_provider_->CanHandleRequest(request);
}

std::unique_ptr<net::test_server::HttpResponse>
DelayedResponseProvider::GetEmbeddedTestServerResponse(const Request& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      std::make_unique<net::test_server::DelayedHttpResponse>(delay_));
  http_response->set_content_type("text/html");
  http_response->set_content("Slow Page");
  return std::move(http_response);
}
}  // namespace web
