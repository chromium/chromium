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
#include "base/types/expected.h"
#include "content/common/content_export.h"
#include "content/public/common/web_identity.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Used to intercept signin/signout response headers from IDPs.
class CONTENT_EXPORT IdentityUrlLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  IdentityUrlLoaderThrottle(SetIdpStatusCallback status_cb,
                            ParseSetLoginHeaderCallback parse_cb);
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
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;

 private:
  void HandleResponseOrRedirect(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head,
      bool* defer);

  void OnHeaderParsed(
      const url::Origin& idp_origin,
      std::optional<net::structured_headers::ParameterizedItem> item);

  GURL request_url_;
  std::optional<url::Origin> request_initiator_;
  SetIdpStatusCallback set_idp_status_cb_;
  ParseSetLoginHeaderCallback parse_set_login_header_cb_;
  bool is_inside_handler_response_ = false;
  bool is_header_parsed_ = false;

  base::WeakPtrFactory<IdentityUrlLoaderThrottle> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_COMMON_WEBID_IDENTITY_URL_LOADER_THROTTLE_H_
