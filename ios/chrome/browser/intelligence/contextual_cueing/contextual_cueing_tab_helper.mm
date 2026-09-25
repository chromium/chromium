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
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/optimization_guide/core/model_execution/remote_model_executor.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_capabilities.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service_factory.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/features.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/on_device_page_classification_service_factory.h"
#import "ios/chrome/browser/intelligence/page_classification/features.h"
#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "ios/chrome/browser/intelligence/page_classification/page_classification_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
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

  if (IsGeminiContextualSuggestionsCuesEnabled()) {
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
    if (profile && !profile->IsOffTheRecord()) {
      PrefService* prefs = profile->GetPrefs();
      if (prefs) {
        pref_change_registrar_.Init(prefs);
        pref_change_registrar_.Add(
            prefs::kIOSGeminiSuggestionsSetting,
            base::BindRepeating(
                &ContextualCueingTabHelper::OnSuggestionsPreferenceChanged,
                base::Unretained(this)));
      }

      GeminiService* gemini_service =
          GeminiServiceFactory::GetForProfile(profile);
      if (gemini_service) {
        gemini_service_observation_.Observe(gemini_service);
      }
    }
  }
}

ContextualCueingTabHelper::~ContextualCueingTabHelper() {
  DismissFeatureEngagementPromo();
}

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

const std::optional<PageClassificationResult>&
ContextualCueingTabHelper::GetPageClassificationResult() const {
  return page_classification_result_;
}

const std::optional<optimization_guide::proto::ContextualCue>&
ContextualCueingTabHelper::GetContextualCue() const {
  return cue_;
}

std::optional<ContextualCueUiType>
ContextualCueingTabHelper::GetContextualCueUiType() const {
  return cue_ui_type_;
}

std::optional<page_content_annotations::CategoryType>
ContextualCueingTabHelper::GetActiveCategoryType() const {
  return active_category_type_;
}

bool ContextualCueingTabHelper::RecordCueShown() {
  CHECK(!fet_dismiss_runner_);
  feature_engagement::Tracker* tracker = GetFeatureEngagementTracker();
  if (tracker && !IsIgnoreContextualCueingThresholdsEnabled()) {
    if (!tracker->ShouldTriggerHelpUI(
            feature_engagement::kIPHiOSGeminiContextualCueChip)) {
      RecordContextualCueingDecision(
          ContextualCueingDecision::kTargetFeatureNotEligible);
      // FET owns promo arbitration; drop the cue so no surface keeps showing a
      // chip that FET has not authorized.
      InvalidateCue();
      return false;
    }
    fet_dismiss_runner_.ReplaceClosure(base::BindOnce(
        [](feature_engagement::Tracker* tracker) {
          tracker->Dismissed(
              feature_engagement::kIPHiOSGeminiContextualCueChip);
        },
        base::Unretained(tracker)));
  }

  RecordContextualCueingDecision(ContextualCueingDecision::kSuccess);
  ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
  if (cap_service && web_state_) {
    cap_service->RecordCueShown(web_state_->GetLastCommittedURL(),
                                active_category_type_);
  }
  return true;
}

void ContextualCueingTabHelper::RecordCueDismissed() {
  if (cue_ui_type_ == ContextualCueUiType::kMessage) {
    ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
    if (cap_service && web_state_) {
      cap_service->RecordCueDismissed(web_state_->GetLastCommittedURL(),
                                      active_category_type_);
    }
  }

  DismissFeatureEngagementPromo();
}

void ContextualCueingTabHelper::RecordCueClicked() {
  ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
  if (cap_service && web_state_) {
    cap_service->RecordCueClicked(web_state_->GetLastCommittedURL(),
                                  active_category_type_);
  }

  feature_engagement::Tracker* tracker = GetFeatureEngagementTracker();
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kIOSGeminiContextualCueChipUsed);
  }

  DismissFeatureEngagementPromo();
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

  if (!IsGeminiSuggestionsSettingEnabled()) {
    CancelClassification();
    categories_.reset();
    page_classification_result_.reset();
    return;
  }

  if (url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
      !navigation_context->GetError()) {
    ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
    if (cap_service) {
      cap_service->RecordPageNavigation();
    }
  }

  CancelClassification();
  categories_.reset();
  page_classification_result_.reset();

  if (navigation_context->IsSameDocument()) {
    StartClassification();
  }
}

void ContextualCueingTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (!IsGeminiSuggestionsSettingEnabled()) {
    return;
  }
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    StartClassification();
  }
}

