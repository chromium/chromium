// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_host_ios.h"

#import <UIKit/UIKit.h>

#import <algorithm>

#import "base/check.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/rand_util.h"
#import "base/strings/strcat.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/browser/db/allowlist_checker_client.h"
#import "components/safe_browsing/core/browser/db/database_manager.h"
#import "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#import "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/phishing_classifier/phishing_classifier.h"
#import "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"
#import "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/core/common/safebrowsing_switches.h"
#import "components/safe_browsing/core/common/threat_enums.h"
#import "components/safe_browsing/core/common/visual_utils.h"
#import "components/safe_browsing/ios/browser/client_side_detection_feature_cache.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"
#import "ios/chrome/browser/safe_browsing/model/user_population_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/ip_address.h"
#import "net/base/url_util.h"
#import "ui/display/screen.h"
#import "ui/gfx/image/image.h"
#import "url/url_constants.h"

namespace safe_browsing {

namespace {
// Delay before initiating snapshot and classification to allow page to settle.
constexpr base::TimeDelta kStabilizationDelay = base::Milliseconds(750);

// Whether local resource / localhost checks should be bypassed for testing.
bool g_bypass_local_resource_check_for_testing = false;

// Matches enum in tools/metrics/histograms/metadata/sb_client/enums.xml.
enum class ClientSideAllowlistMatchResult {
  kNoMatch = 0,
  kCsdMatch = 1,
  kHighConfidenceMatch = 2,
  kCsdAndHighConfidenceMatch = 3,
  kMaxValue = kCsdAndHighConfidenceMatch,
};

ClientSideAllowlistMatchResult GetClientSideAllowlistMatchResult(
    bool match_csd_allowlist,
    bool match_hc_allowlist) {
  if (match_csd_allowlist && match_hc_allowlist) {
    return ClientSideAllowlistMatchResult::kCsdAndHighConfidenceMatch;
  } else if (match_csd_allowlist) {
    return ClientSideAllowlistMatchResult::kCsdMatch;
  } else if (match_hc_allowlist) {
    return ClientSideAllowlistMatchResult::kHighConfidenceMatch;
  } else {
    return ClientSideAllowlistMatchResult::kNoMatch;
  }
}

PhishingDetectorResult GetPhishingDetectorResult(
    PhishingClassifier::Result result) {
  switch (result) {
    case PhishingClassifier::Result::kSuccess:
      return PhishingDetectorResult::CLASSIFICATION_SUCCESS;
    case PhishingClassifier::Result::kInvalidScore:
      return PhishingDetectorResult::INVALID_SCORE;
    case PhishingClassifier::Result::kInvalidURLFormatRequest:
      return PhishingDetectorResult::INVALID_URL_FORMAT_REQUEST;
    case PhishingClassifier::Result::kInvalidDocumentLoader:
      return PhishingDetectorResult::INVALID_DOCUMENT_LOADER;
    case PhishingClassifier::Result::kURLFeatureExtractionFailed:
      return PhishingDetectorResult::URL_FEATURE_EXTRACTION_FAILED;
    case PhishingClassifier::Result::kDOMExtractionFailed:
      return PhishingDetectorResult::DOM_EXTRACTION_FAILED;
    case PhishingClassifier::Result::kTermExtractionFailed:
      return PhishingDetectorResult::TERM_EXTRACTION_FAILED;
    case PhishingClassifier::Result::kVisualExtractionFailed:
      return PhishingDetectorResult::VISUAL_EXTRACTION_FAILED;
  }
}

// Returns true if the CSD allowlist check should be bypassed based on for
// `request_type` or command-line switch state.
bool ShouldSkipCSDAllowlist(ClientSideDetectionType request_type) {
  // If we get a suspicious verdict from RTLookupResponse, we should get a
  // second opinion on CSD side, so we skip the allowlist. If we get an
  // explicit request to send a report from the user, we skip the allowlist.
  // We also check the command line flag if the allowlist should be skipped.
  return request_type == ClientSideDetectionType::FORCE_REQUEST ||
         request_type == ClientSideDetectionType::USER_REPORT ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSkipCSDAllowlistOnPreclassification);
}

}  // namespace

#pragma mark - Public

ClientSideDetectionHostIOS::ClientSideDetectionHostIOS(
    web::WebState* web_state,
    ClientSideDetectionService* service,
    VerdictCacheManager* cache_manager,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    history::HistoryService* history_service)
    : ClientSideDetectionHostBase(
          service ? service->GetWeakPtr() : nullptr,
          cache_manager,
          /*intelligent_scan_delegate=*/nullptr,
          pref_service,
          identity_manager
              ? std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
                    identity_manager)
              : nullptr,
          history_service,
          web_state ? web_state->GetBrowserState()->IsOffTheRecord() : false),
      web_state_(web_state),
      service_(service),
      identity_manager_(identity_manager) {
  CHECK(web_state_);
  web_state_->AddObserver(this);
  EnsureObservingQueryManager();
  classifier_ = std::make_unique<safe_browsing::PhishingClassifier>();
  image_embedder_ = std::make_unique<safe_browsing::PhishingImageEmbedder>();
}

