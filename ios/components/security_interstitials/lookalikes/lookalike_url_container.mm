// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"

#import "base/memory/ptr_util.h"
#import "components/lookalikes/core/lookalike_url_util.h"
#import "ios/web/public/web_state.h"

WEB_STATE_USER_DATA_KEY_IMPL(LookalikeUrlContainer)

LookalikeUrlContainer::LookalikeUrlContainer(web::WebState* web_state) {}

LookalikeUrlContainer::LookalikeUrlContainer(LookalikeUrlContainer&& other) =
    default;

LookalikeUrlContainer& LookalikeUrlContainer::operator=(
    LookalikeUrlContainer&& other) = default;

LookalikeUrlContainer::~LookalikeUrlContainer() = default;

LookalikeUrlContainer::InterstitialParams::InterstitialParams() = default;

LookalikeUrlContainer::InterstitialParams::~InterstitialParams() = default;

LookalikeUrlContainer::InterstitialParams::InterstitialParams(
    const InterstitialParams& other) = default;

LookalikeUrlContainer::LookalikeUrlInfo::LookalikeUrlInfo(
    const GURL& safe_url,
    const GURL& request_url,
    lookalikes::LookalikeUrlMatchType match_type)
    : safe_url(safe_url), request_url(request_url), match_type(match_type) {}

LookalikeUrlContainer::LookalikeUrlInfo::~LookalikeUrlInfo() {}

LookalikeUrlContainer::LookalikeUrlInfo::LookalikeUrlInfo(
    const LookalikeUrlInfo& other) = default;

void LookalikeUrlContainer::RecordLookalikeBlockingPageParams(
    const GURL& url,
    const web::Referrer& referrer,
    const std::vector<GURL>& redirect_chain) {
  interstitial_params_->url = url;
  interstitial_params_->referrer = referrer;
  interstitial_params_->redirect_chain = redirect_chain;
}

void LookalikeUrlContainer::SetLookalikeUrlInfo(
    const GURL& safe_url,
    const GURL& request_url,
    lookalikes::LookalikeUrlMatchType match_type) {
  lookalike_info_ =
      std::make_unique<LookalikeUrlInfo>(safe_url, request_url, match_type);
}

std::unique_ptr<LookalikeUrlContainer::InterstitialParams>
LookalikeUrlContainer::ReleaseInterstitialParams() {
  return std::move(interstitial_params_);
}

std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo>
LookalikeUrlContainer::ReleaseLookalikeUrlInfo() {
  return std::move(lookalike_info_);
}
