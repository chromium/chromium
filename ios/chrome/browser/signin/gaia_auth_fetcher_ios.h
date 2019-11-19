// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_bridge.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GaiaAuthFetcherIOSBridge;
class GURL;

namespace net {
class URLRequestStatus;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace web {
class BrowserState;
}  // namespace web

// Specialization of GaiaAuthFetcher on iOS.
//
// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
// The queries are fetched using native APIs.
class GaiaAuthFetcherIOS
    : public GaiaAuthFetcher,
      public GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate {
 public:
  // Sets whether the iOS specialization of the GaiaAuthFetcher should be used.
  // Mainly used for testing.
  // Note that if |should_use| is true, it might still not be used if it is
  // unnecessary or WKWebView isn't enabled.
  static void SetShouldUseGaiaAuthFetcherIOSForTesting(
      bool use_gaia_fetcher_ios);

  // Returns whether the iOS specialization of the GaiaAuthFetcher should be
  // used.
  static bool ShouldUseGaiaAuthFetcherIOS();

  GaiaAuthFetcherIOS(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      web::BrowserState* browser_state);
  ~GaiaAuthFetcherIOS() override;

  void CancelRequest() override;

 private:
  friend class GaiaAuthFetcherIOSBridge;
  friend class GaiaAuthFetcherIOSTest;

  // GaiaAuthFetcher.
  void CreateAndStartGaiaFetcher(
      const std::string& body,
      const std::string& body_content_type,
      const std::string& headers,
      const GURL& gaia_gurl,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  // GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate.
  void OnFetchComplete(const GURL& url,
                       const std::string& data,
                       const net::URLRequestStatus& status,
                       int response_code) override;

  std::unique_ptr<GaiaAuthFetcherIOSBridge> bridge_;
  web::BrowserState* browser_state_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOS);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_H_
