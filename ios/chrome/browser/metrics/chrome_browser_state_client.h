// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_CHROME_BROWSER_STATE_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_CHROME_BROWSER_STATE_CLIENT_H_

#include "components/metrics/demographics/demographic_metrics_provider.h"

namespace metrics {

// Client used by DemographicMetricsProvider to retrieve Profile information.
class ChromeBrowserStateClient
    : public metrics::DemographicMetricsProvider::ProfileClient {
 public:
  ChromeBrowserStateClient(const ChromeBrowserStateClient&) = delete;
  ChromeBrowserStateClient& operator=(const ChromeBrowserStateClient&) = delete;

  ~ChromeBrowserStateClient() override;
  ChromeBrowserStateClient() = default;

  // DemographicMetricsProvider::ProfileClient:
  int GetNumberOfProfilesOnDisk() override;
  syncer::SyncService* GetSyncService() override;
  PrefService* GetLocalState() override;
  PrefService* GetProfilePrefs() override;
  base::Time GetNetworkTime() const override;
};

}  //    namespace metrics

#endif  // IOS_CHROME_BROWSER_METRICS_CHROME_BROWSER_STATE_CLIENT_H_
