// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#import "components/metrics/persistent_synthetic_trial_observer.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/ukm/observers/history_delete_observer.h"
#include "components/ukm/observers/ukm_consent_state_observer.h"
#include "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/browser/metrics/incognito_web_state_observer.h"
#import "ios/web/public/deprecated/global_web_state_observer.h"

class ChromeBrowserState;
class IOSChromeStabilityMetricsProvider;
class PrefRegistrySimple;

namespace metrics {
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

namespace ukm {
class UkmService;
}

// IOSChromeMetricsServiceClient provides an implementation of
// MetricsServiceClient that depends on //ios/chrome/.
class IOSChromeMetricsServiceClient : public IncognitoWebStateObserver,
                                      public metrics::MetricsServiceClient,
                                      public ukm::HistoryDeleteObserver,
                                      public ukm::UkmConsentStateObserver,
                                      public web::GlobalWebStateObserver {
 public:
  IOSChromeMetricsServiceClient(const IOSChromeMetricsServiceClient&) = delete;
  IOSChromeMetricsServiceClient& operator=(
      const IOSChromeMetricsServiceClient&) = delete;

  ~IOSChromeMetricsServiceClient() override;

  // Factory function.
  static std::unique_ptr<IOSChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmService* GetUkmService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  const network_time::NetworkTimeTracker* GetNetworkTimeTracker() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  bool IsExtendedStableChannel() override;
  std::string GetVersionString() override;
  void CollectFinalMetricsForLog(base::OnceClosure done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const GURL& server_url,
      const GURL& insecure_server_url,
      base::StringPiece mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  bool IsUkmAllowedForAllProfiles() override;
  bool AreNotificationListenersEnabledOnAllProfiles() override;
  std::string GetUploadSigningKey() override;

  // ukm::HistoryDeleteObserver:
  void OnHistoryDeleted() override;

  // ukm::UkmConsentStateObserver:
  void OnUkmAllowedStateChanged(bool must_purge, ukm::UkmConsentState) override;

  // web::GlobalWebStateObserver:
  void WebStateDidStartLoading(web::WebState* web_state) override;
  void WebStateDidStopLoading(web::WebState* web_state) override;

  // IncognitoWebStateObserver:
  void OnIncognitoWebStateAdded() override;
  void OnIncognitoWebStateRemoved() override;

  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;

  // Determine what to do with a file based on filename. Visible for testing.
  static metrics::FileMetricsProvider::FilterAction FilterBrowserMetricsFiles(
      const base::FilePath& path);

 private:
  explicit IOSChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager);

  // Completes the two-phase initialization of IOSChromeMetricsServiceClient.
  void Initialize();

  // Registers providers to the MetricsService. These provide data from
  // alternate sources.
  void RegisterMetricsServiceProviders();

  // Registers providers to the UkmService. These provide data from alternate
  // sources.
  void RegisterUKMProviders();

  // Callbacks for various stages of final log info collection. Do not call
  // these directly.
  void CollectFinalHistograms();

  // Registers `this` as an observer for notifications which indicate that a
  // user is performing work. This is useful to allow some features to sleep,
  // until the machine becomes active, such as precluding UMA uploads unless
  // there was recent activity.
  // Returns true if registration was successful.
  bool RegisterForNotifications();

  // Register to observe events on a browser state's services.
  // Returns true if registration was successful.
  bool RegisterForBrowserStateEvents(ChromeBrowserState* browser_state);

  // Called when a tab is parented.
  void OnTabParented(web::WebState* web_state);

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  base::ThreadChecker thread_checker_;

  // Weak pointer to the MetricsStateManager.
  metrics::MetricsStateManager* metrics_state_manager_;

  // The synthetic trial registry shared by metrics_service_ and ukm_service_.
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  // Metrics service observer for synthetic trials.
  metrics::PersistentSyntheticTrialObserver synthetic_trial_observer_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_observation_{&synthetic_trial_observer_};

  // The MetricsService that `this` is a client of.
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // The UkmService that `this` is a client of.
  std::unique_ptr<ukm::UkmService> ukm_service_;

  // Whether we registered all notification listeners successfully.
  bool notification_listeners_active_;

  // The IOSChromeStabilityMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as `metrics_service_`.
  IOSChromeStabilityMetricsProvider* stability_metrics_provider_;

  // Saved callback received from CollectFinalMetricsForLog().
  base::OnceClosure collect_final_metrics_done_callback_;

  // Subscription for receiving callbacks that a tab was parented.
  base::CallbackListSubscription tab_parented_subscription_;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  base::CallbackListSubscription omnibox_url_opened_subscription_;

  base::WeakPtrFactory<IOSChromeMetricsServiceClient> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_CLIENT_H_
