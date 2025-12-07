// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDP_REGISTRATION_HANDLER_H_
#define CONTENT_BROWSER_WEBID_IDP_REGISTRATION_HANDLER_H_

#include "content/browser/webid/config_fetcher.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;

namespace webid {

class CONTENT_EXPORT IdpRegistrationHandler {
 public:
  IdpRegistrationHandler(RenderFrameHost& render_frame_host,
                         IdpNetworkRequestManager* network_manager,
                         const GURL& idp_url);
  ~IdpRegistrationHandler();

  IdpRegistrationHandler(const IdpRegistrationHandler&) = delete;
  IdpRegistrationHandler& operator=(const IdpRegistrationHandler&) = delete;

  void FetchConfig(ConfigFetcher::RequesterCallback callback);

 private:
  // Owned by RequestService.
  raw_ref<RenderFrameHost> render_frame_host_;
  raw_ptr<IdpNetworkRequestManager> network_manager_;

  GURL idp_url_;
  std::unique_ptr<ConfigFetcher> config_fetcher_;
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_REGISTRATION_HANDLER_H_
