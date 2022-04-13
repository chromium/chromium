// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/fake_safe_browsing_client.h"

#import "ios/chrome/browser/safe_browsing/fake_safe_browsing_service.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FakeSafeBrowsingClient::FakeSafeBrowsingClient()
    : safe_browsing_service_(base::MakeRefCounted<FakeSafeBrowsingService>()) {}

FakeSafeBrowsingClient::~FakeSafeBrowsingClient() = default;

SafeBrowsingService* FakeSafeBrowsingClient::GetSafeBrowsingService() {
  return safe_browsing_service_.get();
}

safe_browsing::RealTimeUrlLookupService*
FakeSafeBrowsingClient::GetRealTimeUrlLookupService() {
  return lookup_service_;
}

bool FakeSafeBrowsingClient::ShouldBlockUnsafeResource(
    const security_interstitials::UnsafeResource& resource) const {
  return should_block_unsafe_resource_;
}
void FakeSafeBrowsingClient::OnMainFrameUrlQueryCancellationDecided(
    web::WebState* web_state,
    const GURL& url) const {
  // No op.
}

bool FakeSafeBrowsingClient::OnSubFrameUrlQueryCancellationDecided(
    web::WebState* web_state,
    const GURL& url) const {
  return true;
}
