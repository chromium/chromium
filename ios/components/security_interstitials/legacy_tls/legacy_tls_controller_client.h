// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_CONTROLLER_CLIENT_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_CONTROLLER_CLIENT_H_

#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "url/gurl.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

// Controller client used for legacy TLS blocking pages.
class LegacyTLSControllerClient
    : public security_interstitials::IOSBlockingPageControllerClient {
 public:
  LegacyTLSControllerClient(web::WebState* web_state,
                            const GURL& request_url,
                            const std::string& app_locale);
  ~LegacyTLSControllerClient() override;

  // security_interstitials::ControllerClient:
  void Proceed() override;

 private:
  // The URL of the page causing the insterstitial.
  const GURL request_url_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_CONTROLLER_CLIENT_H_
