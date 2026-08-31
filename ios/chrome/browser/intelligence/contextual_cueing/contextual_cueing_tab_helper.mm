// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_tab_helper.h"

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
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace contextual_cueing {

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

  for (Observer& observer : observers_) {
    observer.OnContextualCueInvalidated(web_state_);
  }

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
    observer.OnPageClassificationCompleted(web_state_, categories_);
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