ClientSideDetectionHostIOS::~ClientSideDetectionHostIOS() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
  }
  CancelPendingRequests();
}

#pragma mark - ClientSideDetectionHostBase

GURL ClientSideDetectionHostIOS::GetCurrentUrl() const {
  return web_state_ ? web_state_->GetLastCommittedURL() : GURL();
}

ClientSideDetectionFeatureCacheBase*
ClientSideDetectionHostIOS::GetFeatureCache() {
  if (!web_state_) {
    return nullptr;
  }

  // This does nothing if the cache already exists.
  ClientSideDetectionFeatureCache::CreateForWebState(web_state_);
  return ClientSideDetectionFeatureCache::FromWebState(web_state_);
}

std::vector<GURL> ClientSideDetectionHostIOS::GetRedirectChain() {
  // TODO(crbug.com/502615476): Hook into SafeBrowsingTabHelper to leverage its
  // existing redirect chain extraction.
  return std::vector<GURL>();
}

credit_card_form::ReferringApp ClientSideDetectionHostIOS::GetReferringApp()
    const {
  return credit_card_form::kNoReferringApp;
}

ChromeUserPopulation ClientSideDetectionHostIOS::GetUserPopulation() {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  return GetUserPopulationForProfile(profile);
}

bool ClientSideDetectionHostIOS::IsAccountSignedIn() {
  return identity_manager_ &&
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

bool ClientSideDetectionHostIOS::IsErrorDocument() {
  return is_error_page_ || web_state_->IsCrashed();
}

void ClientSideDetectionHostIOS::GetInnerText(HostInnerTextCallback callback) {
  // TODO(crbug.com/502615476): Implement inner text extraction on iOS.
  std::move(callback).Run("");
}

void ClientSideDetectionHostIOS::ClassifyPhishingThroughThresholds(
    ClientPhishingRequest* verdict) {
  DCHECK(service_);
  service_->ClassifyPhishingThroughThresholds(verdict);
}

void ClientSideDetectionHostIOS::MaybeStartImageEmbedding(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    safe_browsing::PhishingDetectorResult result) {
  bool should_run_image_embedding =
      IsEnhancedProtectionEnabled() && service_ &&
      service_->HasImageEmbeddingModel() &&
      service_->IsModelMetadataImageEmbeddingVersionMatching() &&
      !verdict->has_image_feature_embedding();

  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result = DetermineVisualFeaturesExtraction();

  // Clear the blurred image from the visual features if we should not extract
  // visual features.
  if (can_extract_visual_features_result !=
      visual_utils::CanExtractVisualFeaturesResult::kCanExtractVisualFeatures) {
    if (verdict->has_visual_features()) {
      verdict->mutable_visual_features()->clear_image();
    }
  } else {
    base::UmaHistogramBoolean("SBClientPhishing.HasVisualFeaturesImage2",
                              verdict->has_visual_features() &&
                                  verdict->visual_features().has_image());
  }

  if (should_run_image_embedding && !classification_image_.IsEmpty()) {
    bool can_extract_visual_features =
        result ==
            safe_browsing::PhishingDetectorResult::CLASSIFICATION_SKIPPED ||
        can_extract_visual_features_result ==
            visual_utils::CanExtractVisualFeaturesResult::
                kCanExtractVisualFeatures;
    LogClientSideDetectionEvent(ClientSideDetectionEvent::kImageEmbeddingBegin,
                                verdict->client_side_detection_type());
    set_image_embedding_start_time(tick_clock()->NowTicks());
    image_embedder_->BeginImageEmbedding(
        classification_image_, can_extract_visual_features,
        base::BindOnce(&ClientSideDetectionHostIOS::OnImageEmbeddingDone,
                       weak_ptr_factory_.GetWeakPtr(), std::move(verdict),
                       did_match_high_confidence_allowlist, is_invalid_ip));
    // Release the snapshot image to minimize memory usage now that embedder
    // has received the frame.
    classification_image_ = gfx::Image();
    return;
  }

  // Image embedding has been skipped. Release the snapshot image to minimize
  // memory usage.
  classification_image_ = gfx::Image();
  MaybeStartIntelligentScanForScamDetection(
      std::move(verdict), did_match_high_confidence_allowlist, is_invalid_ip);
}

void ClientSideDetectionHostIOS::MaybeRunUserReportCallback() {
  // TODO(crbug.com/502615476): Implement user report callback.
}

void ClientSideDetectionHostIOS::MaybeStartGeminiAntiscamProtection(
    GURL url,
    ClientSideDetectionType request_type,
    std::optional<bool> did_match_high_confidence_allowlist) {
  // No-op on iOS for now.
}

void ClientSideDetectionHostIOS::MaybeStartPreClassification(
    ClientSideDetectionType request_type) {
  if (!web_state_) {
    return;
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionKillswitch)) {
    MaybeRunUserReportCallback();
    return;
  }

  if (request_type != ClientSideDetectionType::USER_REPORT &&
      !IsEnhancedProtectionEnabled() &&
      base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification)) {
    return;
  }

  if (is_preclassifying_ || is_classifying() || is_csd_running()) {
    if (base::FeatureList::IsEnabled(kClientSideDetectionTierSystem)) {
      if (!NewRequestTypeTierHigher(request_type)) {
        base::UmaHistogramExactLinear(
            base::StrCat({"SBClientPhishing.BlockingRequestType.",
                          GetRequestTypeName(request_type)}),
            last_request_type(), ClientSideDetectionType_MAX + 1);
        return;
      }
    }
    CancelPendingRequests();
  }

  if (!service_) {
    if (request_type == ClientSideDetectionType::USER_REPORT) {
      MaybeRunUserReportCallback();
    }
    return;
  }

  is_preclassifying_ = true;
  set_last_request_type(request_type);
  set_last_committed_url(request_type, web_state_->GetLastCommittedURL());

  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kTriggerStartsPreClassification, request_type);

  MaybeStartClassification(web_state_->GetLastCommittedURL());
}

