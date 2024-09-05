// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_data_impl.h"

#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/browser/preloading/preloading_prediction.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if DCHECK_IS_ON()
#include "base/no_destructor.h"
#endif  // DCHECK_IS_ON()

namespace content {

namespace {
// Called by `AddPreloadingAttempt` and `AddPreloadingPrediction`. Fails
// the callers if the predictor is redefined. This method can be racy due to the
// static variable.
static void CheckPreloadingPredictorValidity(PreloadingPredictor predictor) {
#if DCHECK_IS_ON()
  // Use `std::string` because we can't guarantee std::string_view has a static
  // lifetime.
  static base::NoDestructor<std::vector<std::pair<int64_t, std::string>>>
      seen_predictors;
  std::pair<int64_t, std::string> new_predictor(predictor.ukm_value(),
                                                predictor.name());
  bool found = false;
  for (const auto& seen : *seen_predictors) {
    if ((seen.first == new_predictor.first) ^
        (seen.second == new_predictor.second)) {
      // We cannot have two `PreloadingPredictor`s that only differ in either
      // the ukm int value or the string description - each new
      // `PreloadingPredictor` needs to be unique in both.
      DCHECK(false) << new_predictor.second << "/" << new_predictor.first
                    << " vs " << seen.second << "/" << seen.first;
    } else if (seen == new_predictor) {
      found = true;
      break;
    }
  }
  if (!found) {
    seen_predictors->push_back(new_predictor);
  }
#endif  // DCHECK_IS_ON()
}

size_t GetMaxPredictions(bool max_predictions_is_ten_for_testing) {
  // The limit is somewhat arbitrary. It should be large enough that most pages
  // won't reach it, but small enough to keep memory usage reasonable for those
  // that do.
  constexpr size_t kMaxPredictions = 10000;
  return max_predictions_is_ten_for_testing ? 10 : kMaxPredictions;
}

std::optional<double> GetSamplingLikelihood(
    bool max_predictions_is_ten_for_testing,
    size_t total_seen_predictions) {
  const size_t max_predictions =
      GetMaxPredictions(max_predictions_is_ten_for_testing);
  return (total_seen_predictions <= max_predictions)
             ? std::nullopt
             : std::optional<double>{static_cast<double>(max_predictions) /
                                     total_seen_predictions};
}

// We may produce a large number of predictions over the lifetime of a long
// lived page. After the number of predictions grows sufficiently large, we'll
// start randomly sampling and replacing existing predictions in order to limit
// memory usage.
// See https://en.wikipedia.org/wiki/Reservoir_sampling#Simple:_Algorithm_R
// We don't report anything from the predictions that aren't sampled in here
// when the page navigates/unloads. When we record UKMs, we include the
// sampling factor which indicates how much downsampling happened here.
template <typename PredictionType>
void PredictionReservoirSample(std::vector<PredictionType>& predictions,
                               size_t& items_seen,
                               bool max_predictions_is_ten_for_testing,
                               PredictionType new_prediction) {
  CHECK_LE(predictions.size(), items_seen);

  const size_t max_predictions =
      GetMaxPredictions(max_predictions_is_ten_for_testing);

  if (items_seen < max_predictions) {
    predictions.push_back(std::move(new_prediction));
    ++items_seen;
    return;
  }

  size_t replace_idx = static_cast<size_t>(base::RandGenerator(items_seen + 1));
  if (replace_idx < predictions.size()) {
    predictions[replace_idx] = std::move(new_prediction);
  }
  ++items_seen;
}

}  // namespace

// static
PreloadingURLMatchCallback PreloadingData::GetSameURLMatcher(
    const GURL& destination_url) {
  // To save memory, only bind a hash of the URL for comparison.
  return base::BindRepeating(
      [](size_t predicted_url_hash, const GURL& navigated_url) {
        return predicted_url_hash == base::FastHash(navigated_url.spec());
      },
      base::FastHash(destination_url.spec()));
}

// static
PreloadingURLMatchCallback PreloadingDataImpl::GetPrefetchServiceMatcher(
    PrefetchService& prefetch_service,
    const PrefetchContainer::Key& predicted) {
  return base::BindRepeating(
      [](base::WeakPtr<PrefetchService> prefetch_service,
         const PrefetchContainer::Key& predicted, const GURL& navigated_url) {
        if (!prefetch_service) {
          return predicted.url() == navigated_url;
        }
        if (predicted.url() == navigated_url) {
          return true;
        }

        base::WeakPtr<PrefetchContainer> prefetch_container =
            prefetch_service->MatchUrl(predicted.WithNewUrl(navigated_url));
        return prefetch_container && prefetch_container->key() == predicted;
      },
      prefetch_service.GetWeakPtr(), predicted);
}

// static
PreloadingData* PreloadingData::GetOrCreateForWebContents(
    WebContents* web_contents) {
  return PreloadingDataImpl::GetOrCreateForWebContents(web_contents);
}

// static
PreloadingData* PreloadingData::GetForWebContents(WebContents* web_contents) {
  return PreloadingDataImpl::FromWebContents(web_contents);
}

// static
PreloadingDataImpl* PreloadingDataImpl::GetOrCreateForWebContents(
    WebContents* web_contents) {
  auto* preloading_impl = PreloadingDataImpl::FromWebContents(web_contents);
  if (!preloading_impl)
    PreloadingDataImpl::CreateForWebContents(web_contents);

  return PreloadingDataImpl::FromWebContents(web_contents);
}

PreloadingAttempt* PreloadingDataImpl::AddPreloadingAttempt(
    PreloadingPredictor predictor,
    PreloadingType preloading_type,
    PreloadingURLMatchCallback url_match_predicate,
    std::optional<PreloadingType> planned_max_preloading_type,
    ukm::SourceId triggering_primary_page_source_id) {
  // The same `predictor` created and enacted the candidate associated with this
  // attempt.
  return AddPreloadingAttempt(predictor, predictor, preloading_type,
                              std::move(url_match_predicate),
                              std::move(planned_max_preloading_type),
                              triggering_primary_page_source_id);
}

PreloadingAttemptImpl* PreloadingDataImpl::AddPreloadingAttempt(
    const PreloadingPredictor& creating_predictor,
    const PreloadingPredictor& enacting_predictor,
    PreloadingType preloading_type,
    PreloadingURLMatchCallback url_match_predicate,
    std::optional<PreloadingType> planned_max_preloading_type,
    ukm::SourceId triggering_primary_page_source_id) {
  auto attempt = std::make_unique<PreloadingAttemptImpl>(
      creating_predictor, enacting_predictor, preloading_type,
      triggering_primary_page_source_id, std::move(url_match_predicate),
      std::move(planned_max_preloading_type), sampling_seed_);
  preloading_attempts_.push_back(std::move(attempt));

  return preloading_attempts_.back().get();
}

void PreloadingDataImpl::AddPreloadingPrediction(
    PreloadingPredictor predictor,
    int confidence,
    PreloadingURLMatchCallback url_match_predicate,
    ukm::SourceId triggering_primary_page_source_id) {
  AddPreloadingPrediction(predictor, PreloadingConfidence{confidence},
                          std::move(url_match_predicate),
                          triggering_primary_page_source_id);
}

void PreloadingDataImpl::AddPreloadingPrediction(
    const PreloadingPredictor& predictor,
    PreloadingConfidence confidence,
    PreloadingURLMatchCallback url_match_predicate,
    ukm::SourceId triggering_primary_page_source_id) {
  // We want to log the metrics for user visible primary pages to measure the
  // impact of PreloadingPredictions on the page user is viewing.
  // TODO(crbug.com/40227283): Extend this for non-primary page and inner
  // WebContents preloading predictions.
  PredictionReservoirSample(
      preloading_predictions_, total_seen_preloading_predictions_,
      max_predictions_is_ten_for_testing_,
      PreloadingPrediction{predictor, confidence,
                           triggering_primary_page_source_id,
                           std::move(url_match_predicate)});
}

void PreloadingDataImpl::AddExperimentalPreloadingPrediction(
    std::string_view name,
    PreloadingURLMatchCallback url_match_predicate,
    float score,
    float min_score,
    float max_score,
    size_t buckets) {
  PredictionReservoirSample(
      experimental_predictions_, total_seen_experimental_predictions_,
      max_predictions_is_ten_for_testing_,
      ExperimentalPreloadingPrediction{name, std::move(url_match_predicate),
                                       score, min_score, max_score, buckets});
}

void PreloadingDataImpl::SetIsNavigationInDomainCallback(
    PreloadingPredictor predictor,
    PredictorDomainCallback is_navigation_in_domain_callback) {
  is_navigation_in_predictor_domain_callbacks_.insert(
      {predictor, std::move(is_navigation_in_domain_callback)});
}

void PreloadingDataImpl::CopyPredictorDomains(
    const PreloadingDataImpl& other,
    const std::vector<PreloadingPredictor>& predictors) {
  for (const auto& predictor : predictors) {
    if (const auto it =
            other.is_navigation_in_predictor_domain_callbacks_.find(predictor);
        it != other.is_navigation_in_predictor_domain_callbacks_.end()) {
      SetIsNavigationInDomainCallback(predictor, it->second);
    }
  }
}

PreloadingDataImpl::PreloadingDataImpl(WebContents* web_contents)
    : WebContentsUserData<PreloadingDataImpl>(*web_contents),
      WebContentsObserver(web_contents),
      sampling_seed_(static_cast<uint32_t>(base::RandUint64())) {}

PreloadingDataImpl::~PreloadingDataImpl() = default;

void PreloadingDataImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Record UKMs for primary page navigations only. The reason we don't use
  // WebContentsObserver::PrimaryPageChanged is because we want to get the
  // navigation UkmSourceId which is different from
  // RenderFrameHost::GetPageUkmSourceId for prerender activation.
  // TODO(crbug.com/40215894): Switch to PrimaryPageChanged once we align
  // RenderFrameHost::GetPageUkmSourceId with
  // PageLoadTracker::GetPageUKMSourceId.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  ukm::SourceId navigated_page_source_id =
      navigation_handle->GetNextPageUkmSourceId();

