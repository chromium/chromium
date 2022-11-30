// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PRIVATE_NETWORK_ACCESS_UTIL_H_
#define CONTENT_PUBLIC_TEST_PRIVATE_NETWORK_ACCESS_UTIL_H_

#include "content/public/test/url_loader_interceptor.h"
#include "url/gurl.h"

namespace content {

// URL loader interceptor for testing the
// `PrivateNetworkAccessNonSecureContextsAllowed` deprecation trial.
//
// Trial tokens are tied to a single origin, which precludes the use of
// `net::EmbeddedTestServer` and its random port assignment. Instead, we resort
// to the use of an interceptor that can serve resources from a fixed origin.
class DeprecationTrialURLLoaderInterceptor {
 public:
  // An interceptor starts intercepting requests as soon as it is constructed.
  DeprecationTrialURLLoaderInterceptor();

  ~DeprecationTrialURLLoaderInterceptor();

  // Instances of this type are neither copyable nor movable.
  DeprecationTrialURLLoaderInterceptor(
      const DeprecationTrialURLLoaderInterceptor&) = delete;
  DeprecationTrialURLLoaderInterceptor& operator=(
      const DeprecationTrialURLLoaderInterceptor&) = delete;

  // Returns the URL of a document that bears a valid trial token.
  GURL EnabledUrl() const { return enabled_url_; }

  // Returns the URL of a document that does not bear a valid trial token.
  GURL DisabledUrl() const { return disabled_url_; }

 private:
  using RequestParams = URLLoaderInterceptor::RequestParams;

  bool HandleRequest(RequestParams* request_params) const;
  void HandleEnabledUrlRequest(RequestParams& request_params) const;
  void HandleDisabledUrlRequest(RequestParams& request_params) const;

  const GURL enabled_url_{"http://enabled.test/"};
  const GURL disabled_url_{"http://disabled.test/"};
  URLLoaderInterceptor interceptor_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PRIVATE_NETWORK_ACCESS_UTIL_H_