void ClientSideDetectionHostIOS::CancelPendingRequests() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (classifier_ && classifier_->is_ready()) {
    classifier_->CancelPendingClassification();
  }
  if (image_embedder_ && image_embedder_->is_ready()) {
    image_embedder_->CancelPendingImageEmbedding();
  }
  is_preclassifying_ = false;
  set_is_classifying(false);
  set_is_csd_running(false);
  classification_image_ = gfx::Image();
  ClientSideDetectionHostBase::CancelPendingRequests();
}

void ClientSideDetectionHostIOS::ShowBlockingPage(
    GURL phishing_url,
    safe_browsing::ClientSideDetectionType request_type,
    std::optional<safe_browsing::IntelligentScanVerdict>
        intelligent_scan_verdict,
    bool should_show_scam_warning) {
  if (!web_state_ || !web_state_->GetNavigationManager() ||
      !kCsdEnforceIos.Get()) {
    return;
  }

  LogClientSideDetectionEvent(ClientSideDetectionEvent::kWarningShown,
                              request_type);

  security_interstitials::UnsafeResource resource;
  resource.url = phishing_url;
  resource.original_url = phishing_url;
  resource.navigation_url = phishing_url;
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING;
  resource.threat_source = safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION;
  resource.weak_web_state = web_state_->GetWeakPtr();
  // When we present a scam warning, we want to add separate interstitial
  // metrics to track specifics.
  if (should_show_scam_warning && intelligent_scan_verdict.has_value()) {
    resource.threat_subtype = GetThreatSubtype(*intelligent_scan_verdict);
    if (GetIntelligentScanDelegate()) {
      GetIntelligentScanDelegate()->OnScamWarningShown();
    }
  }

  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(web_state_);
  if (allow_list) {
    allow_list->AddPendingUnsafeNavigationDecision(resource.url,
                                                   resource.threat_type);
  }

  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state_);
  // The unsafe resource container should always be present. If it's not, then
  // the reload will not trigger an interstitial.
  DCHECK(container);
  container->StoreMainFrameUnsafeResource(resource);

  web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                             /*check_for_repost=*/false);
}

void ClientSideDetectionHostIOS::UpdateDebuggingMetadataWithNetworkResult(
    GURL phishing_url,
    net::HttpStatusCode response_code) {
  if (web_state_) {
    ClientSideDetectionFeatureCacheBase* feature_cache = GetFeatureCache();
    if (feature_cache) {
      LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
          feature_cache->GetOrCreateDebuggingMetadataForURL(phishing_url);
      if (debugging_metadata) {
        debugging_metadata->set_network_result(response_code);
      }
    }
  }
}

void ClientSideDetectionHostIOS::AddReferrerChain(
    safe_browsing::ClientPhishingRequest* verdict) {
  if (!verdict || !web_state_ || !web_state_->GetNavigationManager()) {
    return;
  }

  if (!IsEnhancedProtectionEnabled()) {
    return;
  }

  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  if (!item) {
    return;
  }

  safe_browsing::ReferrerChainEntry* entry = verdict->add_referrer_chain();
  entry->set_url(verdict->url());
  entry->set_type(safe_browsing::ReferrerChainEntry::EVENT_URL);

  if (!item->GetReferrer().url.is_empty()) {
    entry->set_referrer_url(item->GetReferrer().url.spec());
  }
}

#pragma mark - web::WebStateObserver

