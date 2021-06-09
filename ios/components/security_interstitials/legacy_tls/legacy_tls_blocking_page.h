// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_BLOCKING_PAGE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_BLOCKING_PAGE_H_

#include "ios/components/security_interstitials/ios_security_interstitial_page.h"
#include "ios/components/security_interstitials/legacy_tls/legacy_tls_controller_client.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that is
// shown for legacy TLS connections.
class LegacyTLSBlockingPage
    : public security_interstitials::IOSSecurityInterstitialPage {
 public:
  ~LegacyTLSBlockingPage() override;

  // Creates a legacy TLS blocking page.
  LegacyTLSBlockingPage(web::WebState* web_state,
                        const GURL& request_url,
                        std::unique_ptr<LegacyTLSControllerClient> client);

 protected:
  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(base::Value* load_time_data) const override;

 private:
  void HandleCommand(
      security_interstitials::SecurityInterstitialCommand command,
      const GURL& origin_url,
      bool user_is_interacting,
      web::WebFrame* sender_frame) override;

  web::WebState* web_state_ = nullptr;
  const GURL request_url_;
  std::unique_ptr<LegacyTLSControllerClient> controller_;

  DISALLOW_COPY_AND_ASSIGN(LegacyTLSBlockingPage);
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_LEGACY_TLS_LEGACY_TLS_BLOCKING_PAGE_H_
