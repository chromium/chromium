// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_DEMOGRAPHICS_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_DEMOGRAPHICS_CLIENT_H_

#include "components/metrics/demographics/demographic_metrics_provider.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace metrics {

// Client used by DemographicMetricsProvider to retrieve Profile information.
class DemographicsClient
    : public metrics::DemographicMetricsProvider::ProfileClient {
 public:
  DemographicsClient(const DemographicsClient&) = delete;
  DemographicsClient& operator=(const DemographicsClient&) = delete;

  DemographicsClient() = default;
  ~DemographicsClient() override = default;

  // DemographicMetricsProvider::ProfileClient:
  int GetNumberOfProfilesOnDisk() override;
  syncer::SyncService* GetSyncService() override;
  PrefService* GetLocalState() override;
  PrefService* GetProfilePrefs() override;
  base::Time GetNetworkTime() const override;

 private:
  // Returns the browser state for which metrics will be gathered. Once a
  // suitable browser state has been found, future calls will continue to return
  // the same value so that reported metrics are consistent (unless that browser
  // state becomes invalid).
  ChromeBrowserState* GetCachedBrowserState();
  raw_ptr<ChromeBrowserState> chrome_browser_state_ = nullptr;
};

}  // namespace metrics

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_DEMOGRAPHICS_CLIENT_H_