void ClientSideDetectionHostIOS::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument()) {
    if (is_csd_running()) {
      base::UmaHistogramExactLinear(
          "SBClientPhishing.ClientSideDetection.InterruptedByNavigation",
          last_request_type(), ClientSideDetectionType_MAX + 1);
    }
    CancelPendingRequests();
    set_is_csd_running(false);
    set_is_classifying(false);
    is_preclassifying_ = false;
    set_last_request_type(
        ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED);
    set_should_send_as_force_request(false);
    set_trigger_model_request_sent_as_force_request(false);
    clear_clipboard_extracted_data();
    classification_image_ = gfx::Image();

    MaybeRunUserReportCallback();

    set_current_url(navigation_context->GetUrl());
    is_page_loaded_ = false;
    is_fcp_received_ = false;
    is_error_page_ = navigation_context->GetError() != nullptr;
    did_match_high_confidence_allowlist_ = std::nullopt;
    send_sample_ping_ = false;
    stabilization_timer_.Stop();
    EnsureObservingMetricsHelper();
    EnsureObservingQueryManager();
  }
}

void ClientSideDetectionHostIOS::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    EnsureObservingMetricsHelper();
    is_page_loaded_ = true;
    MaybeTriggerClassification();
  }
}

void ClientSideDetectionHostIOS::WebStateDestroyed(web::WebState* web_state) {
  query_manager_observation_.Reset();
  metrics_helper_observation_.Reset();
  CancelPendingRequests();
  stabilization_timer_.Stop();
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

#pragma mark - SafeBrowsingQueryManager::Observer

void ClientSideDetectionHostIOS::SafeBrowsingAsyncQueryFinished(
    const SafeBrowsingQueryManager::QueryData& query_data) {
  // Ensure the completed query matches the current main-frame URL or is part of
  // its redirect chain before evaluating force-request verdicts.
  if (query_data.query->url != GetCurrentUrl() &&
      !std::ranges::contains(GetRedirectChain(), query_data.query->url)) {
    return;
  }
  OnAsyncSafeBrowsingCheckCompleted();
}

void ClientSideDetectionHostIOS::SafeBrowsingQueryManagerDestroyed(
    SafeBrowsingQueryManager* manager) {
  DCHECK(query_manager_observation_.IsObservingSource(manager));
  query_manager_observation_.Reset();
}

#pragma mark - Testing

void ClientSideDetectionHostIOS::
    OnVisualClassificationDoneForTesting(  // IN-TEST
        const GURL& url,
        const std::vector<double>& visual_scores) {
  stabilization_timer_.Stop();
  set_current_url(url);

  ClientSideDetectionType request_type = last_request_type();
  if (request_type ==
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED) {
    if (should_send_as_force_request() || HasForceRequestFromRtUrlLookup()) {
      request_type = ClientSideDetectionType::FORCE_REQUEST;
    } else {
      request_type = ClientSideDetectionType::TRIGGER_MODELS;
    }
    set_last_request_type(request_type);
  }

  ClientPhishingRequest verdict;
  verdict.set_url(url.spec());
  verdict.set_client_score(0.0);
  verdict.set_is_phishing(false);

  Scorer* scorer = service_ ? service_->GetScorer() : nullptr;

  for (size_t i = 0; i < visual_scores.size(); i++) {
    ClientPhishingRequest::CategoryScore* category =
        verdict.add_tflite_model_scores();
    if (scorer && i < static_cast<size_t>(scorer->tflite_thresholds().size())) {
      category->set_label(scorer->tflite_thresholds().at(i).label());
    } else {
      category->set_label("dummy_label");
    }
    category->set_value(visual_scores[i]);
  }

  OnClassificationDone(url, gfx::Image(), request_type,
                       /*classification_start_time=*/tick_clock()->NowTicks(),
                       verdict, PhishingClassifier::Result::kSuccess);
}

// static
void ClientSideDetectionHostIOS::
    SetBypassLocalResourceCheckForTesting(  // IN-TEST
        bool bypass) {
  g_bypass_local_resource_check_for_testing = bypass;
}

#pragma mark - WebPerformanceMetricsTabHelper::Observer

void ClientSideDetectionHostIOS::OnFirstContentfulPaint(
    WebPerformanceMetricsTabHelper* tab_helper,
    double first_contentful_paint) {
  is_fcp_received_ = true;
  MaybeTriggerClassification();
}

#pragma mark - Private

void ClientSideDetectionHostIOS::EnsureObservingMetricsHelper() {
  WebPerformanceMetricsTabHelper* metrics_helper =
      WebPerformanceMetricsTabHelper::FromWebState(web_state_);
  if (metrics_helper) {
    if (!metrics_helper_observation_.IsObserving()) {
      metrics_helper_observation_.Observe(metrics_helper);
    }

    double fcp = metrics_helper->GetAggregateAbsoluteFirstContentfulPaint();
    if (fcp != std::numeric_limits<double>::max()) {
      // FCP has already been received prior to starting observation or for
      // the current navigation.
      is_fcp_received_ = true;
    }
  } else {
    // If the performance metrics helper is not available (e.g. in unit
    // tests), skip the FCP gate and mark it as received immediately.
    is_fcp_received_ = true;
  }
}

void ClientSideDetectionHostIOS::EnsureObservingQueryManager() {
  if (!query_manager_observation_.IsObserving() && web_state_) {
    SafeBrowsingQueryManager* query_manager =
        SafeBrowsingQueryManager::FromWebState(web_state_);
    if (query_manager) {
      query_manager_observation_.Observe(query_manager);
    }
  }
}

void ClientSideDetectionHostIOS::MaybeTriggerClassification() {
  if (!is_page_loaded_ || !is_fcp_received_) {
    return;
  }

  if (!IsEnhancedProtectionEnabled() &&
      base::FeatureList::IsEnabled(kClientSideDetectionOnlyESBClassification)) {
    return;
  }

  if (stabilization_timer_.IsRunning()) {
    return;
  }

  stabilization_timer_.Start(
      FROM_HERE, kStabilizationDelay,
      base::BindOnce(
          &ClientSideDetectionHostIOS::TriggerClassificationAfterDelay,
          weak_ptr_factory_.GetWeakPtr()));
}

void ClientSideDetectionHostIOS::TriggerClassificationAfterDelay() {
  if (should_send_as_force_request() || HasForceRequestFromRtUrlLookup()) {
    base::UmaHistogramBoolean(
        "SBClientPhishing.TriggerModelsConvertedToForceRequestAtLoad", true);
    MaybeStartPreClassification(ClientSideDetectionType::FORCE_REQUEST);
  } else {
    MaybeStartPreClassification(ClientSideDetectionType::TRIGGER_MODELS);
  }
}

void ClientSideDetectionHostIOS::MaybeStartClassification(const GURL& url) {
  DCHECK(service_);

  // 1. Chrome UI Scheme.
  if (url.SchemeIs(kChromeUIScheme)) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_CHROME_UI_PAGE);
    return;
  }

  // 2. Local Resource / Localhost Guard.
  std::string_view host = url.host();
  if (!g_bypass_local_resource_check_for_testing) {
    if (base::FeatureList::IsEnabled(
            kClientSideDetectionLocalResourceCheckFix)) {
      if (url.SchemeIsFile() || net::IsLocalhost(url)) {
        RecordPreClassificationCheckResult(
            url, PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE);
        return;
      }
    } else {
      if (url.HostIsIPAddress()) {
        net::IPAddress address;
        if (address.AssignFromIPLiteral(host) && !address.IsValid()) {
          RecordPreClassificationCheckResult(
              url, PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE);
          return;
        }
      } else if (host == "localhost" ||
                 host.find('.') == std::string_view::npos) {
        // Intranet hostnames have no dots.
        RecordPreClassificationCheckResult(
            url, PreClassificationCheckResult::NO_CLASSIFY_LOCAL_RESOURCE);
        return;
      }
    }
  }

  // 3. Error Page / Document.
  std::string mime_type = web_state_->GetContentsMimeType();
  bool is_mime_type_unsupported =
      mime_type != "text/html" && mime_type != "application/xhtml+xml";
  bool is_error_page = IsErrorDocument();
  if (!is_mime_type_unsupported) {
    base::UmaHistogramBoolean(
        "SBClientPhishing.IsErrorDocumentOnSupportedMimeType", is_error_page);
  }
  if (base::FeatureList::IsEnabled(kClientSideDetectionSkipErrorPage) &&
      is_error_page) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_ERROR_DOCUMENT);
    return;
  }

  // 4. Unsupported MIME type.
  if (is_mime_type_unsupported) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_UNSUPPORTED_MIME_TYPE);
    return;
  }

  // 5. Private IP Address.
  if (!g_bypass_local_resource_check_for_testing && url.HostIsIPAddress()) {
    net::IPAddress address;
    if (address.AssignFromIPLiteral(host) &&
        service_->IsPrivateIPAddress(address)) {
      RecordPreClassificationCheckResult(
          url, PreClassificationCheckResult::NO_CLASSIFY_PRIVATE_IP);
      return;
    }
  }

  // 6. Scheme Not Supported (only HTTP/HTTPS are classified).
  if (!url.SchemeIsHTTPOrHTTPS()) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_SCHEME_NOT_SUPPORTED);
    return;
  }

  // 7. Off The Record (Incognito).
  if (web_state_->GetBrowserState()->IsOffTheRecord()) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_OFF_THE_RECORD);
    return;
  }

  // 8. Policy Allowlist.
  if (GetPrefs() && IsURLAllowlistedByPolicy(url, *GetPrefs())) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_ALLOWLISTED_BY_POLICY);
    return;
  }

  // 9. Safe Browsing database manager not available.
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager =
      GetApplicationContext()->GetSafeBrowsingService()
          ? GetApplicationContext()
                ->GetSafeBrowsingService()
                ->GetDatabaseManager()
          : nullptr;
  if (!database_manager) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_NO_DATABASE_MANAGER);
    return;
  }

  // 10. Query CSD Allowlist (Asynchronous Database Check).
  if (ShouldSkipCSDAllowlist(last_request_type())) {
    OnAllowlistCheckDone(url, /*match_allowlist=*/false);
    return;
  }

  base::OnceCallback<void(bool)> result_callback =
      base::BindOnce(&ClientSideDetectionHostIOS::OnAllowlistCheckDone,
                     weak_ptr_factory_.GetWeakPtr(), url);
  safe_browsing::AllowlistCheckerClient::StartCheckCsdAllowlist(
      database_manager, url, std::move(result_callback));
}

