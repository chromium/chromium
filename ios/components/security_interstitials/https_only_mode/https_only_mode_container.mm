// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"

#import "base/memory/ptr_util.h"
#import "ios/web/public/web_state.h"

WEB_STATE_USER_DATA_KEY_IMPL(HttpsOnlyModeContainer)

HttpsOnlyModeContainer::HttpsOnlyModeContainer(web::WebState* web_state) {}

HttpsOnlyModeContainer::HttpsOnlyModeContainer(HttpsOnlyModeContainer&& other) =
    default;

HttpsOnlyModeContainer& HttpsOnlyModeContainer::operator=(
    HttpsOnlyModeContainer&& other) = default;

HttpsOnlyModeContainer::~HttpsOnlyModeContainer() = default;

HttpsOnlyModeContainer::InterstitialParams::InterstitialParams() = default;

HttpsOnlyModeContainer::InterstitialParams::~InterstitialParams() = default;

HttpsOnlyModeContainer::InterstitialParams::InterstitialParams(
    const InterstitialParams& other) = default;

void HttpsOnlyModeContainer::RecordBlockingPageParams(
    const GURL& url,
    const web::Referrer& referrer,
    const std::vector<GURL>& redirect_chain) {
  interstitial_params_->url = url;
  interstitial_params_->referrer = referrer;
  interstitial_params_->redirect_chain = redirect_chain;
}

void HttpsOnlyModeContainer::SetHttpUrl(const GURL& http_url) {
  http_url_ = http_url;
}

GURL HttpsOnlyModeContainer::http_url() const {
  return http_url_;
}

std::unique_ptr<HttpsOnlyModeContainer::InterstitialParams>
HttpsOnlyModeContainer::ReleaseInterstitialParams() {
  return std::move(interstitial_params_);
}
