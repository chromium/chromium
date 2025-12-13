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
  if (url == EnabledHttpUrl() || url == EnabledHttpWorkerUrl() ||
      url == EnabledHttpSharedWorkerUrl()) {
    std::optional<std::string> body_file = std::nullopt;
    if (url == EnabledHttpWorkerUrl()) {
      body_file =
          "chrome/test/data/local_network_access/"
          "request-from-worker-as-public-address.html";
    } else if (url == EnabledHttpSharedWorkerUrl()) {
      body_file =
          "chrome/test/data/local_network_access/"
          "fetch-from-shared-worker-as-public-address.html";
    }
    HandleEnabledHttpUrlRequest(*request_params, body_file);
    return true;
  }

  if (url == enabled_http_worker_js_url_) {
    // Served off of the http://enabled.test domain, but no origin trial token
    // served. This is needed because the worker html hosting will request the
    // JS based on the worker hosting domain which isn't mapped to the right
    // port for the embedded test server, and so needs to be served from this
    // interceptor.
    //
    // TODO(crbug.com/395895368): remove this functionality when we move away
    // from using this interceptor for browser tests and use a library function
    // like in https://crbug.com/40860522#comment8.
    URLLoaderInterceptor::WriteResponse(
        "chrome/test/data/local_network_access/"
        "request-from-worker-as-public-address.js",
        request_params->client.get());
    return true;
  }

  if (url == enabled_http_shared_worker_js_url_) {
    URLLoaderInterceptor::WriteResponse(
        "chrome/test/data/local_network_access/"
        "fetch-from-shared-worker-as-public-address.js",
        request_params->client.get());
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
    RequestParams& request_params,
    std::optional<std::string> body_file) const {
  std::string headers =            //
      "HTTP/1.1 200 OK\n"          //
      "Content-Type: text/html\n"  //
      // Use CSP to make the page `public`, even though it is served with no
      // IP address information. Without this it is treated as `unknown`, and
      // that interferes with its local network request policy.
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
  if (body_file) {
    URLLoaderInterceptor::WriteResponse(*body_file, request_params.client.get(),
                                        &headers);
  } else {
    URLLoaderInterceptor::WriteResponse(headers, "",
                                        request_params.client.get());
  }
}

void DeprecationTrialURLLoaderInterceptor::HandleEnabledHttpsUrlRequest(
    RequestParams& request_params) const {
  constexpr char kHeaders[] =      //
      "HTTP/1.1 200 OK\n"          //
      "Content-Type: text/html\n"  //
      // Use CSP to make the page `public`, even though it is served with no
      // IP address information. Without this it is treated as `unknown`, and
      // that interferes with its local network request policy.
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
