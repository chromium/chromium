// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/policy_decision_state_tracker.h"

#import "base/barrier_closure.h"

namespace web {

PolicyDecisionStateTracker::PolicyDecisionStateTracker(
    WebStatePolicyDecider::PolicyDecisionCallback callback)
    : callback_(std::move(callback)) {}

PolicyDecisionStateTracker::~PolicyDecisionStateTracker() {
  if (!callback_.is_null()) {
    std::move(callback_).Run(WebStatePolicyDecider::PolicyDecision::Cancel());
  }
}

void PolicyDecisionStateTracker::OnSinglePolicyDecisionReceived(
    WebStatePolicyDecider::PolicyDecision decision) {
  if (DeterminedFinalResult())
    return;
  if (decision.ShouldCancelNavigation() && !decision.ShouldDisplayError()) {
    result_ = decision;
    OnFinalResultDetermined();
    return;
  }
  num_decisions_received_++;
  if (decision.ShouldDisplayError() && result_.ShouldAllowNavigation()) {
    result_ = decision;
  }
  decision_closure_.Run();
}

bool PolicyDecisionStateTracker::DeterminedFinalResult() {
  return callback_.is_null();
}

void PolicyDecisionStateTracker::FinishedRequestingDecisions(
    int num_decisions_requested) {
  if (DeterminedFinalResult())
    return;
  decision_closure_ = base::BarrierClosure(
      num_decisions_requested - num_decisions_received_,
      base::BindOnce(&PolicyDecisionStateTracker::OnFinalResultDetermined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PolicyDecisionStateTracker::OnFinalResultDetermined() {
  std::move(callback_).Run(result_);
}

}  // namespace web
