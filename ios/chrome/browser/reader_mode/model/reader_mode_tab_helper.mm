// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"

#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "components/dom_distiller/core/extraction_utils.h"
#import "components/dom_distiller/ios/distiller_page_utils.h"
#import "components/prefs/pref_service.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "third_party/dom_distiller_js/dom_distiller.pb.h"
#import "third_party/dom_distiller_js/dom_distiller_json_converter.h"

namespace {

// Records the classification accuracy of the Reader Mode heuristic by
// comparing the distillation of the page to the heuristic result.
void RecordReaderModeHeuristicClassification(bool is_distillable_page,
                                             ReaderModeHeuristicResult result) {
  ReaderModeHeuristicClassification classification;
  switch (result) {
    case ReaderModeHeuristicResult::kMalformedResponse:
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength:
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly:
    case ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength: {
      classification = is_distillable_page
                           ? ReaderModeHeuristicClassification::
                                 kPageNotEligibleWithPopulatedDistill
                           : ReaderModeHeuristicClassification::
                                 kPageNotEligibleWithEmptyDistill;
      break;
    }
    case ReaderModeHeuristicResult::kReaderModeEligible: {
      classification = is_distillable_page
                           ? ReaderModeHeuristicClassification::
                                 kPageEligibleWithPopulatedDistill
                           : ReaderModeHeuristicClassification::
                                 kPageEligibleWithEmptyDistill;
      break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION(kReaderModeHeuristicClassificationHistogram,
                            classification);
}

// Records the time elapsed from the execution of the distillation JavaScript to
// the result callback.
void RecordReaderModeDistillationLatency(base::TimeDelta elapsed,
                                         ukm::SourceId source_id) {
  UMA_HISTOGRAM_TIMES(kReaderModeDistillerLatencyHistogram, elapsed);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Distiller_Latency(source_id)
        .SetLatency(elapsed.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }
}

// Records whether the given source ID for a navigation is distillable or not.
void RecordReaderModeDistillationResult(bool is_distillable,
                                        ukm::SourceId source_id) {
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Distiller_Result(source_id)
        .SetResult(static_cast<int64_t>(
            is_distillable ? ReaderModeDistillerResult::kPageIsDistillable
                           : ReaderModeDistillerResult::kPageIsNotDistillable))
        .Record(ukm::UkmRecorder::Get());
  }
}

bool IsKnownAmpCache(web::WebFrame* web_frame) {
  url::Origin origin = web_frame->GetSecurityOrigin();
  // Source:
  // https://github.com/ampproject/amphtml/blob/main/build-system/global-configs/caches.json
  return origin.DomainIs("cdn.ampproject.org") ||
         origin.DomainIs("bing-amp.com");
}

// Records whether any available web frames in the current web state use AMP.
// This metric will help determine whether AMP special casing will affect
// distillation success.
void RecordReaderModeForAmpDistill(bool is_distillable_page,
                                   web::WebState* web_state) {
  if (!web_state) {
    return;
  }

  web::WebFramesManager* page_world_manager =
      web_state->GetPageWorldWebFramesManager();
  auto web_frames = page_world_manager->GetAllWebFrames();
  bool is_amp =
      std::any_of(web_frames.begin(), web_frames.end(), IsKnownAmpCache);

  ReaderModeAmpClassification classification;
  if (is_amp) {
    classification = is_distillable_page
                         ? ReaderModeAmpClassification::kPopulatedDistillWithAmp
                         : ReaderModeAmpClassification::kEmptyDistillWithAmp;
  } else {
    classification = is_distillable_page
                         ? ReaderModeAmpClassification::kPopulatedDistillNoAmp
                         : ReaderModeAmpClassification::kEmptyDistillNoAmp;
  }

  UMA_HISTOGRAM_ENUMERATION(kReaderModeAmpClassificationHistogram,
                            classification);
}

}  // namespace

ReaderModeTabHelper::ReaderModeTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
  web_state_->AddObserver(this);
}

ReaderModeTabHelper::~ReaderModeTabHelper() = default;

void ReaderModeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  CHECK_EQ(web_state, web_state_);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    // Reset the overridden URL state if there is a new navigation.
    if (web_state_->GetVisibleURL() != overridden_url_for_debug_) {
      overridden_url_for_debug_ = GURL();
    }
    // Guarantee that there is only one trigger heuristic running at a time.
    if (trigger_reader_mode_timer_.IsRunning()) {
      trigger_reader_mode_timer_.Stop();
    }
    trigger_reader_mode_timer_.Start(
        FROM_HERE, ReaderModeDistillerPageLoadDelay(),
        base::BindOnce(&ReaderModeTabHelper::TriggerReaderModeHeuristic,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ReaderModeTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  // A new navigation is started while the Reader Mode heuristic trigger is
  // running on the previous navigation. Stop the trigger to attach the new
  // navigation.
  if (trigger_reader_mode_timer_.IsRunning()) {
    trigger_reader_mode_timer_.Stop();
  }
}

void ReaderModeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void ReaderModeTabHelper::HandleReaderModeHeuristicResult(
    const GURL& url,
    ReaderModeHeuristicResult result) {
  UMA_HISTOGRAM_ENUMERATION(kReaderModeHeuristicResultHistogram, result);

  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Result(source_id)
        .SetResult(static_cast<int64_t>(result))
        .Record(ukm::UkmRecorder::Get());
  }

  if (!base::FeatureList::IsEnabled(kEnableReaderModeDistiller)) {
    return;
  }

  // Gets the instance of the WebFramesManager from `web_state_` that can
  // execute the DOM distiller JavaScript in the isolated content world.
  web::WebFramesManager* web_frames_manager =
      web_state_->GetWebFramesManager(web::ContentWorld::kIsolatedWorld);
  if (!web_frames_manager) {
    return;
  }
  web::WebFrame* main_frame = web_frames_manager->GetMainWebFrame();
  if (!main_frame) {
    return;
  }
  // If the current WebState URL is not the same as the one processed by the
  // heuristic then abort next steps.
  if (url != web_state_->GetVisibleURL()) {
    return;
  }
  // TODO(crbug.com/405309236): The distillation should be moved into the
  // core DOM distiller logic. This extraction is for metrics collection
  // purposes only.
  dom_distiller::proto::DomDistillerOptions options;
  main_frame->ExecuteJavaScript(
      base::UTF8ToUTF16(dom_distiller::GetDistillerScriptWithOptions(options)),
      base::BindOnce(&ReaderModeTabHelper::PageDistillationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), result,
                     base::TimeTicks::Now()));
}

void ReaderModeTabHelper::RecordReaderModeHeuristicLatency(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kReaderModeHeuristicLatencyHistogram, delta);
  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::IOS_ReaderMode_Heuristic_Latency(source_id)
        .SetLatency(delta.InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }
}

bool ReaderModeTabHelper::CanTriggerReaderModeHeuristic() {
  if (!base::FeatureList::IsEnabled(kEnableReaderModeDistillerHeuristic)) {
    return false;
  }
  // If the Reader Mode HTML has already been overridden for this page, do not
  // trigger the heuristic again.
  if (experimental_flags::ShouldForceReaderModeDebugHTMLOverride() &&
      overridden_url_for_debug_.spec() == web_state_->GetVisibleURL().spec()) {
    return false;
  }
  const double page_load_probability =
      kReaderModeDistillerPageLoadProbability.Get();
  if (page_load_probability <= 0.0 || page_load_probability > 1.0) {
    // Invalid probability range. Disable the Reader Mode feature.
    return false;
  }

  const double rand_double = base::RandDouble();
  return rand_double < page_load_probability;
}

void ReaderModeTabHelper::TriggerReaderModeHeuristic() {
  if (!CanTriggerReaderModeHeuristic()) {
    return;
  }
  web::WebFramesManager* web_frames_manager =
      ReaderModeJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  if (!web_frames_manager) {
    return;
  }
  web::WebFrame* web_frame = web_frames_manager->GetMainWebFrame();
  if (!web_frame) {
    return;
  }
  ReaderModeJavaScriptFeature::GetInstance()->TriggerReaderModeHeuristic(
      web_frame);
}

void ReaderModeTabHelper::PageDistillationCompleted(
    ReaderModeHeuristicResult heuristic_result,
    base::TimeTicks start_time,
    const base::Value* value) {
  // If ExecuteJavaScript completion is run after WebState is destroyed, do
  // not continue metrics collection.
  if (!web_state_ || web_state_->IsBeingDestroyed()) {
    return;
  }
  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebStateDocument(web_state_);
  RecordReaderModeDistillationLatency(base::TimeTicks::Now() - start_time,
                                      source_id);

  std::unique_ptr<dom_distiller::proto::DomDistillerResult> distiller_result =
      std::make_unique<dom_distiller::proto::DomDistillerResult>();
  bool found_content = false;
  base::Value result_as_value =
      dom_distiller::ParseValueFromScriptResult(value);
  if (!result_as_value.is_none()) {
    found_content =
        dom_distiller::proto::json::DomDistillerResult::ReadFromValue(
            result_as_value, distiller_result.get());
  }

  bool is_distillable_page =
      found_content && !distiller_result->distilled_content().html().empty();
  RecordReaderModeDistillationResult(is_distillable_page, source_id);
  RecordReaderModeHeuristicClassification(is_distillable_page,
                                          heuristic_result);
  RecordReaderModeForAmpDistill(is_distillable_page, web_state_);

  if (is_distillable_page &&
      experimental_flags::ShouldForceReaderModeDebugHTMLOverride()) {
    overridden_url_for_debug_ = web_state_->GetVisibleURL();
    web_state_->LoadSimulatedRequest(
        web_state_->GetVisibleURL(),
        base::SysUTF8ToNSString(distiller_result->distilled_content().html()));
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(ReaderModeTabHelper)