void ClientSideDetectionHostIOS::RecordPreClassificationCheckResult(
    const GURL& url,
    PreClassificationCheckResult reason) {
  is_preclassifying_ = false;

  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kPreClassificationCheckComplete,
      last_request_type());

  RecordPreClassificationCheckResultWithAndWithoutSuffix(reason,
                                                         last_request_type());
  if (IsEnhancedProtectionEnabled() && web_state_) {
    ClientSideDetectionFeatureCacheBase* feature_cache = GetFeatureCache();
    if (feature_cache) {
      LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
          feature_cache->GetOrCreateDebuggingMetadataForURL(url);
      if (debugging_metadata) {
        debugging_metadata->set_preclassification_check_result(reason);
      }
    }
  }
}

void ClientSideDetectionHostIOS::OnAllowlistCheckDone(const GURL& url,
                                                      bool match_allowlist) {
  send_sample_ping_ = CanSendSamplePing(last_request_type());

  switch (last_request_type()) {
    case ClientSideDetectionType::CREDIT_CARD_FORM:
    case ClientSideDetectionType::CLIPBOARD_COPY_API:
    case ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE:
      base::UmaHistogramBoolean(
          base::StrCat(
              {"SBClientPhishing.MatchCSDAllowlistOn",
               safe_browsing::GetRequestTypeName(last_request_type())}),
          match_allowlist);
      break;
    default:
      break;
  }

  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager =
      GetApplicationContext()->GetSafeBrowsingService()
          ? GetApplicationContext()
                ->GetSafeBrowsingService()
                ->GetDatabaseManager()
          : nullptr;
  if (!database_manager) {
    OnHighConfidenceAllowlistCheckDone(
        url, match_allowlist, tick_clock()->NowTicks(), false, std::nullopt);
    return;
  }

  base::OnceCallback<void(
      bool, std::optional<safe_browsing::SafeBrowsingDatabaseManager::
                              HighConfidenceAllowlistCheckLoggingDetails>)>
      hc_callback = base::BindOnce(
          &ClientSideDetectionHostIOS::OnHighConfidenceAllowlistCheckDone,
          weak_ptr_factory_.GetWeakPtr(), url, match_allowlist,
          tick_clock()->NowTicks());

  // TODO(crbug.com/551707094): High confidence allowlist checking should be
  // skipped if the CSD allowlist has a match.
  database_manager->CheckUrlForHighConfidenceAllowlist(url,
                                                       std::move(hc_callback));
}

