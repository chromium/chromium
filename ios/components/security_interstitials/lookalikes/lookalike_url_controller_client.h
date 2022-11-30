// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_CONTROLLER_CLIENT_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_CONTROLLER_CLIENT_H_

#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "url/gurl.h"

class GURL;

namespace web {
class WebState;
}  // namespace web

// Controller client used for lookalike URL blocking pages.
class LookalikeUrlControllerClient
    : public security_interstitials::IOSBlockingPageControllerClient {
 public:
  LookalikeUrlControllerClient(web::WebState* web_state,
                               const GURL& safe_url,
                               const GURL& request_url,
                               const std::string& app_locale);
  ~LookalikeUrlControllerClient() override;

  // security_interstitials::ControllerClient:
  void Proceed() override;
  void GoBack() override;

 private:
  // The URL suggested to the user as the safe URL. Can be empty, in which case
  // the default action on the interstitial closes the tab.
  const GURL safe_url_;
  // The URL of the page causing the insterstitial.
  const GURL request_url_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LOOKALIKES_LOOKALIKE_URL_CONTROLLER_CLIENT_H_
