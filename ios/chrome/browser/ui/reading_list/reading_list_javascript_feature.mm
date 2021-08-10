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
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/ukm/ios/ukm_url_recorder.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/reading_list/ios_add_to_reading_list_infobar_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/web_state.h"
#include "services/metrics/public/cpp/ukm_builders.h"

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
  // The JavaScript shouldn't be executed in incognito pages.
  DCHECK(!web_state->GetBrowserState()->IsOffTheRecord());

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

  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(web_state);
  if (sourceID != ukm::kInvalidSourceId) {
    // Round to the nearest tenth, and additionally round to a .5 level of
    // granularity if <0.5 or > 1.5. Get accuracy to the tenth digit in UKM by
    // multiplying by 10.
    int score_minimization = (int)(round(score * 10));
    int long_score_minimization = (int)(round(long_score * 10));
    if (score_minimization > 15 || score_minimization < 5) {
      score_minimization = ((score_minimization + 2.5) / 5) * 5;
    }
    if (long_score_minimization > 15 || long_score_minimization < 5) {
      long_score_minimization = ((long_score_minimization + 2.5) / 5) * 5;
    }
    ukm::builders::IOS_PageReadability(sourceID)
        .SetDistilibilityScore(score_minimization)
        .SetDistilibilityLongScore(long_score_minimization)
        .Record(ukm::UkmRecorder::Get());
  }

  // Calculate Time to Read
  absl::optional<double> opt_word_count =
      message.body()->FindDoubleKey("wordCount");
  double estimated_read_time = 0.0;
  if (opt_word_count.has_value()) {
    estimated_read_time = opt_word_count.value() / kWordCountPerMinute;
    base::UmaHistogramCounts100("IOS.ReadingList.Javascript.TimeToRead",
                                estimated_read_time);
  }

  ReadingListModel* model = ReadingListModelFactory::GetForBrowserState(
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState()));
  const ReadingListEntry* entry = model->GetEntryByURL(url.value());
  if (entry) {
    // Update an existing Reading List entry with the estimated time to read.
    // Either way, return early to not show a Messages banner for an existing
    // Reading List entry.
    if (entry->EstimatedReadTime().is_zero()) {
      model->SetEstimatedReadTime(
          url.value(), base::TimeDelta::FromMinutes(estimated_read_time));
    }
    return;
  }
  if (!web_state->IsVisible()) {
    // Do not show the Messages banner if the WebState is not visible, but delay
    // this check in case the estimated read time can be set for an existing
    // entry.
    return;
  }
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  PrefService* user_prefs = browser_state->GetPrefs();
  bool neverShowPrefSet =
      user_prefs->GetBoolean(kPrefReadingListMessagesNeverShow);
  if (neverShowPrefSet) {
    // Do not show prompt if user explicitly selected to never show it.
    return;
  }

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (manager && score > 0 && estimated_read_time > kReadTimeThreshold &&
      CanShowReadingListMessages()) {
    SaveReadingListMessagesShownTime();
    auto delegate = std::make_unique<IOSAddToReadingListInfobarDelegate>(
        web_state->GetVisibleURL(), web_state->GetTitle(),
        static_cast<int>(estimated_read_time), model, web_state);
    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeAddToReadingList, std::move(delegate), false);
    manager->AddInfoBar(std::move(infobar), true);
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
