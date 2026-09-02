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
#import "components/safe_browsing/core/common/phishing_classifier/phishing_classifier.h"
#import "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"
#import "components/safe_browsing/core/common/visual_utils.h"
#import "ios/chrome/browser/web/model/web_performance_metrics/web_performance_metrics_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "net/http/http_status_code.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

class PrefService;
@class UIImage;

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

// Reasons why visual classification returned early on iOS.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(VisualClassificationEarlyReturnReason)
enum class VisualClassificationEarlyReturnReason {
  kScorerMissingBeforeSnapshot = 0,
  kSnapshotHelperMissing = 1,
  kSnapshotFailed = 2,
  kScorerMissingAfterSnapshot = 3,
  kMaxValue = kScorerMissingAfterSnapshot,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientPhishingVisualClassificationEarlyReturnReason)

// Host that manages client-side phishing detection for a WebState.
class ClientSideDetectionHostIOS
    : public ClientSideDetectionHostBase,
      public web::WebStateObserver,
      public SafeBrowsingQueryManager::Observer,
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
  // Note: Full multi-hop referrer chain tracking is not yet supported on iOS.
  // This provides a minimal fallback containing only the current event URL and
  // its immediate HTTP referrer.
  void AddReferrerChain(ClientPhishingRequest* verdict) override;

  // web::WebStateObserver implementation:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // `SafeBrowsingQueryManager::Observer` implementation:
  void SafeBrowsingAsyncQueryFinished(
      const SafeBrowsingQueryManager::QueryData& query_data) override;
  void SafeBrowsingQueryManagerDestroyed(
      SafeBrowsingQueryManager* manager) override;

  // Simulates visual classification completion for testing by bypassing UI
  // snapshotting and asynchronous ML inference (`PhishingClassifier`).
  // Synthesizes the provided `visual_scores` into a `ClientPhishingRequest`
  // verdict and feeds it directly into `OnClassificationDone()`. Cancels any
  // pending stabilization timer and resolves `last_request_type()` if
  // unspecified.
  void OnVisualClassificationDoneForTesting(
      const GURL& url,
      const std::vector<double>& visual_scores);

  // Sets whether the local resource / localhost pre-classification check should
  // be bypassed for testing.
  static void SetBypassLocalResourceCheckForTesting(bool bypass);

 private:
  friend class ClientSideDetectionHostIOSTest;

  // WebPerformanceMetricsTabHelper::Observer implementation:
  void OnFirstContentfulPaint(WebPerformanceMetricsTabHelper* tab_helper,
                              double first_contentful_paint) override;

  // Ensures observation of `WebPerformanceMetricsTabHelper` for FCP signals.
  void EnsureObservingMetricsHelper();

  // Ensures observation of `SafeBrowsingQueryManager` for real-time check
  // signals.
  void EnsureObservingQueryManager();

  // Checks if page load and FCP conditions are met to start stabilization
  // timer.
  void MaybeTriggerClassification();

  // Initiates model trigger pre-classification after stabilization delay.
  void TriggerClassificationAfterDelay();

  // Starts the pre-classification check and eventually classification.
  void MaybeStartClassification(const GURL& url);

  // Records the pre-classification check result to histograms and updates the
  // feature cache if Enhanced Protection is enabled. This serves a similar
  // purpose to content's
  // `ClientSideDetectionHost::ShouldClassifyUrlRequest::`
  // `DontClassifyForPhishing()`.
  // It differs from the base class's
  // `RecordPreClassificationCheckResultWithAndWithoutSuffix()` which only
  // handles histogram recording.
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

  // Handles visual classification early returns by resetting CSD running state
  // and recording the early return reason histogram.
  void HandleVisualClassificationEarlyReturn(
      VisualClassificationEarlyReturnReason reason);

  // Callback invoked when `SnapshotTabHelper` produces a snapshot of the page.
  void OnSnapshotReceived(const GURL& url, UIImage* ui_image);

  // Callback invoked when `PhishingClassifier` completes visual phishing
  // classification.
  void OnClassificationDone(const GURL& url,
                            const gfx::Image& image,
                            ClientSideDetectionType request_type,
                            base::TimeTicks classification_start_time,
                            const ClientPhishingRequest& verdict,
                            PhishingClassifier::Result result);

  // Returns whether visual features can be extracted from the current page.
  visual_utils::CanExtractVisualFeaturesResult
  DetermineVisualFeaturesExtraction();

  // Callback invoked when `PhishingImageEmbedder` completes image embedding.
  void OnImageEmbeddingDone(
      std::unique_ptr<safe_browsing::ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      safe_browsing::PhishingImageEmbedder::Result result,
      const safe_browsing::ImageFeatureEmbedding& image_embedding,
      const safe_browsing::VisualFeatures& visual_features);

  // Associated WebState.
  raw_ptr<web::WebState> web_state_ = nullptr;

  // Reference to the service.
  raw_ptr<ClientSideDetectionService> service_ = nullptr;

  // Reference to the identity manager.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  std::unique_ptr<PhishingClassifier> classifier_;
  std::unique_ptr<PhishingImageEmbedder> image_embedder_;

  // Cached page snapshot image associated with active visual classification.
  // Retained on classification success for downstream visual image embedding.
  gfx::Image classification_image_;

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

  base::ScopedObservation<SafeBrowsingQueryManager,
                          SafeBrowsingQueryManager::Observer>
      query_manager_observation_{this};

  base::WeakPtrFactory<ClientSideDetectionHostIOS> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_HOST_IOS_H_
