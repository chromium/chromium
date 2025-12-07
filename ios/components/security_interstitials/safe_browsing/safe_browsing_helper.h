// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_HELPER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;
class SafeBrowsingService;

namespace safe_browsing {
class SafeBrowsingMetricsCollector;
}  // namespace safe_browsing

// KeyedService used to notify SafeBrowsingService of BrowserState creation
// and destruction.
class SafeBrowsingHelper : public KeyedService {
 public:
  SafeBrowsingHelper(
      PrefService* pref_service,
      SafeBrowsingService* safe_browsing_service,
      safe_browsing::SafeBrowsingMetricsCollector* metrics_collector);

  // KeyedService implementation.
  void Shutdown() override;

 private:
  raw_ptr<PrefService> pref_service_;
  raw_ptr<SafeBrowsingService> safe_browsing_service_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_HELPER_H_
