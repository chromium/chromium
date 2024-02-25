// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_H_

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios_bridge.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GaiaAuthFetcherIOSBridge;
class GURL;

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
  GaiaAuthFetcherIOS(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      web::BrowserState* browser_state);

  GaiaAuthFetcherIOS(const GaiaAuthFetcherIOS&) = delete;
  GaiaAuthFetcherIOS& operator=(const GaiaAuthFetcherIOS&) = delete;

  ~GaiaAuthFetcherIOS() override;

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
                       net::Error net_error,
                       int response_code) override;

  raw_ptr<web::BrowserState> browser_state_;
  std::unique_ptr<GaiaAuthFetcherIOSBridge> bridge_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_GAIA_AUTH_FETCHER_IOS_H_
