// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/logging.h"
#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios_ns_url_session_bridge.h"
#import "ios/web/public/browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

GaiaAuthFetcherIOS::GaiaAuthFetcherIOS(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    web::BrowserState* browser_state)
    : GaiaAuthFetcher(consumer, source, url_loader_factory),
      browser_state_(browser_state),
      bridge_(new GaiaAuthFetcherIOSNSURLSessionBridge(this, browser_state_)) {}

GaiaAuthFetcherIOS::~GaiaAuthFetcherIOS() {}

void GaiaAuthFetcherIOS::CreateAndStartGaiaFetcher(
    const std::string& body,
    const std::string& body_content_type,
    const std::string& headers,
    const GURL& gaia_gurl,
    network::mojom::CredentialsMode credentials_mode,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!HasPendingFetch()) << "Tried to fetch two things at once!";

  bool cookies_required =
      credentials_mode != network::mojom::CredentialsMode::kOmit &&
      credentials_mode !=
          network::mojom::CredentialsMode::kOmitBug_775438_Workaround;

  if (!cookies_required) {
    GaiaAuthFetcher::CreateAndStartGaiaFetcher(body, body_content_type, headers,
                                               gaia_gurl, credentials_mode,
                                               traffic_annotation);
    return;
  }

  DVLOG(2) << "Gaia fetcher URL: " << gaia_gurl.spec();
  DVLOG(2) << "Gaia fetcher headers: " << headers;
  DVLOG(2) << "Gaia fetcher body: " << body;

  // The fetch requires cookies and WKWebView is being used. The only way to do
  // a network request with cookies sent and saved is by making it through a
  // WKWebView.
  SetPendingFetch(true);
  bool should_use_xml_http_request = IsMultiloginUrl(gaia_gurl);
  bridge_->Fetch(gaia_gurl, headers, body, should_use_xml_http_request);
}

void GaiaAuthFetcherIOS::OnFetchComplete(const GURL& url,
                                         const std::string& data,
                                         net::Error net_error,
                                         int response_code) {
  VLOG(2) << "Response " << url.spec() << ", code = " << response_code << "\n";
  VLOG(2) << "data: " << data << "\n";
  SetPendingFetch(false);
  DispatchFetchedRequest(url, data, net_error, response_code);
}
