// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_javascript_feature.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/dom_distiller/core/distillable_page_detector.h"
#include "components/dom_distiller/core/page_features.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kScriptName[] = "distiller_js";
const char kScriptHandlerName[] = "ReadingListDOMMessageHandler";
// Heuristic for words per minute reading speed.
const int kWordCountPerMinute = 275;
// Minimum read time to show the Messages prompt.
const double kReadTimeThreshold = 7.0;
}  // namespace

ReadingListJavaScriptFeature::ReadingListJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kAnyContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentEnd,
              FeatureScript::TargetFrames::kMainFrame,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {web::java_script_features::GetCommonJavaScriptFeature()}) {}

ReadingListJavaScriptFeature::~ReadingListJavaScriptFeature() = default;

absl::optional<std::string>
ReadingListJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptHandlerName;
}

void ReadingListJavaScriptFeature::ScriptMessageReceived(
    web::WebState* web_state,
    const web::ScriptMessage& message) {
  if (!message.body() || !message.body()->is_dict()) {
    // Ignore malformed responses.
    return;
  }

  absl::optional<GURL> url = message.request_url();
  if (!url.has_value() || UrlHasChromeScheme(url.value()) ||
      url.value().SchemeIs(url::kAboutScheme)) {
    // Ignore any Chrome-handled or NTP pages.
    return;
  }

  absl::optional<double> opt_num_elements =
      message.body()->FindDoubleKey("numElements");
  double num_elements = 0.0;
  if (opt_num_elements.has_value()) {
    num_elements = opt_num_elements.value();
  }

  absl::optional<double> opt_num_anchors =
      message.body()->FindDoubleKey("numAnchors");
  double num_anchors = 0.0;
  if (opt_num_anchors.has_value()) {
    num_anchors = opt_num_anchors.value();
  }

  absl::optional<double> opt_num_forms =
      message.body()->FindDoubleKey("numForms");
  double num_forms = 0.0;
  if (opt_num_forms.has_value()) {
    num_forms = opt_num_forms.value();
  }

  absl::optional<double> opt_moz_score =
      message.body()->FindDoubleKey("mozScore");
  double moz_score = 0.0;
  if (opt_moz_score.has_value()) {
    moz_score = opt_moz_score.value();
  }

  absl::optional<double> opt_moz_sqrt =
      message.body()->FindDoubleKey("mozScoreAllSqrt");
  double moz_sqrt = 0.0;
  if (opt_moz_sqrt.has_value()) {
    moz_sqrt = opt_moz_sqrt.value();
  }

  absl::optional<double> opt_moz_linear =
      message.body()->FindDoubleKey("mozScoreAllLinear");
  double moz_linear = 0.0;
  if (opt_moz_linear.has_value()) {
    moz_linear = opt_moz_linear.value();
  }

  absl::optional<double> time = message.body()->FindDoubleKey("time");
  if (time.has_value()) {
    UMA_HISTOGRAM_TIMES("IOS.ReadingList.Javascript.ExecutionTime",
                        base::TimeDelta::FromMillisecondsD(time.value()));
  }

  std::vector<double> result = dom_distiller::CalculateDerivedFeatures(
      true, message.request_url().value(), num_elements, num_anchors, num_forms,
      moz_score, moz_sqrt, moz_linear);

  const dom_distiller::DistillablePageDetector* detector =
      dom_distiller::DistillablePageDetector::GetNewModel();
  // Equivalent of DistillablePageDetector::Classify().
  double score = detector->Score(result) - detector->GetThreshold();
  // Translate score by +1 to make histogram logging simpler by keeping all
  // scores positive. Multiply by 100 to get granular scoring logging to the
  // hundredths digit.
  base::UmaHistogramCustomCounts(
      "IOS.ReadingList.Javascript.RegularDistillabilityScore",
      (score + 1) * 100, 0, 400, 50);

  const dom_distiller::DistillablePageDetector* long_detector =
      dom_distiller::DistillablePageDetector::GetLongPageModel();
  // Equivalent of DistillablePageDetector::Classify().
  double long_score =
      long_detector->Score(result) - long_detector->GetThreshold();
  // Translate score by +1 to make histogram logging simpler by keeping all
  // scores positive. Multiply by 100 to get granular scoring logging to the
  // hundredths digit.
  base::UmaHistogramCustomCounts(
      "IOS.ReadingList.Javascript.LongPageDistillabilityScore",
      (long_score + 1) * 100, 0, 400, 50);

  // Calculate Time to Read
  absl::optional<double> opt_word_count =
      message.body()->FindDoubleKey("wordCount");
  double estimated_read_time = 0.0;
  if (opt_word_count.has_value()) {
    estimated_read_time = opt_word_count.value() / kWordCountPerMinute;
    base::UmaHistogramCounts100("IOS.ReadingList.Javascript.TimeToRead",
                                estimated_read_time);
  }

  if (score > 0 && estimated_read_time > kReadTimeThreshold &&
      CanShowReadingListMessages()) {
    // TODO(crbug.com/1195978): Insert Reading List Banner Overlay Request after
    // a three second delay. Also check if the page is already in the Reading
    // List before adding.
    SaveReadingListMessagesShownTime();
  }
}

bool ReadingListJavaScriptFeature::CanShowReadingListMessages() {
  NSDate* last_shown_timestamp = [[NSUserDefaults standardUserDefaults]
      objectForKey:kLastTimeUserShownReadingListMessages];
  return !last_shown_timestamp;
}

void ReadingListJavaScriptFeature::SaveReadingListMessagesShownTime() {
  [[NSUserDefaults standardUserDefaults]
      setObject:[NSDate date]
         forKey:kLastTimeUserShownReadingListMessages];
}
