// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/web_view_gaia_auth_fetcher.h"

#include "google_apis/gaia/gaia_urls.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

namespace ios_web_view {

WebViewGaiaAuthFetcher::WebViewGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : GaiaAuthFetcher(consumer, source, url_loader_factory),
      source_(source.ToString()) {}

void WebViewGaiaAuthFetcher::CreateAndStartGaiaFetcher(
    const std::string& body,
    const std::string& body_content_type,
    const std::string& headers,
    const GURL& gaia_gurl,
    network::mojom::CredentialsMode credentials_mode,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // ios/web_view does not manage gaia auth cookies in the content area.
  DCHECK_EQ(gaia_gurl,
            GaiaUrls::GetInstance()->ListAccountsURLWithSource(source_));
  DispatchFetchedRequest(gaia_gurl, /*data=*/"",
                         net::Error::ERR_NOT_IMPLEMENTED,
                         net::HttpStatusCode::HTTP_NOT_IMPLEMENTED);
}

}  // namespace ios_web_view