void ClientSideDetectionHostIOS::OnHighConfidenceAllowlistCheckDone(
    const GURL& url,
    bool match_allowlist,
    base::TimeTicks check_start_time,
    bool url_on_high_confidence_allowlist,
    std::optional<safe_browsing::SafeBrowsingDatabaseManager::
                      HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  base::UmaHistogramMediumTimes(
      "SBClientPhishing.HighConfidenceAllowlistCheckDuration",
      tick_clock()->NowTicks() - check_start_time);

  ClientSideAllowlistMatchResult match_result =
      GetClientSideAllowlistMatchResult(match_allowlist && !send_sample_ping_,
                                        url_on_high_confidence_allowlist);

  base::UmaHistogramEnumeration("SBClientPhishing.MatchHighConfidenceAllowlist",
                                match_result);
  base::UmaHistogramEnumeration(
      base::StrCat({"SBClientPhishing.MatchHighConfidenceAllowlist.",
                    safe_browsing::GetRequestTypeName(last_request_type())}),
      match_result);

  PreClassificationCheckResult phishing_reason =
      PreClassificationCheckResult::NO_CLASSIFY_MAX;

  if (match_allowlist && !send_sample_ping_) {
    phishing_reason =
        PreClassificationCheckResult::NO_CLASSIFY_MATCH_CSD_ALLOWLIST;
  }

  if (phishing_reason == PreClassificationCheckResult::NO_CLASSIFY_MAX &&
      ShouldAcceptHCAllowlist(last_request_type(),
                              url_on_high_confidence_allowlist)) {
    phishing_reason =
        PreClassificationCheckResult::NO_CLASSIFY_MATCH_HC_ALLOWLIST;
  }

  did_match_high_confidence_allowlist_ = url_on_high_confidence_allowlist;

  ContinueClassificationAfterAllowlistChecks(url, phishing_reason);
}