  // Log the metrics also on navigation when the user ends up navigating. Please
  // note that we currently log the metrics on the primary page to analyze
  // preloading impact on user-visible primary pages.
  RecordMetricsForPreloadingAttempts(navigated_page_source_id);
  RecordUKMForPreloadingPredictions(navigated_page_source_id);

  // Delete the user data after logging.
  web_contents()->RemoveUserData(UserDataKey());
}

void PreloadingDataImpl::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Only observe for the navigation in the primary frame tree to log the
  // metrics after which this class will be deleted.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Ignore same-document navigations as preloading is not served for these
  // cases.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // Match the preloading based on the URL the frame is navigating to rather
  // than the committed URL as they could be different because of redirects. We
  // set accurate triggering and prediction bits in DidStartNavigation before
  // PrimaryPageChanged is invoked where the metrics are logged to capture if
  // the prediction/triggering was accurate. This doesn't imply that the user
  // navigated to the predicted URL.
  ResetRecallStats();
  SetIsAccurateTriggeringAndPrediction(navigation_handle->GetURL());
  RecordRecallStatsToUMA(navigation_handle);
}

void PreloadingDataImpl::WebContentsDestroyed() {
  // Log the metrics also on WebContentsDestroyed event to avoid losing the data
  // in case the user doesn't end up navigating. When the WebContents is
  // destroyed before navigation, we pass ukm::kInvalidSourceId and empty URL to
  // avoid the UKM associated to wrong page.
  RecordMetricsForPreloadingAttempts(ukm::kInvalidSourceId);
  RecordUKMForPreloadingPredictions(ukm::kInvalidSourceId);

  for (const auto& experimental_prediction : experimental_predictions_) {
    experimental_prediction.RecordToUMA();
  }
  experimental_predictions_.clear();
  total_seen_experimental_predictions_ = 0;

  const std::optional<double> sampling_likelihood = GetSamplingLikelihood(
      max_predictions_is_ten_for_testing_, total_seen_ml_predictions_);
  for (auto& ml_prediction : ml_predictions_) {
    ml_prediction.Record(sampling_likelihood);
  }
  ml_predictions_.clear();
  total_seen_ml_predictions_ = 0;

  // Delete the user data after logging.
  web_contents()->RemoveUserData(UserDataKey());
}

