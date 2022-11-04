// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DELAYED_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DELAYED_RESPONSE_PROVIDER_H_

#include <memory>

#include "base/time/time.h"
#import "ios/web/public/test/http_server/response_provider.h"

namespace web {

// A Response provider that delays the response provided by another response
// provider
class DelayedResponseProvider : public ResponseProvider {
 public:
  // Creates a DelayedResponseProvider that delays the response from
  // `delayed_provider` by `delay`.
  DelayedResponseProvider(
      std::unique_ptr<web::ResponseProvider> delayed_provider,
      base::TimeDelta delay);

  DelayedResponseProvider(const DelayedResponseProvider&) = delete;
  DelayedResponseProvider& operator=(const DelayedResponseProvider&) = delete;

  ~DelayedResponseProvider() override;

  // Forwards to `delayed_provider_`.
  bool CanHandleRequest(const Request& request) override;

  // Creates a test_server::HttpResponse that will delay the read operation
  // by `delay_` seconds.
  std::unique_ptr<net::test_server::HttpResponse> GetEmbeddedTestServerResponse(
      const Request& request) override;

 private:
  std::unique_ptr<web::ResponseProvider> delayed_provider_;
  const base::TimeDelta delay_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_DELAYED_RESPONSE_PROVIDER_H_
