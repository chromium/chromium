// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDCM_IDP_REGISTRATION_HANDLER_H_
#define CONTENT_BROWSER_WEBID_FEDCM_IDP_REGISTRATION_HANDLER_H_

#include "content/browser/webid/fedcm_config_fetcher.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT FedCmIdpRegistrationHandler {
 public:
  FedCmIdpRegistrationHandler(RenderFrameHost& render_frame_host,
                              IdpNetworkRequestManager* network_manager,
                              const GURL& idp_url);
  ~FedCmIdpRegistrationHandler();

  FedCmIdpRegistrationHandler(const FedCmIdpRegistrationHandler&) = delete;
  FedCmIdpRegistrationHandler& operator=(const FedCmIdpRegistrationHandler&) =
      delete;

  void FetchConfig(FedCmConfigFetcher::RequesterCallback callback);

 private:
  // Owned by FederatedAuthRequestImpl.
  raw_ref<RenderFrameHost> render_frame_host_;
  raw_ptr<IdpNetworkRequestManager> network_manager_;

  GURL idp_url_;
  std::unique_ptr<FedCmConfigFetcher> config_fetcher_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDCM_IDP_REGISTRATION_HANDLER_H_
