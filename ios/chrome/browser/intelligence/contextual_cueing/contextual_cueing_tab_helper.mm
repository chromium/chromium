// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"

#import <algorithm>

#import "base/check.h"
#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/contextual_cueing/contextual_cueing_enums.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/signin/public/identity_manager/account_capabilities.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace contextual_cueing {

namespace {

constexpr size_t kMaxBackgroundTabs = 5;

}  // namespace

ContextualCueingTabHelper::ContextualCueingTabHelper(web::WebState* web_state)
    : web_state_(web_state),
      current_url_(web_state ? web_state->GetLastCommittedURL() : GURL()) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

ContextualCueingTabHelper::~ContextualCueingTabHelper() = default;

void ContextualCueingTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualCueingTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::optional<std::vector<page_content_annotations::Category>>&
ContextualCueingTabHelper::GetCategories() const {
  return categories_;
}

const std::optional<optimization_guide::proto::ContextualCue>&
ContextualCueingTabHelper::GetContextualCue() const {
  return cue_;
}

void ContextualCueingTabHelper::RecordCueShown() {
  RecordContextualCueingDecision(ContextualCueingDecision::kSuccess);
  ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
  if (!cap_service) {
    return;
  }
  cap_service->RecordCueShown(web_state_->GetLastCommittedURL());
}

void ContextualCueingTabHelper::RecordCueDismissed() {
  ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
  if (!cap_service) {
    return;
  }
  cap_service->RecordCueDismissed(web_state_->GetLastCommittedURL());
}

void ContextualCueingTabHelper::RecordCueClicked() {
  ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
  if (!cap_service) {
    return;
  }
  cap_service->RecordCueClicked(web_state_->GetLastCommittedURL());
}

#pragma mark - web::WebStateObserver

void ContextualCueingTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->HasCommitted()) {
    return;
  }

  const GURL& url = navigation_context->GetUrl();
  const GURL& new_url_without_ref = url.GetWithoutRef();
  if (new_url_without_ref == current_url_.GetWithoutRef()) {
    return;
  }
  current_url_ = url;

  if (url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
      !navigation_context->GetError()) {
    ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
    if (cap_service) {
      cap_service->RecordPageNavigation();
    }
  }

  CancelClassification();
  categories_.reset();

  if (navigation_context->IsSameDocument()) {
    StartClassification();
  }
}

void ContextualCueingTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    StartClassification();
  }
}

void ContextualCueingTabHelper::WasShown(web::WebState* web_state) {
  if (!categories_.has_value() && web_state_ && web_state_->IsVisible() &&
      !web_state_->IsLoading() &&
      web_state_->GetLastCommittedURL().is_valid()) {
    StartClassification();
  }
}

void ContextualCueingTabHelper::WasHidden(web::WebState* web_state) {
  CancelClassification();
}

void ContextualCueingTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CancelClassification();
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ContextualCueingTabHelper::CancelClassification() {
  if (!web_state_) {
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  bool had_cue = cue_.has_value();
  cue_.reset();
  log_entry_.reset();

  if (had_cue) {
    for (Observer& observer : observers_) {
      observer.OnContextualCueInvalidated(this);
    }
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  OnDevicePageClassificationService* service =
      OnDevicePageClassificationServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }
  service->CancelClassification(web_state_);
}

void ContextualCueingTabHelper::StartClassification() {
  if (!web_state_) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  if (profile->IsOffTheRecord()) {
    return;
  }

  if (!IsUserEligibleForGemini(profile)) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kTargetFeatureNotEligible);
    return;
  }

  if (!IsHistorySyncEnabled(profile)) {
    RecordContextualCueingDecision(ContextualCueingDecision::kHistorySyncOff);
    return;
  }

  const GURL& url = web_state_->GetLastCommittedURL();
  std::string mime_type = web_state_->GetContentsMimeType();
  if (mime_type.empty()) {
    mime_type = "text/html";
  }
  ContextualCueingEvaluator evaluator(GetCapTrackerService());
  ContextualCueingDecision page_decision =
      evaluator.EvaluatePageEligibility(url, mime_type);
  // Check if we are eligible to show a cue before classifying the page.
  if (page_decision != ContextualCueingDecision::kSuccess) {
    RecordContextualCueingDecision(page_decision);
    return;
  }

  // TODO(crbug.com/549660446): Add PageClassificationService via verticals
  // here.
  if (IsGeminiContextualSuggestionsCuesOnDeviceClassifierEnabled()) {
    OnDevicePageClassificationService* service =
        OnDevicePageClassificationServiceFactory::GetForProfile(profile);
    if (!service) {
      return;
    }

    service->ClassifyWebState(
        web_state_, base::BindOnce(&ContextualCueingTabHelper::OnPageClassified,
                                   weak_ptr_factory_.GetWeakPtr(), url));
  }
}

