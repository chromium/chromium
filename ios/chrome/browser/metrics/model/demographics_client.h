// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_DEMOGRAPHICS_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_DEMOGRAPHICS_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "components/metrics/demographics/demographic_metrics_provider.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace metrics {

// Client used by DemographicMetricsProvider to retrieve Profile information.
class DemographicsClient
    : public metrics::DemographicMetricsProvider::ProfileClient {
 public:
  DemographicsClient();

  DemographicsClient(const DemographicsClient&) = delete;
  DemographicsClient& operator=(const DemographicsClient&) = delete;

  ~DemographicsClient() override;

  // DemographicMetricsProvider::ProfileClient:
  int GetNumberOfProfilesOnDisk() override;
  syncer::SyncService* GetSyncService() override;
  PrefService* GetLocalState() override;
  PrefService* GetProfilePrefs() override;
  base::Time GetNetworkTime() const override;

 private:
  // Returns the profile for which metrics will be gathered. Once a
  // suitable profile has been found, future calls will continue to return
  // the same value so that reported metrics are consistent (unless that browser
  // state becomes invalid).
  ProfileIOS* GetCachedProfile();

  // Weak pointer to the cached ProfileIOS.
  base::WeakPtr<ProfileIOS> profile_;
};

}  // namespace metrics

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_DEMOGRAPHICS_CLIENT_H_
