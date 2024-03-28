// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEBID_IDENTITY_URL_LOADER_THROTTLE_H_
#define CONTENT_COMMON_WEBID_IDENTITY_URL_LOADER_THROTTLE_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/web_identity.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

// Used to intercept signin/signout response headers from IDPs.
class CONTENT_EXPORT IdentityUrlLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  explicit IdentityUrlLoaderThrottle(SetIdpStatusCallback callback);
  ~IdentityUrlLoaderThrottle() override;
  IdentityUrlLoaderThrottle(const IdentityUrlLoaderThrottle&) = delete;
  IdentityUrlLoaderThrottle& operator=(const IdentityUrlLoaderThrottle&) =
      delete;

  // URLLoaderThrottle implementation:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(IdentityUrlLoaderThrottleTest, HeaderHasToken);

  void HandleResponseOrRedirect(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head);

  static bool HeaderHasToken(const net::HttpResponseHeaders& headers,
                             std::string_view header_name,
                             std::string_view token);

  GURL request_url_;
  SetIdpStatusCallback set_idp_status_cb_;
  bool has_user_gesture_ = false;

  base::WeakPtrFactory<IdentityUrlLoaderThrottle> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_COMMON_WEBID_IDENTITY_URL_LOADER_THROTTLE_H_
