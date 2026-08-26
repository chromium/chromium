// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_HOST_IOS_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_HOST_IOS_H_

#import <optional>
#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/safe_browsing/core/browser/client_side_detection_host_base.h"
#import "components/safe_browsing/core/browser/db/database_manager.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/web/public/web_state_observer.h"
#import "net/http/http_status_code.h"
#import "url/gurl.h"

class PrefService;

namespace history {
class HistoryService;
}  // namespace history

namespace signin {
class IdentityManager;
}  // namespace signin

namespace web {
class NavigationContext;
}  // namespace web

namespace safe_browsing {

class ClientSideDetectionService;
class ClientSideDetectionHostIOSTest;
class VerdictCacheManager;

// Host that manages client-side phishing detection for a WebState.
class ClientSideDetectionHostIOS
    : public ClientSideDetectionHostBase,
      public web::WebStateObserver,
      public WebPerformanceMetricsTabHelper::Observer {
 public:
  // Constructs a host instance managing client-side detection for `web_state`.
  // `service`, `cache_manager`, `pref_service`, `identity_manager`, and
  // `history_service` supply required service dependencies.
  ClientSideDetectionHostIOS(web::WebState* web_state,
                             ClientSideDetectionService* service,
                             VerdictCacheManager* cache_manager,
                             PrefService* pref_service,
                             signin::IdentityManager* identity_manager,
                             history::HistoryService* history_service);
  ~ClientSideDetectionHostIOS() override;

  ClientSideDetectionHostIOS(const ClientSideDetectionHostIOS&) = delete;
  ClientSideDetectionHostIOS& operator=(const ClientSideDetectionHostIOS&) =
      delete;

  // ClientSideDetectionHostBase overrides:
  GURL GetCurrentUrl() const override;
  ClientSideDetectionFeatureCacheBase* GetFeatureCache() override;
  std::vector<GURL> GetRedirectChain() override;
  credit_card_form::ReferringApp GetReferringApp() const override;
  ChromeUserPopulation GetUserPopulation() override;
  bool IsAccountSignedIn() override;
  bool IsErrorDocument() override;
  void GetInnerText(HostInnerTextCallback callback) override;
  void ClassifyPhishingThroughThresholds(
      ClientPhishingRequest* verdict) override;
  void MaybeStartImageEmbedding(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      PhishingDetectorResult result) override;
  void MaybeRunUserReportCallback() override;
  // TODO(crbug.com/502615476): Fully implement this method on iOS.
  void MaybeStartGeminiAntiscamProtection(
      GURL url,
      ClientSideDetectionType request_type,
      std::optional<bool> did_match_high_confidence_allowlist) override;
  void MaybeStartPreClassification(
      ClientSideDetectionType request_type) override;
  void CancelPendingRequests() override;
  void ShowBlockingPage(
      GURL phishing_url,
      ClientSideDetectionType request_type,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict,
      bool should_show_scam_warning) override;
  void UpdateDebuggingMetadataWithNetworkResult(
      GURL phishing_url,
      net::HttpStatusCode response_code) override;
  void AddReferrerChain(ClientPhishingRequest* verdict) override;

  // web::WebStateObserver implementation:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class ClientSideDetectionHostIOSTest;

  // WebPerformanceMetricsTabHelper::Observer implementation:
  void OnFirstContentfulPaint(WebPerformanceMetricsTabHelper* tab_helper,
                              double first_contentful_paint) override;

  // Ensures observation of `WebPerformanceMetricsTabHelper` for FCP signals.
  void EnsureObservingMetricsHelper();

  // Checks if page load and FCP conditions are met to start stabilization
  // timer.
  void MaybeTriggerClassification();

  // Initiates model trigger pre-classification after stabilization delay.
  void TriggerClassificationAfterDelay();

  // Starts the pre-classification check and eventually classification.
  void MaybeStartClassification(const GURL& url);

  // Records the pre-classification check result to histograms and updates the
  // feature cache if Enhanced Protection is enabled.
  void RecordPreClassificationCheckResult(const GURL& url,
                                          PreClassificationCheckResult reason);

  // Callback invoked when CSD allowlist lookup completes. Initiates
  // high-confidence allowlist check.
  void OnAllowlistCheckDone(const GURL& url, bool match_allowlist);

  // Callback invoked when High-Confidence allowlist lookup completes. Evaluates
  // allowlist results and logs duration metrics.
  void OnHighConfidenceAllowlistCheckDone(
      const GURL& url,
      bool match_allowlist,
      base::TimeTicks check_start_time,
      bool url_on_high_confidence_allowlist,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  // Evaluates sampling, cache hit, and report limit checks after allowlist
  // validation before triggering snapshot extraction and visual classification.
  void ContinueClassificationAfterAllowlistChecks(
      const GURL& url,
      PreClassificationCheckResult phishing_reason);

  // Returns whether pre-classification should stop based on the behavioral
  // trigger sampling rate.
  bool ShouldStopAtPreClassification();

  // Associated WebState.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Reference to the service.
  raw_ptr<ClientSideDetectionService> service_ = nullptr;

  // Reference to the identity manager.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  bool is_preclassifying_ = false;
  bool is_page_loaded_ = false;
  bool is_fcp_received_ = false;
  bool is_error_page_ = false;
  std::optional<bool> did_match_high_confidence_allowlist_;
  bool send_sample_ping_ = false;
  base::OneShotTimer stabilization_timer_;

  base::ScopedObservation<WebPerformanceMetricsTabHelper,
                          WebPerformanceMetricsTabHelper::Observer>
      metrics_helper_observation_{this};

  base::WeakPtrFactory<ClientSideDetectionHostIOS> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_HOST_IOS_H_
