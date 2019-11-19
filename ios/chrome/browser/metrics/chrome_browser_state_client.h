// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_CHROME_BROWSER_STATE_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_CHROME_BROWSER_STATE_CLIENT_H_

#include "base/macros.h"
#include "components/metrics/demographic_metrics_provider.h"

namespace metrics {

// Client used by DemographicMetricsProvider to retrieve Profile information.
class ChromeBrowserStateClient
    : public metrics::DemographicMetricsProvider::ProfileClient {
 public:
  ~ChromeBrowserStateClient() override;
  ChromeBrowserStateClient() = default;

  // DemographicMetricsProvider::ProfileClient:
  int GetNumberOfProfilesOnDisk() override;
  syncer::SyncService* GetSyncService() override;
  base::Time GetNetworkTime() const override;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserStateClient);
};

}  //    namespace metrics

#endif  // IOS_CHROME_BROWSER_METRICS_CHROME_BROWSER_STATE_CLIENT_H_