void ContextualCueingTabHelper::WasShown(web::WebState* web_state) {
  if (!IsGeminiSuggestionsSettingEnabled()) {
    return;
  }
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
  pref_change_registrar_.Reset();
  gemini_service_observation_.Reset();
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

void ContextualCueingTabHelper::CancelClassification() {
  if (!web_state_) {
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  log_entry_.reset();
  is_model_execution_in_flight_ = false;

  DismissFeatureEngagementPromo();

  InvalidateCue();

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  OnDevicePageClassificationService* on_device_page_classification_service =
      OnDevicePageClassificationServiceFactory::GetForProfile(profile);
  if (on_device_page_classification_service) {
    on_device_page_classification_service->CancelClassification(web_state_);
  }

  PageClassificationService* page_classification_service =
      PageClassificationServiceFactory::GetForProfile(profile);
  if (page_classification_service) {
    page_classification_service->CancelClassification(web_state_);
  }
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

  CHECK(IsGeminiSuggestionsSettingEnabled());

  if (!IsIgnoreContextualCueingThresholdsEnabled()) {
    if (!IsUserEligibleForGemini(profile)) {
      RecordContextualCueingDecision(ContextualCueingDecision::kUserIneligible);
      return;
    }

    if (!IsHistorySyncEnabled(profile)) {
      RecordContextualCueingDecision(ContextualCueingDecision::kHistorySyncOff);
      return;
    }
  }

  const GURL& url = web_state_->GetLastCommittedURL();
  std::string mime_type = web_state_->GetContentsMimeType();
  if (mime_type.empty()) {
    mime_type = "text/html";
  }
  ContextualCueingEvaluator evaluator(GetCapTrackerService(),
                                      GetFeatureEngagementTracker());
  ContextualCueingDecision page_decision =
      evaluator.EvaluatePageEligibility(url, mime_type);
  // Check if we are eligible to show a cue before classifying the page.
  if (page_decision != ContextualCueingDecision::kSuccess) {
    RecordContextualCueingDecision(page_decision);
    return;
  }

  PageClassificationMode mode = IsIgnoreContextualCueingThresholdsEnabled()
                                    ? PageClassificationMode::kOnDeviceOnly
                                    : GetPageClassificationMode();

  if (mode == PageClassificationMode::kVerticalsOnly) {
    RequestPageClassificationService(url);
  } else {
    // Mode is kOnDeviceOnly or kOnDeviceWithVerticalsFallback.
    OnDevicePageClassificationService* on_device_page_classification_service =
        OnDevicePageClassificationServiceFactory::GetForProfile(profile);
    if (on_device_page_classification_service) {
      on_device_page_classification_service->ClassifyWebState(
          web_state_,
          base::BindOnce(&ContextualCueingTabHelper::OnPageClassified,
                         weak_ptr_factory_.GetWeakPtr(), url));
    } else if (mode == PageClassificationMode::kOnDeviceWithVerticalsFallback) {
      RequestPageClassificationService(url);
    }
  }
}

void ContextualCueingTabHelper::RequestPageClassificationService(
    const GURL& url) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  PageClassificationService* page_classification_service =
      PageClassificationServiceFactory::GetForProfile(profile);
  if (page_classification_service) {
    page_classification_service->ClassifyWebState(
        web_state_,
        base::BindOnce(&ContextualCueingTabHelper::
                           OnPageClassificationServiceResultReceived,
                       weak_ptr_factory_.GetWeakPtr(), url));
  }
}

// TODO(crbug.com/517561797): Unify OnDevicePageClassificationService under the
// PageClassificationService interface so both paths share the same result
// callback and types.
void ContextualCueingTabHelper::OnPageClassified(
    const GURL& expected_url,
    const std::optional<std::vector<page_content_annotations::Category>>&
        categories) {
  if (!web_state_ || web_state_->GetLastCommittedURL() != expected_url) {
    return;
  }

  // If OnDevice model was unavailable / returned std::nullopt and fallback is
  // enabled, trigger PageClassificationService.
  if (!categories.has_value() && !IsIgnoreContextualCueingThresholdsEnabled() &&
      GetPageClassificationMode() ==
          PageClassificationMode::kOnDeviceWithVerticalsFallback) {
    RequestPageClassificationService(expected_url);
    return;
  }

  ProcessClassificationResult(expected_url, categories);
}

void ContextualCueingTabHelper::OnPageClassificationServiceResultReceived(
    const GURL& expected_url,
    const PageClassificationResult& result) {
  if (!web_state_ || web_state_->GetLastCommittedURL() != expected_url) {
    return;
  }

  page_classification_result_ = result;

  // TODO(crbug.com/517561797): Remove this translation once
  // ContextualCueingEvaluator accepts PageClassificationResult directly.
  std::vector<page_content_annotations::Category> eligible_categories;
  for (const auto& category_result : result.category_results) {
    if (category_result.is_eligible) {
      eligible_categories.push_back(page_content_annotations::Category{
          .category_type = category_result.category_type,
          .score = category_result.score,
      });
    }
  }

  std::optional<std::vector<page_content_annotations::Category>> opt_categories;
  if (!eligible_categories.empty()) {
    opt_categories = std::move(eligible_categories);
  }

  ProcessClassificationResult(expected_url, opt_categories);
}

void ContextualCueingTabHelper::ProcessClassificationResult(
    const GURL& expected_url,
    const std::optional<std::vector<page_content_annotations::Category>>&
        categories) {
  categories_ = categories;

  for (Observer& observer : observers_) {
    observer.OnPageClassificationCompleted(this, categories_);
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  CHECK(profile);
  if (!IsGeminiSuggestionsSettingEnabled()) {
    RecordContextualCueingDecision(ContextualCueingDecision::kUserOptedOut);
    return;
  }
  if (!IsIgnoreContextualCueingThresholdsEnabled() &&
      !IsUserEligibleForGemini(profile)) {
    RecordContextualCueingDecision(ContextualCueingDecision::kUserIneligible);
    return;
  }

  if (!categories_.has_value() || categories_->empty()) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kFailedCategoryClassification);
    return;
  }

  // Do not issue duplicate requests if a cue is already available or model
  // execution is already in flight.
  if (cue_.has_value() || is_model_execution_in_flight_) {
    return;
  }

  std::string mime_type = web_state_->GetContentsMimeType();
  if (mime_type.empty()) {
    mime_type = "text/html";
  }
  ContextualCueingEvaluator evaluator(GetCapTrackerService(),
                                      GetFeatureEngagementTracker());
  ContextualCueingEvaluator::EvaluationResult evaluation_result =
      evaluator.Evaluate(expected_url, *categories_, mime_type);
  if (!evaluation_result.is_eligible()) {
    RecordContextualCueingDecision(evaluation_result.decision);
    return;
  }

  if (evaluation_result.top_category.has_value()) {
    active_category_type_ = evaluation_result.top_category->category_type;
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
  if (cue_.has_value() || is_model_execution_in_flight_) {
    return;
  }
  is_model_execution_in_flight_ = true;

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
        delegate_->GetEligibleBackgroundTabs(web_state_,
                                             kMaxBackgroundTabs.Get());
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
      {.service_type =
           kUsePrivateAi.Get()
               ? optimization_guide::ModelExecutionServiceType::kPrivateAi
               : optimization_guide::ModelExecutionServiceType::kDefault},
      base::BindOnce(
          &ContextualCueingTabHelper::OnModelExecutionResponseReceived,
          weak_ptr_factory_.GetWeakPtr(), expected_url));
}

