// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_eligibility_decider.h"

#import "base/containers/fixed_flat_set.h"
#import "base/functional/bind.h"
#import "base/strings/utf_string_conversions.h"
#import "components/dom_distiller/core/extraction_utils.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns the Readability heuristic result if it is available otherwise returns
// `kMalformedResponse`.
ReaderModeHeuristicResult GetReaderModeHeuristicResult(
    const base::Value* result) {
  if (!result) {
    return ReaderModeHeuristicResult::kMalformedResponse;
  }
  std::optional<bool> result_conversion = result->GetIfBool();
  if (result_conversion.has_value()) {
    return result_conversion.value()
               ? ReaderModeHeuristicResult::kReaderModeEligible
               : ReaderModeHeuristicResult::
                     kReaderModeNotEligibleContentAndLength;
  }
  return ReaderModeHeuristicResult::kMalformedResponse;
}

// These are known google websites that don't support reader mode.
static constexpr auto kGoogleWorkspaceBlocklist =
    base::MakeFixedFlatSet<std::string_view>(
        {"assistant.google.com", "calendar.google.com", "docs.google.com",
         "drive.google.com", "mail.google.com", "photos.google.com"});

}  // namespace

ReaderModeEligibilityDecider::ReaderModeEligibilityDecider(
    web::WebState* web_state,
    ReaderModeMetricsHelper* metrics_helper)
    : web_state_(web_state), metrics_helper_(metrics_helper) {
  if (IsReaderModeOptimizationGuideEligibilityAvailable()) {
    OptimizationGuideService* optimization_guide_service =
        OptimizationGuideServiceFactory::GetForProfile(
            ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
    if (optimization_guide_service) {
      optimization_guide_service->RegisterOptimizationTypes(
          {optimization_guide::proto::READER_MODE_ELIGIBLE});
      optimization_guide_decider_ = optimization_guide_service;
    }
  }
}

ReaderModeEligibilityDecider::~ReaderModeEligibilityDecider() = default;

void ReaderModeEligibilityDecider::HandleReaderModeHeuristicResult(
    ReaderModeHeuristicResult result) {
  if (result == ReaderModeHeuristicResult::kReaderModeEligible &&
      optimization_guide_decider_ &&
      IsReaderModeOptimizationGuideEligibilityAvailable()) {
    // Do additional checks.
    optimization_guide_decider_->CanApplyOptimization(
        eligibility_heuristic_url_.value(),
        optimization_guide::proto::READER_MODE_ELIGIBLE,
        base::BindOnce(
            &ReaderModeEligibilityDecider::OnOptimizationGuideDecision,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  CompleteHeuristic(result);
}

void ReaderModeEligibilityDecider::StartDecision(const GURL& url) {
  if (!IsReaderModeAvailable()) {
    return;
  }
  // Guarantee that there is only one trigger heuristic running at a time.
  ResetDecision(url);

  trigger_reader_mode_timer_.Start(
      FROM_HERE, ReaderModeHeuristicPageLoadDelay(),
      base::BindOnce(&ReaderModeEligibilityDecider::TriggerReaderModeHeuristic,
                     weak_ptr_factory_.GetWeakPtr(), url));
}

void ReaderModeEligibilityDecider::ResetDecision(const GURL& url) {
  // Ensure that only one asynchronous eligibility check is running at a time.
  if (trigger_reader_mode_timer_.IsRunning()) {
    trigger_reader_mode_timer_.Stop();
    metrics_helper_->RecordReaderHeuristicCanceled();
  } else {
    // If there is no trigger in progress ensure any metrics related to a
    // past navigation have been recorded.
    metrics_helper_->Flush(
        ReaderModeDeactivationReason::kNavigationDeactivated);
  }
  eligibility_heuristic_url_.reset();

  // Do not reset URL eligibility for same-page navigations.
  if (!reader_mode_eligible_url_.EqualsIgnoringRef(url)) {
    reader_mode_eligible_url_ = GURL();
  }
}

bool ReaderModeEligibilityDecider::CurrentPageIsEligibleForReaderMode() const {
  bool eligible = web_state_ && !web_state_->IsBeingDestroyed() &&
                  web_state_->ContentIsHTML();
  if (!eligible) {
    return false;
  }
  GURL current_url = web_state_->GetLastCommittedURL();
  return current_url.SchemeIsHTTPOrHTTPS() &&
         !google_util::IsGoogleHomePageUrl(current_url) &&
         !google_util::IsGoogleSearchUrl(current_url) &&
         !google_util::IsYoutubeDomainUrl(
             current_url, google_util::ALLOW_SUBDOMAIN,
             google_util::ALLOW_NON_STANDARD_PORTS) &&
         !kGoogleWorkspaceBlocklist.contains(current_url.host());
}

bool ReaderModeEligibilityDecider::CurrentPageIsDistillable() const {
  return CurrentPageIsEligibleForReaderMode() &&
         last_committed_url_eligibility_ready_ &&
         last_committed_url_without_ref_.is_valid() &&
         last_committed_url_without_ref_.EqualsIgnoringRef(
             reader_mode_eligible_url_);
}

void ReaderModeEligibilityDecider::FetchLastCommittedUrlDistillabilityResult(
    base::OnceCallback<void(std::optional<bool>)> callback) {
  if (last_committed_url_eligibility_ready_) {
    std::move(callback).Run(CurrentPageIsDistillable());
    return;
  }
  last_committed_url_eligibility_callbacks_.push_back(std::move(callback));
}

void ReaderModeEligibilityDecider::SetLastCommittedUrl(const GURL& url) {
  if (url.EqualsIgnoringRef(last_committed_url_without_ref_)) {
    return;
  }
  last_committed_url_without_ref_ = url;
  last_committed_url_eligibility_ready_ = false;
  // At this point, the only callbacks waiting for results have been added since
  // the last committed URL, before the Reader mode heuristic could determine
  // eligibility. Hence, they can all be called with nullopt (no result).
  CallLastCommittedUrlEligibilityCallbacks(std::nullopt);
}

void ReaderModeEligibilityDecider::TriggerReaderModeHeuristic(const GURL& url) {
  if (!IsReaderModeAvailable()) {
    return;
  }
  if (!CurrentPageIsEligibleForReaderMode()) {
    // If the current page does not support running the heuristic, then the
    // eligibility of the current page is already known.
    last_committed_url_eligibility_ready_ = true;
    CallLastCommittedUrlEligibilityCallbacks(false);
    return;
  }

  web::WebFramesManager* web_frames_manager =
      ReaderModeJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  if (!web_frames_manager) {
    return;
  }
  web::WebFrame* main_frame = web_frames_manager->GetMainWebFrame();
  if (!main_frame) {
    return;
  }

  metrics_helper_->RecordReaderHeuristicTriggered();
  eligibility_heuristic_url_ = url;
  if (base::FeatureList::IsEnabled(kEnableReadabilityHeuristic)) {
    main_frame->ExecuteJavaScript(
        base::UTF8ToUTF16(dom_distiller::GetReadabilityTriggeringScript()),
        base::BindOnce(
            &ReaderModeEligibilityDecider::HandleReadabilityHeuristicResult,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    ReaderModeJavaScriptFeature::GetInstance()->TriggerReaderModeHeuristic(
        main_frame);
  }
}

void ReaderModeEligibilityDecider::HandleReadabilityHeuristicResult(
    const base::Value* result) {
  HandleReaderModeHeuristicResult(GetReaderModeHeuristicResult(result));
}

void ReaderModeEligibilityDecider::OnOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  ReaderModeHeuristicResult result;
  switch (decision) {
    case optimization_guide::OptimizationGuideDecision::kTrue: {
      result = ReaderModeHeuristicResult::kReaderModeEligible;
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kFalse: {
      result = ReaderModeHeuristicResult::
          kReaderModeNotEligibleOptimizationGuideIneligible;
      break;
    }
    case optimization_guide::OptimizationGuideDecision::kUnknown: {
      result = ReaderModeHeuristicResult::
          kReaderModeNotEligibleOptimizationGuideUnknown;
      break;
    }
  }
  return CompleteHeuristic(result);
}

void ReaderModeEligibilityDecider::CompleteHeuristic(
    ReaderModeHeuristicResult result) {
  metrics_helper_->RecordReaderHeuristicCompleted(result);

  if (!eligibility_heuristic_url_.has_value() ||
      !eligibility_heuristic_url_->EqualsIgnoringRef(
          web_state_->GetLastCommittedURL())) {
    // There has been a change in the committed URL since the last heuristic
    // run, do not process the result.
    eligibility_heuristic_url_.reset();
    return;
  }

  reader_mode_eligible_url_ =
      result == ReaderModeHeuristicResult::kReaderModeEligible
          ? eligibility_heuristic_url_.value()
          : GURL();
  if (last_committed_url_without_ref_.EqualsIgnoringRef(
          eligibility_heuristic_url_.value())) {
    last_committed_url_eligibility_ready_ = true;
    CallLastCommittedUrlEligibilityCallbacks(CurrentPageIsDistillable());
  }
  eligibility_heuristic_url_.reset();
}

void ReaderModeEligibilityDecider::CallLastCommittedUrlEligibilityCallbacks(
    std::optional<bool> result) {
  for (auto& callback : last_committed_url_eligibility_callbacks_) {
    std::move(callback).Run(result);
  }
  last_committed_url_eligibility_callbacks_.clear();
}