void ContextualCueingTabHelper::OnPageClassified(
    const GURL& expected_url,
    const std::optional<std::vector<page_content_annotations::Category>>&
        categories) {
  if (!web_state_ || web_state_->GetLastCommittedURL() != expected_url) {
    return;
  }

  categories_ = categories;

  for (Observer& observer : observers_) {
    observer.OnPageClassificationCompleted(this, categories_);
  }

  if (!categories.has_value()) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kFailedCategoryClassification);
    return;
  }

  std::string mime_type = web_state_->GetContentsMimeType();
  if (mime_type.empty()) {
    mime_type = "text/html";
  }
  ContextualCueingEvaluator evaluator(GetCapTrackerService());
  ContextualCueingEvaluator::EvaluationResult evaluation_result =
      evaluator.Evaluate(expected_url, *categories, mime_type);
  if (!evaluation_result.is_eligible()) {
    RecordContextualCueingDecision(evaluation_result.decision);
    return;
  }

  if (IsGeminiContextualSuggestionsCuesServerModelExecutionEnabled()) {
    InitiateModelExecutionRequest(expected_url);
  }
}

void ContextualCueingTabHelper::InitiateModelExecutionRequest(
    const GURL& expected_url) {
  if (!web_state_ || web_state_->GetLastCommittedURL() != expected_url) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  OptimizationGuideService* service =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!service) {
    return;
  }

  optimization_guide::proto::ContextualCueingRequest request;
  request.mutable_active_tab_page_context()->set_url(expected_url.spec());
  request.mutable_active_tab_page_context()->set_title(
      base::UTF16ToUTF8(web_state_->GetTitle()));

  if (delegate_) {
    base::flat_set<GURL> seen_urls;
    seen_urls.insert(expected_url.GetWithoutRef());

    std::vector<BackgroundTabContext> bg_contexts =
        delegate_->GetEligibleBackgroundTabs(web_state_, kMaxBackgroundTabs);
    for (const auto& bg_context : bg_contexts) {
      if (!bg_context.url.is_valid() ||
          !seen_urls.insert(bg_context.url.GetWithoutRef()).second) {
        continue;
      }
      auto* tab_context = request.add_background_tabs();
      tab_context->set_url(bg_context.url.spec());
      tab_context->set_title(bg_context.title);
    }
  }

  service->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, request,
      /*options=*/{},
      base::BindOnce(
          &ContextualCueingTabHelper::OnModelExecutionResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), expected_url));
}

void ContextualCueingTabHelper::OnModelExecutionResponseReceived(
    const GURL& expected_url,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!web_state_ || web_state_->GetLastCommittedURL() != expected_url) {
    return;
  }

  log_entry_ = std::move(log_entry);

  if (!result.response.has_value()) {
    NotifyContextualCueReceived(std::nullopt);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::ContextualCueingResponse>(
      result.response.value());
  if (!response || response->contextual_cues_size() == 0) {
    NotifyContextualCueReceived(std::nullopt);
    return;
  }

  const optimization_guide::proto::ContextualCue& cue =
      response->contextual_cues(0);
  // The proto defines `fulfillment_surface` as a `oneof` to support different
  // surfaces (and future additions). Validate that the surface is populated
  // and set to Gemini in Chrome (GiC), as it is currently the only fulfillment
  // surface supported on iOS.
  if (cue.fulfillment_surface_case() !=
          optimization_guide::proto::ContextualCue::kGeminiInChromeSurface ||
      !cue.has_gemini_in_chrome_surface()) {
    NotifyContextualCueReceived(std::nullopt);
    return;
  }

  ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
  if (cap_service && cap_service->CanShowNudge(expected_url) !=
                         ContextualCueingDecision::kSuccess) {
    NotifyContextualCueReceived(std::nullopt);
    return;
  }

  NotifyContextualCueReceived(cue);
}

void ContextualCueingTabHelper::NotifyContextualCueReceived(
    std::optional<optimization_guide::proto::ContextualCue> cue) {
  cue_ = std::move(cue);
  for (Observer& observer : observers_) {
    observer.OnContextualCueReceived(this, cue_);
  }
}

bool ContextualCueingTabHelper::IsHistorySyncEnabled(ProfileIOS* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return false;
  }
  return sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory);
}

bool ContextualCueingTabHelper::IsUserEligibleForGemini(ProfileIOS* profile) {
  GeminiService* gemini_service = GeminiServiceFactory::GetForProfile(profile);
  return gemini_service && gemini_service->IsProfileEligibleForGemini();
}

ContextualCueingCapTrackerService*
ContextualCueingTabHelper::GetCapTrackerService() const {
  if (!web_state_) {
    return nullptr;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if (!profile) {
    return nullptr;
  }
  return ContextualCueingCapTrackerServiceFactory::GetForProfile(profile);
}

}  // namespace contextual_cueing
