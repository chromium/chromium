// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <memory>

#include "base/macros.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#include "ios/components/security_interstitials/ios_security_interstitial_page.h"
#include "url/gurl.h"

namespace security_interstitials {
class BaseSafeBrowsingErrorUI;
}

// Blocking page for safe browsing interstitials.  Only supports committed
// interstitial behavior.
class SafeBrowsingBlockingPage
    : public security_interstitials::IOSSecurityInterstitialPage {
 public:
  // Creates a safe browsing blocking page that creates the HTML for the error
  // page shown for |resource|.
  static std::unique_ptr<SafeBrowsingBlockingPage> Create(
      const security_interstitials::UnsafeResource& resource);
  ~SafeBrowsingBlockingPage() override;

 private:
  // Controller client used for SafeBrowsing blocking page.
  class SafeBrowsingControllerClient
      : public security_interstitials::IOSBlockingPageControllerClient {
   public:
    SafeBrowsingControllerClient(
        const security_interstitials::UnsafeResource& resource);
    ~SafeBrowsingControllerClient() override;

   private:
    // security_interstitials::ControllerClient:
    void Proceed() override;
    void GoBack() override;
    void GoBackAfterNavigationCommitted() override;

    // The URL of the resource causing the insterstitial.
    GURL url_;
    // The threat type encountered for |url_|.
    safe_browsing::SBThreatType threat_type_;
  };

  // Constructor used by Create().
  SafeBrowsingBlockingPage(
      const security_interstitials::UnsafeResource& resource,
      SafeBrowsingControllerClient* client);

  // Setter for the client.
  void SetClient(std::unique_ptr<SafeBrowsingControllerClient> client);

  // web::WebInterstitialDelegate:
  std::string GetHtmlContents() const override;
  void HandleScriptCommand(const base::DictionaryValue& message,
                           const GURL& origin_url,
                           bool user_is_interacting,
                           web::WebFrame* sender_frame) override;
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) const override;
  void AfterShow() override;

  // The unsafe resource triggering the blocking page.
  security_interstitials::UnsafeResource resource_;
  // Whether the main page load is blocked.
  bool is_main_page_load_blocked_ = false;
  // The controller client.
  std::unique_ptr<SafeBrowsingControllerClient> client_;
  // The WebUI used to create the error page.
  std::unique_ptr<security_interstitials::BaseSafeBrowsingErrorUI> error_ui_;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