void PreloadingDataImpl::RecordPreloadingAttemptPrecisionToUMA(
    const PreloadingAttemptImpl& attempt) {
  bool is_true_positive = attempt.IsAccurateTriggering();

  for (const auto& predictor : attempt.GetPredictors()) {
    const auto uma_attempt_precision = base::StrCat(
        {"Preloading.", PreloadingTypeToString(attempt.preloading_type()),
         ".Attempt.", predictor.name(), ".Precision"});

    base::UmaHistogramEnumeration(
        uma_attempt_precision, is_true_positive
                                   ? PredictorConfusionMatrix::kTruePositive
                                   : PredictorConfusionMatrix::kFalsePositive);
  }
}

void PreloadingDataImpl::RecordPredictionPrecisionToUMA(
    const PreloadingPrediction& prediction) {
  bool is_true_positive = prediction.IsAccuratePrediction();
  const auto uma_predictor_precision =
      base::StrCat({"Preloading.Predictor.", prediction.predictor_type().name(),
                    ".Precision"});
  base::UmaHistogramEnumeration(uma_predictor_precision,
                                is_true_positive
                                    ? PredictorConfusionMatrix::kTruePositive
                                    : PredictorConfusionMatrix::kFalsePositive);
}

void PreloadingDataImpl::UpdatePreloadingAttemptRecallStats(
    const PreloadingAttemptImpl& attempt) {
  bool is_true_positive = attempt.IsAccurateTriggering();
  if (is_true_positive) {
    for (const auto& predictor : attempt.GetPredictors()) {
      preloading_attempt_recall_stats_.insert(
          {predictor, attempt.preloading_type()});
    }
  }
}
void PreloadingDataImpl::UpdatePredictionRecallStats(
    const PreloadingPrediction& prediction) {
  bool is_true_positive = prediction.IsAccuratePrediction();
  if (is_true_positive) {
    predictions_recall_stats_.insert(prediction.predictor_type());
  }
}

