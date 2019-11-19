// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"

#import <WebKit/WebKit.h>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/signin/feature_flags.h"
#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios_ns_url_session_bridge.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios_wk_webview_bridge.h"
#include "ios/web/common/features.h"
#include "ios/web/public/browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Whether the iOS specialization of the GaiaAuthFetcher should be used.
bool g_should_use_gaia_auth_fetcher_ios = true;

}  // namespace

GaiaAuthFetcherIOS::GaiaAuthFetcherIOS(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    web::BrowserState* browser_state)
    : GaiaAuthFetcher(consumer, source, url_loader_factory),
      browser_state_(browser_state) {
  if (base::FeatureList::IsEnabled(kUseNSURLSessionForGaiaSigninRequests)) {
    bridge_.reset(
        new GaiaAuthFetcherIOSNSURLSessionBridge(this, browser_state));
  } else {
    bridge_.reset(new GaiaAuthFetcherIOSWKWebViewBridge(this, browser_state));
  }
}

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
      credentials_mode != network::mojom::CredentialsMode::kOmit;
  if (!ShouldUseGaiaAuthFetcherIOS() || !cookies_required) {
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

void GaiaAuthFetcherIOS::CancelRequest() {
  if (!HasPendingFetch()) {
    return;
  }
  bridge_->Cancel();
  GaiaAuthFetcher::CancelRequest();
}

void GaiaAuthFetcherIOS::OnFetchComplete(const GURL& url,
                                         const std::string& data,
                                         const net::URLRequestStatus& status,
                                         int response_code) {
  DVLOG(2) << "Response " << url.spec() << ", code = " << response_code << "\n";
  DVLOG(2) << "data: " << data << "\n";
  SetPendingFetch(false);
  DispatchFetchedRequest(url, data, static_cast<net::Error>(status.error()),
                         response_code);
}

void GaiaAuthFetcherIOS::SetShouldUseGaiaAuthFetcherIOSForTesting(
    bool use_gaia_fetcher_ios) {
  g_should_use_gaia_auth_fetcher_ios = use_gaia_fetcher_ios;
}

bool GaiaAuthFetcherIOS::ShouldUseGaiaAuthFetcherIOS() {
  return g_should_use_gaia_auth_fetcher_ios;
}
