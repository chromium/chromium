// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_java_script_feature.h"

#import "base/values.h"
#import "components/dom_distiller/core/distillable_page_detector.h"
#import "components/dom_distiller/core/page_features.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "reader_mode";
const char kScriptHandlerName[] = "ReaderModeMessageHandler";

// Tab helper method to record the latency of the heuristic JavaScript
// execution.
void RecordHeuristicLatencyIfAvailable(web::WebState* web_state,
                                       const base::Value::Dict& body) {
  std::optional<double> opt_latency = body.FindDouble("time");
  if (!opt_latency.has_value()) {
    return;
  }
  ReaderModeTabHelper* tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  if (tab_helper) {
    tab_helper->RecordReaderModeHeuristicLatency(
        base::Milliseconds(opt_latency.value()));
  }
}

// Tab helper method to process the result of the DOM distiller heuristic.
void ReaderModeHeuristicResultAvailable(web::WebState* web_state,
                                        const GURL& original_url,
                                        ReaderModeHeuristicResult result) {
  ReaderModeTabHelper* tab_helper =
      ReaderModeTabHelper::FromWebState(web_state);
  if (tab_helper) {
    tab_helper->HandleReaderModeHeuristicResult(original_url, result);
  }
}

}  // namespace

ReaderModeJavaScriptFeature::ReaderModeJavaScriptFeature()
    : JavaScriptFeature(
          web::ContentWorld::kIsolatedWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

ReaderModeJavaScriptFeature::~ReaderModeJavaScriptFeature() = default;

// static
ReaderModeJavaScriptFeature* ReaderModeJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ReaderModeJavaScriptFeature> instance;
  return instance.get();
}

std::optional<std::string>
ReaderModeJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void ReaderModeJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  std::optional<GURL> url = message.request_url();
  if (!url.has_value() || UrlHasChromeScheme(url.value()) ||
      url->IsAboutBlank()) {
    // Ignore any Chrome-handled pages. JavaScript cannot be executed on NTP,
    // so this is also ignored by design.
    return;
  }

  if (!message.body() || !message.body()->is_dict()) {
    ReaderModeHeuristicResultAvailable(
        web_state, url.value(), ReaderModeHeuristicResult::kMalformedResponse);
    return;
  }

  RecordHeuristicLatencyIfAvailable(web_state, message.body()->GetDict());

  std::optional<std::vector<double>> result =
      TransformToDerivedFeatures(message.body()->GetDict(), url.value());
  if (!result.has_value()) {
    ReaderModeHeuristicResultAvailable(
        web_state, url.value(), ReaderModeHeuristicResult::kMalformedResponse);
    return;
  }

  const dom_distiller::DistillablePageDetector* detector =
      dom_distiller::DistillablePageDetector::GetNewModel();
  double page_score =
      detector->Score(result.value()) - detector->GetThreshold();
  const dom_distiller::DistillablePageDetector* long_page_detector =
      dom_distiller::DistillablePageDetector::GetLongPageModel();
  double long_page_score = long_page_detector->Score(result.value()) -
                           long_page_detector->GetThreshold();

  ReaderModeHeuristicResult heuristic_result;
  if (long_page_score > 0 && page_score > 0) {
    heuristic_result = ReaderModeHeuristicResult::kReaderModeEligible;
  } else if (page_score <= 0 && long_page_score <= 0) {
    heuristic_result =
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentAndLength;
  } else if (page_score <= 0) {
    heuristic_result =
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentOnly;
  } else {
    heuristic_result =
        ReaderModeHeuristicResult::kReaderModeNotEligibleContentLength;
  }
  ReaderModeHeuristicResultAvailable(web_state, url.value(), heuristic_result);
}

void ReaderModeJavaScriptFeature::TriggerReaderModeHeuristic(
    web::WebFrame* web_frame) {
  CallJavaScriptFunction(web_frame, "readerMode.retrieveDOMFeatures", {});
}

std::optional<std::vector<double>>
ReaderModeJavaScriptFeature::TransformToDerivedFeatures(
    const base::Value::Dict& body,
    const GURL& request_url) {
  std::optional<double> opt_num_elements = body.FindDouble("numElements");
  if (!opt_num_elements.has_value()) {
    return std::nullopt;
  }

  std::optional<double> opt_num_anchors = body.FindDouble("numAnchors");
  if (!opt_num_anchors.has_value()) {
    return std::nullopt;
  }

  std::optional<double> opt_num_forms = body.FindDouble("numForms");
  if (!opt_num_forms.has_value()) {
    return std::nullopt;
  }

  std::optional<double> opt_moz_score = body.FindDouble("mozScore");
  if (!opt_moz_score.has_value()) {
    return std::nullopt;
  }

  std::optional<double> opt_moz_sqrt = body.FindDouble("mozScoreAllSqrt");
  if (!opt_moz_sqrt.has_value()) {
    return std::nullopt;
  }

  std::optional<double> opt_moz_linear = body.FindDouble("mozScoreAllLinear");
  if (!opt_moz_linear.has_value()) {
    return std::nullopt;
  }

  return dom_distiller::CalculateDerivedFeatures(
      true, request_url, opt_num_elements.value(), opt_num_elements.value(),
      opt_num_forms.value(), opt_moz_score.value(), opt_moz_sqrt.value(),
      opt_moz_linear.value());
}
