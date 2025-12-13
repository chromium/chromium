// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_LOCAL_NETWORK_ACCESS_UTIL_H_
#define CONTENT_PUBLIC_TEST_LOCAL_NETWORK_ACCESS_UTIL_H_

#include <optional>
#include <string>

#include "content/public/test/url_loader_interceptor.h"
#include "url/gurl.h"

namespace content {

// URL loader interceptor for testing the
// `LocalNetworkAccessNonSecureContextAllowed` deprecation trial.
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
  GURL EnabledHttpUrl() const { return enabled_http_url_; }
  GURL EnabledHttpsUrl() const { return enabled_https_url_; }
  GURL EnabledHttpWorkerUrl() const { return enabled_http_worker_url_; }
  GURL EnabledHttpSharedWorkerUrl() const {
    return enabled_http_shared_worker_url_;
  }

  // Returns the URL of a document that does not bear a valid trial token.
  GURL DisabledHttpUrl() const { return disabled_http_url_; }
  GURL DisabledHttpsUrl() const { return disabled_https_url_; }

 private:
  using RequestParams = URLLoaderInterceptor::RequestParams;

  bool HandleRequest(RequestParams* request_params) const;
  // body_file, if provide, represents the relative path of a file to serve as
  // the response body.
  void HandleEnabledHttpUrlRequest(RequestParams& request_params,
                                   std::optional<std::string> body_file) const;
  void HandleEnabledHttpsUrlRequest(RequestParams& request_params) const;
  void HandleDisabledUrlRequest(RequestParams& request_params) const;

  const GURL enabled_http_url_{"http://enabled.test/"};
  const GURL disabled_http_url_{"http://disabled.test/"};
  const GURL enabled_https_url_{"https://enabled.test/"};
  const GURL disabled_https_url_{"https://disabled.test/"};
  const GURL enabled_http_worker_url_{"http://enabled.test/worker"};
  const GURL enabled_http_shared_worker_url_{
      "http://enabled.test/sharedworker"};
  const GURL enabled_http_worker_js_url_{
      "http://enabled.test/request-from-worker-as-public-address.js"};
  const GURL enabled_http_shared_worker_js_url_{
      "http://enabled.test/fetch-from-shared-worker-as-public-address.js"};
  URLLoaderInterceptor interceptor_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_LOCAL_NETWORK_ACCESS_UTIL_H_
