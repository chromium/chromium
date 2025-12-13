// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_ELIGIBILITY_DECIDER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_ELIGIBILITY_DECIDER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "base/values.h"
#import "components/optimization_guide/core/hints/optimization_guide_decider.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "components/optimization_guide/core/hints/optimization_metadata.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"
#import "ios/web/public/web_state_observer.h"
#import "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

enum class ReaderModeHeuristicResult;

// Responsible for determining the eligibility of a web page for Reader Mode.
class ReaderModeEligibilityDecider {
 public:
  ReaderModeEligibilityDecider(web::WebState* web_state,
                               ReaderModeMetricsHelper* metrics_helper);
  ReaderModeEligibilityDecider(const ReaderModeEligibilityDecider&) = delete;
  ReaderModeEligibilityDecider& operator=(const ReaderModeEligibilityDecider&) =
      delete;
  ~ReaderModeEligibilityDecider();

  // Processes the result of the Reader Mode heuristic trigger.
  void HandleReaderModeHeuristicResult(ReaderModeHeuristicResult result);

  // Initiates the decision process for Reader Mode eligibility for the given
  // `url`.
  void StartDecision(const GURL& url);

  // Resets the Reader Mode eligibility decision for the given `url`.
  void ResetDecision(const GURL& url);

  // Returns true if the current page is eligible for reader mode.
  bool CurrentPageIsEligibleForReaderMode() const;

  // Returns true if the page can be distilled.
  bool CurrentPageIsDistillable() const;

  // Fetches the distillability result for the last committed URL.
  void FetchLastCommittedUrlDistillabilityResult(
      base::OnceCallback<void(std::optional<bool>)> callback);

  // Sets the last committed URL.
  void SetLastCommittedUrl(const GURL& url);

  const GURL& GetLastCommittedUrlWithoutRefForTesting() const {
    return last_committed_url_without_ref_;
  }

  const base::OneShotTimer& GetTimerForTesting() const {
    return trigger_reader_mode_timer_;
  }

 private:
  // Trigger the heuristic to determine reader mode eligibility.
  void TriggerReaderModeHeuristic(const GURL& url);

  // Handles the result from the Readability JavaScript heuristic triggering
  // logic.
  void HandleReadabilityHeuristicResult(const base::Value* result);

  // Handles the result from the Optimization guide service.
  void OnOptimizationGuideDecision(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // Performs the final operations related to the heuristic result.
  void CompleteHeuristic(ReaderModeHeuristicResult result);

  // Called when the distillability of the page is known.
  void CallLastCommittedUrlEligibilityCallbacks(std::optional<bool> result);

  base::OneShotTimer trigger_reader_mode_timer_;

  raw_ptr<web::WebState> web_state_ = nullptr;

  raw_ptr<ReaderModeMetricsHelper> metrics_helper_ = nullptr;

  // The optimization guide decider for page metadata.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;

  // The URL for which reader mode is eligible.
  GURL reader_mode_eligible_url_;
  // The last committed URL without the ref.
  GURL last_committed_url_without_ref_;
  // The URL for which the eligibility heuristic is running.
  std::optional<GURL> eligibility_heuristic_url_;
  // Whether the eligibility of the last committed URL is ready.
  bool last_committed_url_eligibility_ready_ = false;

  // Callbacks waiting for the result of the distillability of the last
  // committed URL.
  std::vector<base::OnceCallback<void(std::optional<bool>)>>
      last_committed_url_eligibility_callbacks_;

  base::WeakPtrFactory<ReaderModeEligibilityDecider> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_ELIGIBILITY_DECIDER_H_
