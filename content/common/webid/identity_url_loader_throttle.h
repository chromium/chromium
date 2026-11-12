// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEBID_IDENTITY_URL_LOADER_THROTTLE_H_
#define CONTENT_COMMON_WEBID_IDENTITY_URL_LOADER_THROTTLE_H_

#include <optional>

#include "content/common/content_export.h"
#include "content/public/common/web_identity.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// Used to intercept signin/signout response headers from IDPs.
class CONTENT_EXPORT IdentityUrlLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  explicit IdentityUrlLoaderThrottle(SetIdpStatusCallback status_cb);
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
      const network::mojom::URLResponseHead& response_head);

  GURL request_url_;
  std::optional<url::Origin> request_initiator_;
  network::mojom::RequestDestination request_destination_ =
      network::mojom::RequestDestination::kEmpty;
  SetIdpStatusCallback set_idp_status_cb_;
};

}  // namespace content

#endif  // CONTENT_COMMON_WEBID_IDENTITY_URL_LOADER_THROTTLE_H_
