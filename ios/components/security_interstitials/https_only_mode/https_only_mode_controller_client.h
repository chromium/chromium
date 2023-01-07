// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_CONTROLLER_CLIENT_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_CONTROLLER_CLIENT_H_

#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "url/gurl.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

// Controller client used for HTTPS-Only mode blocking pages.
class HttpsOnlyModeControllerClient
    : public security_interstitials::IOSBlockingPageControllerClient {
 public:
  HttpsOnlyModeControllerClient(web::WebState* web_state,
                                const GURL& request_url,
                                const std::string& app_locale);
  ~HttpsOnlyModeControllerClient() override;

  // security_interstitials::ControllerClient:
  void Proceed() override;
  void GoBack() override;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_CONTROLLER_CLIENT_H_