void PreloadingDataImpl::ResetRecallStats() {
  predictions_recall_stats_.clear();
  preloading_attempt_recall_stats_.clear();
}

void PreloadingDataImpl::RecordRecallStatsToUMA(
    NavigationHandle* navigation_handle) {
  constexpr PreloadingType kPreloadingTypes[] = {PreloadingType::kPreconnect,
                                                 PreloadingType::kPrefetch,
                                                 PreloadingType::kPrerender};
  for (const auto& [predictor_type, is_navigation_in_domain_callback] :
       is_navigation_in_predictor_domain_callbacks_) {
    if (!is_navigation_in_domain_callback.Run(navigation_handle)) {
      continue;
    }
    const auto uma_predictor_recall = base::StrCat(
        {"Preloading.Predictor.", predictor_type.name(), ".Recall"});
    base::UmaHistogramEnumeration(
        uma_predictor_recall, predictions_recall_stats_.contains(predictor_type)
                                  ? PredictorConfusionMatrix::kTruePositive
                                  : PredictorConfusionMatrix::kFalseNegative);

    for (const auto& preloading_type : kPreloadingTypes) {
      const auto uma_attemp_recall =
          base::StrCat({"Preloading.", PreloadingTypeToString(preloading_type),
                        ".Attempt.", predictor_type.name(), ".Recall"});
      base::UmaHistogramEnumeration(
          uma_attemp_recall, preloading_attempt_recall_stats_.contains(
                                 {predictor_type, preloading_type})
                                 ? PredictorConfusionMatrix::kTruePositive
                                 : PredictorConfusionMatrix::kFalseNegative);
    }
  }
  // Clear registered predictor domain callbacks and get ready for the next
  // navigation.
  is_navigation_in_predictor_domain_callbacks_.clear();
}

