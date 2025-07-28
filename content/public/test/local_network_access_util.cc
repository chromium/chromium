// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/local_network_access_util.h"

#include "base/functional/bind.h"

namespace content {

DeprecationTrialURLLoaderInterceptor::DeprecationTrialURLLoaderInterceptor()
    : interceptor_(base::BindRepeating(
          &DeprecationTrialURLLoaderInterceptor::HandleRequest,
          base::Unretained(this))) {}

DeprecationTrialURLLoaderInterceptor::~DeprecationTrialURLLoaderInterceptor() =
    default;

bool DeprecationTrialURLLoaderInterceptor::HandleRequest(
    RequestParams* request_params) const {
  const GURL& url = request_params->url_request.url;
  if (url == EnabledHttpUrl()) {
    HandleEnabledHttpUrlRequest(*request_params);
    return true;
  }

  if (url == EnabledHttpsUrl()) {
    HandleEnabledHttpsUrlRequest(*request_params);
    return true;
  }

  if (url == DisabledHttpUrl() || url == DisabledHttpsUrl()) {
    HandleDisabledUrlRequest(*request_params);
    return true;
  }

  return false;
}

void DeprecationTrialURLLoaderInterceptor::HandleEnabledHttpUrlRequest(
    RequestParams& request_params) const {
  constexpr char kHeaders[] =      //
      "HTTP/1.1 200 OK\n"          //
      "Content-Type: text/html\n"  //
      // Use CSP to make the page `public`, even though it is served with no
      // IP address information. Without this it is treated as `unknown`, and
      // that interferes with its private network request policy.
      "Content-Security-Policy: treat-as-public-address\n"  //
      // This token was generated using:
      //
      //   $ tools/origin_trials/generate_token.py \
      //       --expire-days 1000 \
      //       --version 3 \
      //       http://enabled.test LocalNetworkAccessNonSecureContextAllowed
      //
      "Origin-Trial: "
      "A8+6/"
      "4bo2hPUNWNCV6kyLxXFPU0ddMhYjnwqkknDOEuN3vRZQXQu84ZPU+"
      "EzYqofTDfcz3zmjXHu8ARvGarh/"
      "w4AAAByeyJvcmlnaW4iOiAiaHR0cDovL2VuYWJsZWQudGVzdDo4MCIsICJmZWF0dXJlIjogI"
      "kxvY2FsTmV0d29ya0FjY2Vzc05vblNlY3VyZUNvbnRleHRBbGxvd2VkIiwgImV4cGlyeSI6I"
      "DE4MzkxOTU4NTZ9"
      "\n\n";
  URLLoaderInterceptor::WriteResponse(kHeaders, "",
                                      request_params.client.get());
}

void DeprecationTrialURLLoaderInterceptor::HandleEnabledHttpsUrlRequest(
    RequestParams& request_params) const {
  constexpr char kHeaders[] =      //
      "HTTP/1.1 200 OK\n"          //
      "Content-Type: text/html\n"  //
      // Use CSP to make the page `public`, even though it is served with no
      // IP address information. Without this it is treated as `unknown`, and
      // that interferes with its private network request policy.
      "Content-Security-Policy: treat-as-public-address\n"  //
      // This token was generated using:
      //
      //   $ tools/origin_trials/generate_token.py \
      //       --expire-days 1000 \
      //       --version 3 \
      //       https://enabled.test LocalNetworkAccessNonSecureContextAllowed
      //
      "Origin-Trial: "
      "AwHpYP8SqPYnzwaXGbjfEmjoQK5RWNZ0zbhc/o2H0PYnMAm0y9Em631RgOwCwqG/"
      "k1mcOCbGZpqmnCpt1iSkfQsAAAB0eyJvcmlnaW4iOiAiaHR0cHM6Ly9lbmFibGVkLnRlc3Q6"
      "NDQzIiwgImZlYXR1cmUiOiAiTG9jYWxOZXR3b3JrQWNjZXNzTm9uU2VjdXJlQ29udGV4dEFs"
      "bG93ZWQiLCAiZXhwaXJ5IjogMTgzOTE5NTgxNX0="
      "\n\n";
  URLLoaderInterceptor::WriteResponse(kHeaders, "",
                                      request_params.client.get());
}

void DeprecationTrialURLLoaderInterceptor::HandleDisabledUrlRequest(
    RequestParams& request_params) const {
  constexpr char kHeaders[] =      //
      "HTTP/1.1 200 OK\n"          //
      "Content-Type: text/html\n"  //
      // See above.
      "Content-Security-Policy: treat-as-public-address\n\n";
  URLLoaderInterceptor::WriteResponse(kHeaders, "",
                                      request_params.client.get());
}

}  // namespace content
