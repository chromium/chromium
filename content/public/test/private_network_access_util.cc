// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/private_network_access_util.h"

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
  if (url == EnabledUrl()) {
    HandleEnabledUrlRequest(*request_params);
    return true;
  }

  if (url == DisabledUrl()) {
    HandleDisabledUrlRequest(*request_params);
    return true;
  }

  return false;
}

void DeprecationTrialURLLoaderInterceptor::HandleEnabledUrlRequest(
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
      //       --expire-days 5000 \
      //       --version 3 \
      //       http://enabled.test PrivateNetworkAccessNonSecureContextsAllowed
      //
      "Origin-Trial: "
      "A4dgNIB2F3P8qkQQes/oiaobjPNRbfZcaPd5TqdcIHUlpX3/al3rvk5b4f+dnke3WcsXeX"
      "4aMNENL3mg1FM8+wYAAAB1eyJvcmlnaW4iOiAiaHR0cDovL2VuYWJsZWQudGVzdDo4MCIs"
      "ICJmZWF0dXJlIjogIlByaXZhdGVOZXR3b3JrQWNjZXNzTm9uU2VjdXJlQ29udGV4dHNBbG"
      "xvd2VkIiwgImV4cGlyeSI6IDIwNTcxNDYwMzB9"  //
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
