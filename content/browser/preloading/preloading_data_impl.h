// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_DATA_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_DATA_IMPL_H_

#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/preloading_prediction.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {

class PreloadingAttemptImpl;

// Defines predictors confusion matrix enums used by UMA records. Entries should
// not be renumbered and numeric values should never be reused. Please update
// "PredictorConfusionMatrix" in `tools/metrics/histograms/enums.xml` when new
// enums are added.
enum class PredictorConfusionMatrix {
  // True positive.
  kTruePositive = 0,
  // False positive.
  kFalsePositive = 1,
  // True negative.
  kTrueNegative = 2,
  // False negative.
  kFalseNegative = 3,
  // Required by UMA histogram macro.
  kMaxValue = kFalseNegative
};

// The scope of current preloading logging is only limited to the same
// WebContents navigations. If the predicted URL is opened in a new tab we lose
// the data corresponding to the navigation in different WebContents.
// TODO(crbug.com/40227626): Expand PreloadingData scope to consider multiple
// WebContent navigations.
class CONTENT_EXPORT PreloadingDataImpl
    : public PreloadingData,
      public WebContentsUserData<PreloadingDataImpl>,
      public WebContentsObserver {
 public:
  ~PreloadingDataImpl() override;

  static PreloadingDataImpl* GetOrCreateForWebContents(
      WebContents* web_contents);

  // NoVarySearch is a `/content/browser` feature so is the matcher getter.
  // The matcher first checks if the navigated URL is the same as the
  // prediction; if not, the matcher checks if the navigated URL matches
  // any NoVarySearch query using `PrefetchService` if No-Vary-Search feature is
  // enabled.
  static PreloadingURLMatchCallback GetPrefetchServiceMatcher(
      PrefetchService& prefetch_service,
      const PrefetchContainer::Key& predicted);

  // Disallow copy and assign.
  PreloadingDataImpl(const PreloadingDataImpl& other) = delete;
  PreloadingDataImpl& operator=(const PreloadingDataImpl& other) = delete;

  // PreloadingDataImpl implementation:
  PreloadingAttempt* AddPreloadingAttempt(
      PreloadingPredictor predictor,
      PreloadingType preloading_type,
      PreloadingURLMatchCallback url_match_predicate,
      std::optional<PreloadingType> planned_max_preloading_type,
      ukm::SourceId triggering_primary_page_source_id) override;
  void AddPreloadingPrediction(
      PreloadingPredictor predictor,
      int confidence,
      PreloadingURLMatchCallback url_match_predicate,
      ukm::SourceId triggering_primary_page_source_id) override;
  void SetIsNavigationInDomainCallback(
      PreloadingPredictor predictor,
      PredictorDomainCallback is_navigation_in_domain_callback) override;
  void SetHasSpeculationRulesPrerender();
  bool HasSpeculationRulesPrerender() override;
  void OnPreloadingHeuristicsModelInput(
      const GURL& url,
      ModelPredictionTrainingData::OutcomeCallback on_record_outcome) override;

  void AddPreloadingPrediction(const PreloadingPredictor& predictor,
                               PreloadingConfidence confidence,
                               PreloadingURLMatchCallback url_match_predicate,
                               ukm::SourceId triggering_primary_page_source_id);

  // A version of `AddPreloadingAttempt` which takes two PreloadingPredictors in
  // the case where one predictor creates a preloading candidate which is
  // enacted by another predictor (e.g. a non-eager speculation rule creates a
  // candidate which is enacted by a pointer down heuristic).
  PreloadingAttemptImpl* AddPreloadingAttempt(
      const PreloadingPredictor& creating_predictor,
      const PreloadingPredictor& enacting_predictor,
      PreloadingType preloading_type,
      PreloadingURLMatchCallback url_match_predicate,
      std::optional<PreloadingType> planned_max_preloading_type,
      ukm::SourceId triggering_primary_page_source_id);

  void CopyPredictorDomains(const PreloadingDataImpl& other,
                            const std::vector<PreloadingPredictor>& predictors);

  // The output of many predictors is a score (usually probability or logit),
  // where a higher score indicates a higher confidence in the prediction. To
  // use this score for binary classification, we compare it to a threshold. If
  // the score is above the threshold, we classify the instance as positive;
  // otherwise, we classify it as negative. Threshold choice affects classifier
  // precision and recall. There is a trade-off between precision and recall. If
  // we set the threshold too low, we will have high precision but low recall.
  // If we set the threshold too high, we will have high recall but low
  // precision. To choose the best threshold, we can use ROC curves,
  // precision-recall curves, or precision and recall curves.
  // `ExperimentalPreloadingPrediction` helps us collect the UMA data required
  // to achieve this. This method creates a new
  // ExperimentalPreloadingPrediction. Same as above `url_match_predicate` is
  // passed by the caller to verify that navigated URL is a match. The `score`
  // is the probability/logit score, `min_score`  and `max_score` are the
  // minimum/maximum values that the `score` can have and `buckets` is the
  // number of buckets that will be used for UMA aggregation and should be less
  // than 101.
  void AddExperimentalPreloadingPrediction(
      std::string_view name,
      PreloadingURLMatchCallback url_match_predicate,
      float score,
      float min_score,
      float max_score,
      size_t buckets);

  // WebContentsObserver override.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  size_t GetPredictionsSizeForTesting() const;
  void SetMaxPredictionsToTenForTesting();

 private:
  explicit PreloadingDataImpl(WebContents* web_contents);
  friend class WebContentsUserData<PreloadingDataImpl>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  void RecordMetricsForPreloadingAttempts(
      ukm::SourceId navigated_page_source_id);
  void RecordUKMForPreloadingPredictions(
      ukm::SourceId navigated_page_source_id);
  void SetIsAccurateTriggeringAndPrediction(const GURL& navigated_url);

  void RecordPreloadingAttemptPrecisionToUMA(
      const PreloadingAttemptImpl& attempt);
  void RecordPredictionPrecisionToUMA(const PreloadingPrediction& prediction);

  void UpdatePreloadingAttemptRecallStats(const PreloadingAttemptImpl& attempt);
  void UpdatePredictionRecallStats(const PreloadingPrediction& prediction);

  void ResetRecallStats();
  void RecordRecallStatsToUMA(NavigationHandle* navigation_handle);

  // Stores recall statistics for preloading predictions/attempts to later
  // record them to UMA.
  base::flat_map<PreloadingPredictor, PredictorDomainCallback>
      is_navigation_in_predictor_domain_callbacks_;
  base::flat_set<PreloadingPredictor> predictions_recall_stats_;
  base::flat_set<std::pair<PreloadingPredictor, PreloadingType>>
      preloading_attempt_recall_stats_;

  // Stores all the experimental preloading predictions that are happening for
  // the next navigation until the navigation takes place or the WebContents is
  // destroyed.
  std::vector<ExperimentalPreloadingPrediction> experimental_predictions_;
  size_t total_seen_experimental_predictions_ = 0;

  std::vector<ModelPredictionTrainingData> ml_predictions_;
  size_t total_seen_ml_predictions_ = 0;

  // Stores all the preloading attempts that are happening for the next
  // navigation until the navigation takes place.
  std::vector<std::unique_ptr<PreloadingAttemptImpl>> preloading_attempts_;

  // Stores all the preloading predictions that are happening for the next
  // navigation until the navigation takes place.
  std::vector<PreloadingPrediction> preloading_predictions_;
  size_t total_seen_preloading_predictions_ = 0;

  // This flag will be true if there's been at least 1 attempt to do a
  // speculation-rules based prerender.
  bool has_speculation_rules_prerender_ = false;

  // The random seed used to determine if a preloading attempt should be sampled
  // in UKM logs. We use a different random seed for each session and then hash
  // that seed with the UKM source ID so that all attempts for a given source ID
  // are sampled in or out together.
  uint32_t sampling_seed_;

  // In production, a large number of predictions are allowed before we start
  // sampling. For tests, we may set the limit to something small.
  bool max_predictions_is_ten_for_testing_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_DATA_IMPL_H_
