// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "base/threading/thread_checker.h"
#include "components/metrics_services_manager/metrics_services_manager_client.h"

class PrefService;

namespace metrics {
class EnabledStateProvider;
class MetricsStateManager;
}

// Provides an //ios/chrome-specific implementation of
// MetricsServicesManagerClient.
class IOSChromeMetricsServicesManagerClient
    : public metrics_services_manager::MetricsServicesManagerClient {
 public:
  explicit IOSChromeMetricsServicesManagerClient(PrefService* local_state);

  IOSChromeMetricsServicesManagerClient(
      const IOSChromeMetricsServicesManagerClient&) = delete;
  IOSChromeMetricsServicesManagerClient& operator=(
      const IOSChromeMetricsServicesManagerClient&) = delete;

  ~IOSChromeMetricsServicesManagerClient() override;

 private:
  // This is defined as a member class to get access to
  // IOSChromeMetricsServiceAccessor through
  // IOSChromeMetricsServicesManagerClient's friendship.
  class IOSChromeEnabledStateProvider;

  // metrics_services_manager::MetricsServicesManagerClient:
  std::unique_ptr<variations::VariationsService> CreateVariationsService()
      override;
  std::unique_ptr<metrics::MetricsServiceClient> CreateMetricsServiceClient()
      override;
  metrics::MetricsStateManager* GetMetricsStateManager() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool IsMetricsReportingEnabled() override;
  bool IsMetricsConsentGiven() override;
  bool IsOffTheRecordSessionActive() override;

  // Static helper for `IsOffTheRecordSessionActive()`, suitable for binding
  // into callbacks. `true` if any browser states have any incognito WebStates
  // in any Browser.
  static bool AreIncognitoTabsPresent();

  // MetricsStateManager which is passed as a parameter to service constructors.
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // EnabledStateProvider to communicate if the client has consented to metrics
  // reporting, and if it's enabled.
  std::unique_ptr<metrics::EnabledStateProvider> enabled_state_provider_;

  // Ensures that all functions are called from the same thread.
  base::ThreadChecker thread_checker_;

  // Weak pointer to the local state prefs store.
  PrefService* local_state_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICES_MANAGER_CLIENT_H_
