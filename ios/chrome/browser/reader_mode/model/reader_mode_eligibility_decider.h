// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_ELIGIBILITY_DECIDER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_ELIGIBILITY_DECIDER_H_

#import <optional>
#import <string>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "components/web_distiller/core/eligibility/eligibility_decider.h"
#import "components/web_distiller/core/eligibility/reader_mode_eligibility.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"
#import "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace web {
class WebState;
}  // namespace web

// Responsible for determining the eligibility of a web page for Reader Mode.
class ReaderModeEligibilityDecider final
    : public web_distiller::EligibilityDecider::Delegate {
 public:
  ReaderModeEligibilityDecider(web::WebState* web_state,
                               ReaderModeMetricsHelper* metrics_helper);
  ReaderModeEligibilityDecider(const ReaderModeEligibilityDecider&) = delete;
  ReaderModeEligibilityDecider& operator=(const ReaderModeEligibilityDecider&) =
      delete;
  ~ReaderModeEligibilityDecider() override;

  // Records a heuristic result produced outside of a decision, by the DOM
  // features message of ReaderModeJavaScriptFeature. It doesn't change the
  // eligibility of the current page.
  void HandleReaderModeHeuristicResult(
      web_distiller::ReaderModeEligibility result);

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

 private:
  // web_distiller::EligibilityDecider::Delegate:
  GURL GetLastCommittedURL() const override;
  bool IsContentHTML() const override;
  void ExecuteScript(
      const std::string& script,
      base::OnceCallback<void(const base::Value*)> callback) override;

  // Handles the result of a decision started for `url`. `result` is nullopt
  // when the page can't run the heuristic.
  void OnEligibilityResult(
      const GURL& url,
      std::optional<web_distiller::ReaderModeEligibility> result,
      bool is_current_url);

  // Called when the distillability of the page is known.
  void CallLastCommittedUrlEligibilityCallbacks(std::optional<bool> result);

  raw_ptr<web::WebState> web_state_ = nullptr;

  raw_ptr<ReaderModeMetricsHelper> metrics_helper_ = nullptr;

  // The URL for which reader mode is eligible.
  GURL reader_mode_eligible_url_;
  // The last committed URL without the ref.
  GURL last_committed_url_without_ref_;
  // Whether the eligibility of the last committed URL is ready.
  bool last_committed_url_eligibility_ready_ = false;

  // Callbacks waiting for the result of the distillability of the last
  // committed URL.
  std::vector<base::OnceCallback<void(std::optional<bool>)>>
      last_committed_url_eligibility_callbacks_;

  // Runs the eligibility decision. Declared last so it's destroyed first,
  // since it calls back into this object.
  web_distiller::EligibilityDecider decider_;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_ELIGIBILITY_DECIDER_H_
