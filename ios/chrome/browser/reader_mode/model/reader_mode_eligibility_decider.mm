// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_eligibility_decider.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/utf_string_conversions.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

ReaderModeEligibilityDecider::ReaderModeEligibilityDecider(
    web::WebState* web_state,
    ReaderModeMetricsHelper* metrics_helper)
    : web_state_(web_state),
      metrics_helper_(metrics_helper),
      // Unretained is safe because `decider_` is owned by this object.
      decider_(*this,
               OptimizationGuideServiceFactory::GetForProfile(
                   ProfileIOS::FromBrowserState(web_state->GetBrowserState())),
               kReaderModeHeuristicPageLoadDelay,
               base::BindRepeating(
                   &ReaderModeEligibilityDecider::OnEligibilityResult,
                   base::Unretained(this))) {
  CHECK(web_state_);
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  if (optimization_guide_service) {
    optimization_guide_service->RegisterOptimizationTypes(
        {optimization_guide::proto::READER_MODE_ELIGIBLE});
  }
}

ReaderModeEligibilityDecider::~ReaderModeEligibilityDecider() = default;

void ReaderModeEligibilityDecider::HandleReaderModeHeuristicResult(
    web_distiller::ReaderModeEligibility result) {
  metrics_helper_->RecordReaderHeuristicCompleted(result);
}

void ReaderModeEligibilityDecider::StartDecision(const GURL& url) {
  // Guarantee that there is only one trigger heuristic running at a time.
  ResetDecision(url);
  decider_.Start(url);
}

void ReaderModeEligibilityDecider::ResetDecision(const GURL& url) {
  if (decider_.Cancel()) {
    metrics_helper_->RecordReaderHeuristicCanceled();
  } else {
    // If there is no trigger in progress ensure any metrics related to a
    // past navigation have been recorded.
    metrics_helper_->Flush(
        ReaderModeDeactivationReason::kNavigationDeactivated);
  }

  // Do not reset URL eligibility for same-page navigations.
  if (!reader_mode_eligible_url_.EqualsIgnoringRef(url)) {
    reader_mode_eligible_url_ = GURL();
  }
}

bool ReaderModeEligibilityDecider::CurrentPageIsEligibleForReaderMode() const {
  return IsContentHTML() && web_distiller::IsUrlEligible(GetLastCommittedURL());
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

GURL ReaderModeEligibilityDecider::GetLastCommittedURL() const {
  return !web_state_->IsBeingDestroyed() ? web_state_->GetLastCommittedURL()
                                         : GURL();
}

bool ReaderModeEligibilityDecider::IsContentHTML() const {
  return !web_state_->IsBeingDestroyed() && web_state_->ContentIsHTML();
}

void ReaderModeEligibilityDecider::ExecuteScript(
    const std::string& script,
    base::OnceCallback<void(const base::Value*)> callback) {
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
  main_frame->ExecuteJavaScript(base::UTF8ToUTF16(script), std::move(callback));
}

void ReaderModeEligibilityDecider::OnEligibilityResult(
    const GURL& url,
    std::optional<web_distiller::ReaderModeEligibility> result,
    bool is_current_url) {
  if (!result.has_value()) {
    // If the current page does not support running the heuristic, then the
    // eligibility of the current page is already known.
    last_committed_url_eligibility_ready_ = true;
    CallLastCommittedUrlEligibilityCallbacks(false);
    return;
  }

  metrics_helper_->RecordReaderHeuristicCompleted(result.value());

  if (!is_current_url) {
    // There has been a change in the committed URL since the last heuristic
    // run, do not process the result.
    return;
  }

  reader_mode_eligible_url_ =
      result.value() ==
              web_distiller::ReaderModeEligibility::kReaderModeEligible
          ? url
          : GURL();
  if (last_committed_url_without_ref_.EqualsIgnoringRef(url)) {
    last_committed_url_eligibility_ready_ = true;
    CallLastCommittedUrlEligibilityCallbacks(CurrentPageIsDistillable());
  }
}

void ReaderModeEligibilityDecider::CallLastCommittedUrlEligibilityCallbacks(
    std::optional<bool> result) {
  for (auto& callback : last_committed_url_eligibility_callbacks_) {
    std::move(callback).Run(result);
  }
  last_committed_url_eligibility_callbacks_.clear();
}
