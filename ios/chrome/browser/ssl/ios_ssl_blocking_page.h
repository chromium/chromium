// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_IOS_SSL_BLOCKING_PAGE_H_
#define IOS_CHROME_BROWSER_SSL_IOS_SSL_BLOCKING_PAGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ios/components/security_interstitials/ios_security_interstitial_page.h"
#include "net/ssl/ssl_info.h"

class IOSBlockingPageControllerClient;
class GURL;

namespace security_interstitials {
class SSLErrorUI;
}

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error happens.
// It deletes itself when the interstitial page is closed.
class IOSSSLBlockingPage
    : public security_interstitials::IOSSecurityInterstitialPage {
 public:
  ~IOSSSLBlockingPage() override;

  // Creates an SSL blocking page. If the blocking page isn't shown, the caller
  // is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown. |options_mask| must be a bitwise
  // mask of SSLErrorOptionsMask values.
  IOSSSLBlockingPage(
      web::WebState* web_state,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      base::OnceCallback<void(bool)> callback,
      std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
          client);

 protected:
  // InterstitialPageDelegate implementation.
  void CommandReceived(const std::string& command) override;
  void OnProceed() override;
  void OnDontProceed() override;
  void OverrideItem(web::NavigationItem* item) override;

  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) const override;
  void AfterShow() override;

 private:
  void NotifyDenyCertificate();
  void HandleScriptCommand(const base::DictionaryValue& message,
                           const GURL& origin_url,
                           bool user_is_interacting,
                           web::WebFrame* sender_frame) override;

  // Returns true if |options_mask| refers to a soft-overridable SSL error.
  static bool IsOverridable(int options_mask);

  web::WebState* web_state_ = nullptr;
  base::OnceCallback<void(bool)> callback_;
  const net::SSLInfo ssl_info_;
  const bool overridable_;  // The UI allows the user to override the error.

  std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
      controller_;
  std::unique_ptr<security_interstitials::SSLErrorUI> ssl_error_ui_;

  DISALLOW_COPY_AND_ASSIGN(IOSSSLBlockingPage);
};

#endif  // IOS_CHROME_BROWSER_SSL_IOS_SSL_BLOCKING_PAGE_H_
