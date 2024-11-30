// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/security_interstitials/safe_browsing/safe_browsing_helper.h"

#include "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"

SafeBrowsingHelper::SafeBrowsingHelper(
    PrefService* pref_service,
    SafeBrowsingService* safe_browsing_service,
    safe_browsing::SafeBrowsingMetricsCollector* metrics_collector)
    : pref_service_(pref_service),
      safe_browsing_service_(safe_browsing_service) {
  safe_browsing_service_->OnBrowserStateCreated(pref_service_,
                                                metrics_collector);
}

void SafeBrowsingHelper::Shutdown() {
  safe_browsing_service_->OnBrowserStateDestroyed(pref_service_);
}
