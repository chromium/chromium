// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_BLOCKING_PAGE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_BLOCKING_PAGE_H_

#import <memory>
#import <string>

#import "base/values.h"
#import "components/enterprise/net/core/enterprise_proxy_error_data.h"
#import "components/security_interstitials/core/controller_client.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"

namespace security_interstitials {
class IOSBlockingPageControllerClient;
}  // namespace security_interstitials

namespace web {
class WebState;
}  // namespace web

// Secure Gateway interstitial displayed for a disguised enterprise proxy error.
// A single class covers the authentication, authorization and other error
// pages: the template picks the icon, strings and button from the
// `error_category` parameter, and only the button action differs.
class EnterpriseProxyBlockingPage
    : public security_interstitials::IOSSecurityInterstitialPage {
 public:
  // Creates the page displayed in `web_state` for `error_data`.
  // `error_page_params` are the template parameters returned by
  // `EnterpriseProxyErrorService::GetErrorPageParams()` for `error_data`.
  static std::unique_ptr<EnterpriseProxyBlockingPage> Create(
      web::WebState* web_state,
      const enterprise_net::EnterpriseProxyErrorData& error_data,
      base::DictValue error_page_params);

  EnterpriseProxyBlockingPage(const EnterpriseProxyBlockingPage&) = delete;
  EnterpriseProxyBlockingPage& operator=(const EnterpriseProxyBlockingPage&) =
      delete;

  ~EnterpriseProxyBlockingPage() override;

  // security_interstitials::IOSSecurityInterstitialPage:
  std::string GetHtmlContents() const override;
  void HandleCommand(
      security_interstitials::SecurityInterstitialCommand command) override;

 protected:
  // security_interstitials::IOSSecurityInterstitialPage:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictValue& load_time_data) const override;

 private:
  // Creates the page. `client` handles the navigation commands of the page.
  EnterpriseProxyBlockingPage(
      web::WebState* web_state,
      const enterprise_net::EnterpriseProxyErrorData& error_data,
      base::DictValue error_page_params,
      std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
          client);

  // Category of the error, which decides the page variation.
  const enterprise_net::EnterpriseProxyErrorData::ErrorCategory category_;
  // Template parameters of the page. Kept rather than computed on demand since
  // `EnterpriseProxyErrorService::GetErrorPageParams()` records a histogram.
  const base::DictValue error_page_params_;
  // Handles the navigation commands of the page. The base class keeps a
  // non-owning pointer to it, so it must not be reset or replaced.
  const std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
      client_;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_ENTERPRISE_PROXY_BLOCKING_PAGE_H_