void PreloadingDataImpl::SetIsAccurateTriggeringAndPrediction(
    const GURL& navigated_url) {
  for (auto& experimental_prediction : experimental_predictions_) {
    experimental_prediction.SetIsAccuratePrediction(navigated_url);
    experimental_prediction.RecordToUMA();
  }
  experimental_predictions_.clear();
  total_seen_experimental_predictions_ = 0;

  const std::optional<double> sampling_likelihood = GetSamplingLikelihood(
      max_predictions_is_ten_for_testing_, total_seen_ml_predictions_);
  for (auto& ml_prediction : ml_predictions_) {
    ml_prediction.SetIsAccuratePrediction(navigated_url);
    ml_prediction.Record(sampling_likelihood);
  }
  ml_predictions_.clear();
  total_seen_ml_predictions_ = 0;

  for (auto& attempt : preloading_attempts_) {
    attempt->SetIsAccurateTriggering(navigated_url);
    RecordPreloadingAttemptPrecisionToUMA(*attempt);
    UpdatePreloadingAttemptRecallStats(*attempt);
  }

  for (auto& prediction : preloading_predictions_) {
    prediction.SetIsAccuratePrediction(navigated_url);
    RecordPredictionPrecisionToUMA(prediction);
    UpdatePredictionRecallStats(prediction);
  }
}

void PreloadingDataImpl::SetHasSpeculationRulesPrerender() {
  has_speculation_rules_prerender_ = true;
}
bool PreloadingDataImpl::HasSpeculationRulesPrerender() {
  return has_speculation_rules_prerender_;
}

void PreloadingDataImpl::OnPreloadingHeuristicsModelInput(
    const GURL& url,
    ModelPredictionTrainingData::OutcomeCallback on_record_outcome) {
  PredictionReservoirSample(
      ml_predictions_, total_seen_ml_predictions_,
      max_predictions_is_ten_for_testing_,
      ModelPredictionTrainingData{std::move(on_record_outcome),
                                  GetSameURLMatcher(url)});
}

void PreloadingDataImpl::RecordMetricsForPreloadingAttempts(
    ukm::SourceId navigated_page_source_id) {
  for (auto& attempt : preloading_attempts_) {
    // Check the validity at the time of UKMs reporting, as the UKMs are
    // reported from the same thread (whichever thread calls
    // `PreloadingDataImpl::WebContentsDestroyed` or
    // `PreloadingDataImpl::DidFinishNavigation`).
    for (const auto& predictor : attempt->GetPredictors()) {
      CheckPreloadingPredictorValidity(predictor);
    }
    attempt->RecordPreloadingAttemptMetrics(navigated_page_source_id);
  }

  // Clear all records once we record the UKMs.
  preloading_attempts_.clear();
}

void PreloadingDataImpl::RecordUKMForPreloadingPredictions(
    ukm::SourceId navigated_page_source_id) {
  const std::optional<double> sampling_likelihood = GetSamplingLikelihood(
      max_predictions_is_ten_for_testing_, total_seen_preloading_predictions_);
  for (auto& prediction : preloading_predictions_) {
    // Check the validity at the time of UKMs reporting, as the UKMs are
    // reported from the same thread (whichever thread calls
    // `PreloadingDataImpl::WebContentsDestroyed` or
    // `PreloadingDataImpl::DidFinishNavigation`).
    CheckPreloadingPredictorValidity(prediction.predictor_type());
    prediction.RecordPreloadingPredictionUKMs(navigated_page_source_id,
                                              sampling_likelihood);
  }

  // Clear all records once we record the UKMs.
  preloading_predictions_.clear();
  total_seen_preloading_predictions_ = 0;
}

size_t PreloadingDataImpl::GetPredictionsSizeForTesting() const {
  return preloading_predictions_.size();
}

void PreloadingDataImpl::SetMaxPredictionsToTenForTesting() {
  max_predictions_is_ten_for_testing_ = true;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreloadingDataImpl);

}  // namespace content
