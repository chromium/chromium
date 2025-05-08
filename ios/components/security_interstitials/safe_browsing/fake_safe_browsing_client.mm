// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"

#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/web/public/web_state.h"

FakeSafeBrowsingClient::FakeSafeBrowsingClient(PrefService* pref_service)
    : safe_browsing_service_(base::MakeRefCounted<FakeSafeBrowsingService>()),
      pref_service_(pref_service) {}

FakeSafeBrowsingClient::~FakeSafeBrowsingClient() = default;

base::WeakPtr<SafeBrowsingClient> FakeSafeBrowsingClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

PrefService* FakeSafeBrowsingClient::GetPrefs() {
  return pref_service_;
}

SafeBrowsingService* FakeSafeBrowsingClient::GetSafeBrowsingService() {
  return safe_browsing_service_.get();
}

safe_browsing::RealTimeUrlLookupServiceBase*
FakeSafeBrowsingClient::GetRealTimeUrlLookupService() {
  return lookup_service_;
}

safe_browsing::HashRealTimeService*
FakeSafeBrowsingClient::GetHashRealTimeService() {
  return nullptr;
}

variations::VariationsService* FakeSafeBrowsingClient::GetVariationsService() {
  return nullptr;
}

bool FakeSafeBrowsingClient::ShouldBlockUnsafeResource(
    const security_interstitials::UnsafeResource& resource) const {
  return should_block_unsafe_resource_;
}

bool FakeSafeBrowsingClient::OnMainFrameUrlQueryCancellationDecided(
    web::WebState* web_state,
    const GURL& url) {
  main_frame_cancellation_decided_called_ = true;
  return main_frame_cancellation_decided_called_;
}

bool FakeSafeBrowsingClient::ShouldForceSyncRealTimeUrlChecks() const {
  return should_force_sync_real_time_url_checks_;
}
