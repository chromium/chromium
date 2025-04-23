// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_IOS_ENTERPRISE_INTERSTITIAL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_IOS_ENTERPRISE_INTERSTITIAL_H_

#import "components/enterprise/connectors/core/enterprise_interstitial_base.h"
#import "ios/components/security_interstitials/ios_blocking_page_controller_client.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"

namespace enterprise_connectors {

class IOSEnterpriseInterstitial
    : public security_interstitials::IOSSecurityInterstitialPage,
      public EnterpriseInterstitialBase {
 public:
  static std::unique_ptr<IOSEnterpriseInterstitial> CreateBlockingPage(
      const security_interstitials::UnsafeResource& resource);
  static std::unique_ptr<IOSEnterpriseInterstitial> CreateWarningPage(
      const security_interstitials::UnsafeResource& resource);
  ~IOSEnterpriseInterstitial() override;

  // security_interstitials::IOSSecurityInterstitialPage:
  std::string GetHtmlContents() const override;
  void HandleCommand(
      security_interstitials::SecurityInterstitialCommand command) override;
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::Value::Dict& load_time_data) const override;
  void WasDismissed() override;

  // EnterpriseInterstitialBase:
  const std::vector<security_interstitials::UnsafeResource>& unsafe_resources()
      const override;
  GURL request_url() const override;

 protected:
  class EnterprisePageControllerClient
      : public security_interstitials::IOSBlockingPageControllerClient {
   public:
    EnterprisePageControllerClient(
        const security_interstitials::UnsafeResource& resource);
    ~EnterprisePageControllerClient() override;

    // Handles commands forwarded from the IOSEnterpriseInterstitial.
    void HandleCommand(
        security_interstitials::SecurityInterstitialCommand command);

    // security_interstitials::ControllerClient:
    void Proceed() override;
    void GoBack() override;
    void GoBackAfterNavigationCommitted() override;

   private:
    // Removes pending navigation decisions from the allow list. Call this if
    // the user goes back or closes the interstitial.
    void RemovePendingUnsafeNavigationDecisionsFromAllowList();

    // Stored details from the UnsafeResource needed for command handling.
    GURL request_url_;
    safe_browsing::SBThreatType threat_type_;
    safe_browsing::ThreatSource threat_source_;
  };

  IOSEnterpriseInterstitial(
      const security_interstitials::UnsafeResource& resource,
      std::unique_ptr<EnterprisePageControllerClient> client);

 private:
  // The unsafe resource(s) triggering the enterprise blocking/warning page.
  std::vector<security_interstitials::UnsafeResource> unsafe_resources_;
  // The controller client responsible for handling user interactions and
  // navigation logic.
  std::unique_ptr<EnterprisePageControllerClient> client_;
};

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_IOS_ENTERPRISE_INTERSTITIAL_H_
