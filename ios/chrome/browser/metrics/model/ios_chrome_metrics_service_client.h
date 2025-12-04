// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICE_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICE_CLIENT_H_

#import <stdint.h>

#import <memory>
#import <optional>
#import <set>
#import <string>
#import <string_view>

#import "base/callback_list.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "components/metrics/file_metrics_provider.h"
#import "components/metrics/metrics_log_uploader.h"
#import "components/metrics/metrics_service_client.h"
#import "components/metrics/persistent_synthetic_trial_observer.h"
#import "components/omnibox/browser/omnibox_event_global_tracker.h"
#import "components/ukm/observers/history_delete_observer.h"
#import "components/ukm/observers/ukm_consent_state_observer.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

class IOSChromeStabilityMetricsProvider;
class PrefRegistrySimple;
class ProfileIOS;
class ProfileManagerIOS;

namespace regional_capabilities {
class CountryIdHolder;
}

namespace metrics {
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

namespace variations {
class SyntheticTrialRegistry;
}

namespace ukm {
class UkmService;
}

namespace metrics::dwa {
class DwaService;
}

namespace metrics::private_metrics {
class PumaService;
}

// IOSChromeMetricsServiceClient provides an implementation of
// MetricsServiceClient that depends on //ios/chrome/.
class IOSChromeMetricsServiceClient : public metrics::MetricsServiceClient,
                                      public ukm::HistoryDeleteObserver,
                                      public ukm::UkmConsentStateObserver,
                                      public ProfileManagerObserverIOS,
                                      public BrowserListObserver,
                                      public WebStateListObserver,
                                      public web::WebStateObserver {
 public:
  IOSChromeMetricsServiceClient(const IOSChromeMetricsServiceClient&) = delete;
  IOSChromeMetricsServiceClient& operator=(
      const IOSChromeMetricsServiceClient&) = delete;

  ~IOSChromeMetricsServiceClient() override;

  // Factory function.
  static std::unique_ptr<IOSChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsServiceClient:
  variations::SyntheticTrialRegistry* GetSyntheticTrialRegistry() override;
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmService* GetUkmService() override;
  metrics::dwa::DwaService* GetDwaService() override;
  metrics::private_metrics::PumaService* GetPumaService() override;
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
      std::string_view mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  bool IsUkmAllowedForAllProfiles() override;
  bool IsDwaAllowedForAllProfiles() override;
  bool AreNotificationListenersEnabledOnAllProfiles() override;
  std::string GetUploadSigningKey() override;
  bool ShouldStartUpFast() const override;
  std::optional<regional_capabilities::CountryIdHolder>
  GetProfileCountryIdForPrivateMetricsReporting() override;

  // ukm::HistoryDeleteObserver:
  void OnHistoryDeleted() override;

  // ukm::UkmConsentStateObserver:
  void OnUkmAllowedStateChanged(bool must_purge, ukm::UkmConsentState) override;

  // ProfileManagerObserverIOS:
  void OnProfileManagerWillBeDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileManagerDestroyed(ProfileManagerIOS* manager) override;
  void OnProfileCreated(ProfileManagerIOS* manager,
                        ProfileIOS* profile) override;
  void OnProfileLoaded(ProfileManagerIOS* manager,
                       ProfileIOS* profile) override;
  void OnProfileUnloaded(ProfileManagerIOS* manager,
                         ProfileIOS* profile) override;
  void OnProfileMarkedForPermanentDeletion(ProfileManagerIOS* manager,
                                           ProfileIOS* profile) override;

  // BrowserListObserver:
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnBrowserListShutdown(BrowserList* browser_list) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidStartLoading(web::WebState* web_state) override;
  void DidStopLoading(web::WebState* web_state) override;
  void RenderProcessGone(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;

  // Determine what to do with a file based on filename. Visible for testing.
  static metrics::FileMetricsProvider::FilterAction FilterBrowserMetricsFiles(
      const base::FilePath& path);

 private:
  explicit IOSChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager,
      variations::SyntheticTrialRegistry* synthetic_trial_registry);

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
  void RegisterForNotifications();

  // Register to observe events on a Profile's services.
  // Returns true if registration was successful.
  bool RegisterForProfileEvents(ProfileIOS* profile);

  // Called when a tab is parented.
  void OnTabParented(web::WebState* web_state);

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  // Starts/stops observation of `browser_list`.
  void StartObservingBrowserList(BrowserList* browser_list);
  void StopObservingBrowserList(BrowserList* browser_list);

  // Starts/stops observation of `browser`.
  void StartObservingBrowser(Browser* browser);
  void StopObservingBrowser(Browser* browser);

  // Starts/stops observation of `web_state`.
  void StartObservingWebState(web::WebState* web_state);
  void StopObservingWebState(web::WebState* web_state);

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer to the MetricsStateManager.
  raw_ptr<metrics::MetricsStateManager> metrics_state_manager_;

  // The synthetic trial registry shared by metrics_service_ and ukm_service_.
  raw_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;

  // Metrics service observer for synthetic trials.
  metrics::PersistentSyntheticTrialObserver synthetic_trial_observer_;
  base::ScopedObservation<variations::SyntheticTrialRegistry,
                          variations::SyntheticTrialObserver>
      synthetic_trial_observation_{&synthetic_trial_observer_};

  // The MetricsService that `this` is a client of.
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // The UkmService that `this` is a client of.
  std::unique_ptr<ukm::UkmService> ukm_service_;

  // The DwaService that `this` is a client of.
  std::unique_ptr<metrics::dwa::DwaService> dwa_service_;

  // The PumaService that `this` is a client of.
  std::unique_ptr<metrics::private_metrics::PumaService> puma_service_;

  // Observation of the ProfileManagerIOS.
  base::ScopedObservation<ProfileManagerIOS, ProfileManagerObserverIOS>
      profile_manager_observation_{this};

  // Observation of the BrowserLists.
  base::ScopedMultiSourceObservation<BrowserList, BrowserListObserver>
      browser_list_observations_{this};

  // Observation of the WebStateLists.
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observations_{this};

  // Observation of the WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  // Whether we registered all notification listeners successfully.
  bool notification_listeners_active_ = true;

  // The IOSChromeStabilityMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as `metrics_service_`.
  raw_ptr<IOSChromeStabilityMetricsProvider> stability_metrics_provider_;

  // Saved callback received from CollectFinalMetricsForLog().
  base::OnceClosure collect_final_metrics_done_callback_;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  base::CallbackListSubscription omnibox_url_opened_subscription_;

  // Subscription for receiving callbacks when the number of incognito tabs
  // open in the application transition from 0 to 1 or 1 to 0.
  base::CallbackListSubscription incognito_session_tracker_subscription_;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_IOS_CHROME_METRICS_SERVICE_CLIENT_H_
