// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_MODEL_IOS_SSL_BLOCKING_PAGE_H_
#define IOS_CHROME_BROWSER_SSL_MODEL_IOS_SSL_BLOCKING_PAGE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
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
  IOSSSLBlockingPage(const IOSSSLBlockingPage&) = delete;
  IOSSSLBlockingPage& operator=(const IOSSSLBlockingPage&) = delete;

  ~IOSSSLBlockingPage() override;

  // Creates an SSL blocking page. If the blocking page isn't shown, the caller
  // is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown. `options_mask` must be a bitwise
  // mask of SSLErrorOptionsMask values.
  IOSSSLBlockingPage(
      web::WebState* web_state,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
          client);

 protected:
  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::Value::Dict& load_time_data) const override;

 private:
  void HandleCommand(
      security_interstitials::SecurityInterstitialCommand command) override;

  // Returns true if `options_mask` refers to a soft-overridable SSL error.
  static bool IsOverridable(int options_mask);

  raw_ptr<web::WebState> web_state_ = nullptr;
  const net::SSLInfo ssl_info_;
  const bool overridable_;  // The UI allows the user to override the error.

  std::unique_ptr<security_interstitials::IOSBlockingPageControllerClient>
      controller_;
  std::unique_ptr<security_interstitials::SSLErrorUI> ssl_error_ui_;
};

#endif  // IOS_CHROME_BROWSER_SSL_MODEL_IOS_SSL_BLOCKING_PAGE_H_