void ClientSideDetectionHostIOS::ContinueClassificationAfterAllowlistChecks(
    const GURL& url,
    PreClassificationCheckResult phishing_reason) {
  if (phishing_reason != PreClassificationCheckResult::NO_CLASSIFY_MAX) {
    RecordPreClassificationCheckResult(url, phishing_reason);
    return;
  }

  if (ShouldStopAtPreClassification()) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_ALLOWLIST_METRIC);
    return;
  }

  // Cache Guard Check.
  bool is_phishing = false;
  if (last_request_type() == ClientSideDetectionType::TRIGGER_MODELS &&
      service_ && service_->GetValidCachedResult(url, &is_phishing)) {
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_RESULT_FROM_CACHE);

    // Directly invoke warning reload if cached as phishy
    MaybeShowPhishingWarning(
        /*is_from_cache=*/true, ClientSideDetectionType::TRIGGER_MODELS,
        did_match_high_confidence_allowlist_, url, is_phishing,
        /*response_code=*/std::nullopt,
        /*intelligent_scan_verdict=*/std::nullopt);
    return;
  }

  if (last_request_type() != ClientSideDetectionType::USER_REPORT && service_ &&
      service_->AtPhishingReportLimit()) {
    base::UmaHistogramExactLinear("SBClientPhishing.RequestTypeAtReportLimit",
                                  last_request_type(),
                                  ClientSideDetectionType_MAX + 1);
    RecordPreClassificationCheckResult(
        url, PreClassificationCheckResult::NO_CLASSIFY_TOO_MANY_REPORTS);
    return;
  }

  // Successful Pre-classification Gating Metric (Classify!)
  RecordPreClassificationCheckResult(url,
                                     PreClassificationCheckResult::CLASSIFY);

  set_is_csd_running(true);

  if (last_request_type() != ClientSideDetectionType::TRIGGER_MODELS &&
      base::FeatureList::IsEnabled(
          safe_browsing::
              kSkipImageClassificationScoringForNonPageLoadTriggers)) {
    ClientPhishingRequest verdict;
    verdict.set_url(url.spec());
    verdict.set_client_score(0.0);
    PhishingDetectionDone(
        last_request_type(), send_sample_ping_,
        did_match_high_confidence_allowlist_,
        /*is_invalid_ip=*/false, tick_clock()->NowTicks(),
        safe_browsing::PhishingDetectorResult::CLASSIFICATION_SKIPPED, verdict);
    return;
  }

  if (!service_->GetScorer()) {
    HandleVisualClassificationEarlyReturn(
        VisualClassificationEarlyReturnReason::kScorerMissingBeforeSnapshot);
    return;
  }

  SnapshotTabHelper* snapshot_helper =
      SnapshotTabHelper::FromWebState(web_state_);
  if (!snapshot_helper) {
    HandleVisualClassificationEarlyReturn(
        VisualClassificationEarlyReturnReason::kSnapshotHelperMissing);
    return;
  }

  snapshot_helper->GenerateSnapshotWithoutOverlaysWithCallback(
      base::CallbackToBlock(
          base::BindOnce(&ClientSideDetectionHostIOS::OnSnapshotReceived,
                         weak_ptr_factory_.GetWeakPtr(), url)));
}

bool ClientSideDetectionHostIOS::ShouldStopAtPreClassification() {
  switch (last_request_type()) {
    case ClientSideDetectionType::CLIPBOARD_COPY_API:
      return base::RandDouble() >=
             safe_browsing::kCsdClipboardCopyApiSampleRate.Get();
    case ClientSideDetectionType::CREDIT_CARD_FORM:
      return base::RandDouble() >=
             safe_browsing::kCsdCreditCardFormSampleRate.Get();
    case ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE:
      return base::RandDouble() >=
             safe_browsing::kCsdProactivePasswordProtectionSampleRate.Get();
    default:
      break;
  }
  return false;
}

void ClientSideDetectionHostIOS::HandleVisualClassificationEarlyReturn(
    VisualClassificationEarlyReturnReason reason) {
  set_is_csd_running(false);
  base::UmaHistogramEnumeration(
      "SBClientPhishing.iOS.VisualClassificationEarlyReturnReason", reason);
}

void ClientSideDetectionHostIOS::OnSnapshotReceived(const GURL& url,
                                                    UIImage* ui_image) {
  if (!ui_image) {
    HandleVisualClassificationEarlyReturn(
        VisualClassificationEarlyReturnReason::kSnapshotFailed);
    return;
  }

  if (!service_->GetScorer()) {
    HandleVisualClassificationEarlyReturn(
        VisualClassificationEarlyReturnReason::kScorerMissingAfterSnapshot);
    return;
  }

  gfx::Image input_image(ui_image);

  classifier_->set_scorer(service_->GetScorer());
  image_embedder_->set_scorer(service_->GetScorer());

  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kImageClassificationBegin, last_request_type());

  ClientSideDetectionType request_type = last_request_type();
  if (IsEnhancedProtectionEnabled() &&
      request_type == ClientSideDetectionType::TRIGGER_MODELS &&
      base::FeatureList::IsEnabled(kClientSideDetectionImageEmbeddingMatch)) {
    request_type = ClientSideDetectionType::IMAGE_EMBEDDING_MATCH;
  }

  set_is_classifying(true);
  classifier_->SetClientSideDetectionType(request_type);
  base::TimeTicks classification_start_time = tick_clock()->NowTicks();
  classifier_->BeginClassification(
      url, input_image,
      base::BindOnce(&ClientSideDetectionHostIOS::OnClassificationDone,
                     weak_ptr_factory_.GetWeakPtr(), url, input_image,
                     request_type, classification_start_time));
}

