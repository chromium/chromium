// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_FAST_FETCH_MANAGER_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_FAST_FETCH_MANAGER_H_

#include <memory>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace network {
struct URLLoaderCompletionStatus;
}

namespace content {

class NavigationRequest;

// NavigationFastFetchManager manages the dry run of the Fast Fetch feature.
// It determines the eligibility of a navigation, monitors its lifecycle,
// and records metrics to measure the potential benefit of pre-fetching
// the document resource before the URLLoader is fully started.
//
// An instance of this class is created for each main-frame navigation
// and is owned by NavigationRequest.
class CONTENT_EXPORT NavigationFastFetchManager {
 public:
  // Reasons for eligibility or ineligibility.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(EligibilityReason)
  enum class EligibilityReason {
    kEligible = 0,
    kNotInOutermostMainFrame = 1,
    kNotGetMethod = 2,
    kNotHttpsScheme = 3,
    kIsReload = 4,
    kIsHistory = 5,
    kDevToolsAttached = 6,
    kHasSignedExchange = 7,
    kHasServiceWorker = 8,
    kSameDocument = 9,
    kIsPrerender = 10,
    kIsPrerenderActivation = 11,
    kMaxValue = kIsPrerenderActivation,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:FastFetchEligibilityReason)

  // Final outcome of the navigation for eligible requests.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(NavigationOutcome)
  enum class NavigationOutcome {
    kCommitted = 0,
    kFailed = 1,
    kCancelled = 2,
    // TODO(crbug.com/529425553): Add redirect outcome.
    kMaxValue = kCancelled,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:FastFetchNavigationOutcome)

  // Creates a NavigationFastFetchManager instance and performs the initial
  // eligibility check.
  static std::unique_ptr<NavigationFastFetchManager> Create(
      NavigationRequest& request);

  ~NavigationFastFetchManager();

  // Called when the navigation commits successfully. Records kCommitted
  // outcome.
  void OnCommitNavigation(NavigationRequest& request);

  // Called when the navigation fails. Records kFailed outcome and related error
  // codes.
  void OnRequestFailed(NavigationRequest& request,
                       const network::URLLoaderCompletionStatus& status,
                       bool skip_throttles);

  void SuppressEligibilityReasonRecording() {
    should_record_eligibility_reason_ = false;
  }

  EligibilityReason eligibility_reason_for_testing() const {
    return eligibility_reason_;
  }

 private:
  explicit NavigationFastFetchManager(EligibilityReason eligibility_reason);

  void RecordOutcome(NavigationOutcome outcome);

  // The eligibility reason determined for this navigation.
  const EligibilityReason eligibility_reason_;

  // The time when the eligibility check was performed.
  const base::TimeTicks eligibility_check_time_;

  // Whether the eligibility reason should be recorded to UMA in the destructor.
  // Set to false in tests to avoid polluting UMA.
  bool should_record_eligibility_reason_ = true;

  // Whether the outcome (committed/failed/cancelled) has been recorded.
  // Used to ensure we record exactly one outcome, and to record 'cancelled'
  // in the destructor if no other outcome was recorded.
  bool outcome_recorded_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_FAST_FETCH_MANAGER_H_
