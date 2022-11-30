// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_GAIA_AUTH_FETCHER_H_
#define IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_GAIA_AUTH_FETCHER_H_

#include "base/memory/scoped_refptr.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ios_web_view {

// Implementation of GaiaAuthFetcher for ios/web_view.
class WebViewGaiaAuthFetcher : public GaiaAuthFetcher {
 public:
  WebViewGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 protected:
  void CreateAndStartGaiaFetcher(
      const std::string& body,
      const std::string& body_content_type,
      const std::string& headers,
      const GURL& gaia_gurl,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  // String representation of the passed in gaia::GaiaSource.
  std::string source_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SIGNIN_WEB_VIEW_GAIA_AUTH_FETCHER_H_