void ClientSideDetectionHostIOS::OnClassificationDone(
    const GURL& url,
    const gfx::Image& image,
    ClientSideDetectionType request_type,
    base::TimeTicks classification_start_time,
    const ClientPhishingRequest& verdict,
    PhishingClassifier::Result result) {
  if (result == PhishingClassifier::Result::kSuccess) {
    // Retain the snapshot image for subsequent image embedding
    // matching and reporting.
    classification_image_ = image;
  }

  PhishingDetectionDone(request_type, send_sample_ping_,
                        did_match_high_confidence_allowlist_,
                        /*is_invalid_ip=*/false, classification_start_time,
                        GetPhishingDetectorResult(result), verdict);
}

visual_utils::CanExtractVisualFeaturesResult
ClientSideDetectionHostIOS::DetermineVisualFeaturesExtraction() {
  UIView* view = web_state_->GetView();
  int viewport_width = -1;
  int viewport_height = -1;
  gfx::Size size;
  if (view) {
    viewport_width = static_cast<int>(view.bounds.size.width);
    viewport_height = static_cast<int>(view.bounds.size.height);
    size = gfx::Size(viewport_width, viewport_height);
  }

  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result =
          visual_utils::CanExtractVisualFeatures(
              IsEnhancedProtectionEnabled(),
              web_state_->GetBrowserState()->IsOffTheRecord(), size);

  base::UmaHistogramSparse("SBClientPhishing.Viewport.Width", viewport_width);
  base::UmaHistogramSparse("SBClientPhishing.Viewport.Height", viewport_height);

  float ppi = 0;
  if (view && display::Screen::Get()) {
    ppi = display::Screen::Get()
              ->GetDisplayNearestView(gfx::NativeView(view))
              .GetPixelsPerInchX();
  }
  base::UmaHistogramSparse("SBClientPhishing.Viewport.PixelsPerInch",
                           static_cast<int>(ppi));

  if (viewport_width <= 0xFFFF && viewport_width >= 0 &&
      viewport_height <= 0xFFFF && viewport_height >= 0) {
    int32_t encoded_resolution = (viewport_width << 16) | viewport_height;
    base::UmaHistogramSparse("SBClientPhishing.Viewport.EncodedResolution",
                             encoded_resolution);
  }

  base::UmaHistogramEnumeration("SBClientPhishing.VisualFeaturesClearReason2",
                                can_extract_visual_features_result);

  return can_extract_visual_features_result;
}

void ClientSideDetectionHostIOS::OnImageEmbeddingDone(
    std::unique_ptr<safe_browsing::ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    safe_browsing::PhishingImageEmbedder::Result result,
    const safe_browsing::ImageFeatureEmbedding& image_embedding,
    const safe_browsing::VisualFeatures& visual_features) {
  ClientSideDetectionHostBase::ImageEmbeddingResult translated_result;
  switch (result) {
    case safe_browsing::PhishingImageEmbedder::Result::kSuccess:
      translated_result =
          ClientSideDetectionHostBase::ImageEmbeddingResult::kSuccess;
      break;
    case safe_browsing::PhishingImageEmbedder::Result::kInvalidURLFormatRequest:
      translated_result = ClientSideDetectionHostBase::ImageEmbeddingResult::
          kInvalidURLFormatRequest;
      break;
    case safe_browsing::PhishingImageEmbedder::Result::kInvalidDocumentLoader:
      translated_result = ClientSideDetectionHostBase::ImageEmbeddingResult::
          kInvalidDocumentLoader;
      break;
    case safe_browsing::PhishingImageEmbedder::Result::kVisualExtractionFailed:
      translated_result =
          ClientSideDetectionHostBase::ImageEmbeddingResult::kFailed;
      break;
  }

  std::optional<safe_browsing::ImageFeatureEmbedding> embedding;
  std::optional<safe_browsing::VisualFeatures> visual;
  if (result == safe_browsing::PhishingImageEmbedder::Result::kSuccess) {
    embedding = image_embedding;
    visual = visual_features;
  }
  // The snapshot image is no longer needed. Release it to minimize memory
  // usage.
  classification_image_ = gfx::Image();
  PhishingImageEmbeddingDone(
      std::move(verdict), did_match_high_confidence_allowlist, is_invalid_ip,
      translated_result, std::move(embedding), std::move(visual));
}

}  // namespace safe_browsing