void ContextualCueingTabHelper::OnModelExecutionResponseReceived(
    const GURL& expected_url,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  is_model_execution_in_flight_ = false;
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

  feature_engagement::Tracker* tracker = GetFeatureEngagementTracker();
  if (tracker && !IsIgnoreContextualCueingThresholdsEnabled() &&
      !tracker->WouldTriggerHelpUI(
          feature_engagement::kIPHiOSGeminiContextualCueChip)) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kTargetFeatureNotEligible);
    NotifyContextualCueReceived(std::nullopt);
    return;
  }

  NotifyContextualCueReceived(cue);
}

void ContextualCueingTabHelper::NotifyContextualCueReceived(
    std::optional<optimization_guide::proto::ContextualCue> cue) {
  cue_ = std::move(cue);
  if (cue_.has_value()) {
    ContextualCueingCapTrackerService* cap_service = GetCapTrackerService();
    if (cap_service && active_category_type_.has_value()) {
      cue_ui_type_ =
          cap_service->GetCueUiTypeForCategory(*active_category_type_);
    } else {
      cue_ui_type_ = ContextualCueUiType::kMessage;
    }
  } else {
    cue_ui_type_.reset();
  }
  for (Observer& observer : observers_) {
    observer.OnContextualCueReceived(this, cue_);
  }
}

void ContextualCueingTabHelper::InvalidateCue() {
  active_category_type_.reset();
  if (!cue_.has_value()) {
    return;
  }
  cue_.reset();
  cue_ui_type_.reset();
  for (Observer& observer : observers_) {
    observer.OnContextualCueInvalidated(this);
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

feature_engagement::Tracker*
ContextualCueingTabHelper::GetFeatureEngagementTracker() const {
  if (!web_state_) {
    return nullptr;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if (!profile) {
    return nullptr;
  }
  return feature_engagement::TrackerFactory::GetForProfile(profile);
}

#pragma mark - GeminiService::Observer

void ContextualCueingTabHelper::OnGeminiEligibilityChanged() {
  OnSuggestionsPreferenceChanged();
}

#pragma mark - Private

bool ContextualCueingTabHelper::IsGeminiSuggestionsSettingEnabled() const {
  if (!web_state_) {
    return false;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if (!profile || !profile->GetPrefs()) {
    return false;
  }
  return profile->GetPrefs()->GetBoolean(prefs::kIOSGeminiSuggestionsSetting);
}

void ContextualCueingTabHelper::OnSuggestionsPreferenceChanged() {
  if (!web_state_) {
    return;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  if (!profile) {
    return;
  }

  bool is_eligible =
      IsGeminiSuggestionsSettingEnabled() && IsUserEligibleForGemini(profile);
  if (!is_eligible) {
    CancelClassification();
    categories_.reset();
    page_classification_result_.reset();
  }
}

void ContextualCueingTabHelper::DismissFeatureEngagementPromo() {
  fet_dismiss_runner_.RunAndReset();
}

}  // namespace contextual_cueing
