// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_POLICY_DECISION_STATE_TRACKER_H_
#define IOS_WEB_WEB_STATE_POLICY_DECISION_STATE_TRACKER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"

namespace web {

// Tracks responses received from WebStatePolicyDeciders for calls to
// shouldAllowResponse(), maintaining the current result based on decisions
// received so far, and calling a callback once all decisions have been
// received. If all decisions are PolicyDecision::Allow, the final result is
// also Allow. If at least one decision is PolicyDecision::Cancel, the final
// result is Cancel. Otherwise, if at least one decision is
// CancelAndDisplayError, the final result is also CancelAndDisplayError, with
// the error associated with the first such decision. If this is destroyed
// before all decisions have been received and the callback has not yet been
// invoked, the callback is invoked with PolicyDecision::Cancel.
class PolicyDecisionStateTracker final {
 public:
  // Constructor that takes a `callback` to be called once all decisions have
  // been received.
  PolicyDecisionStateTracker(
      WebStatePolicyDecider::PolicyDecisionCallback callback);

  ~PolicyDecisionStateTracker();

  // Called by each WebStatePolicyDecider with its `decision`.
  void OnSinglePolicyDecisionReceived(
      WebStatePolicyDecider::PolicyDecision decision);

  // Returns true if the final result has already been determined. This means
  // that either every decider has provided a decision, or the decisions
  // received so far mean that the final result won't change no matter what
  // other decisions are received.
  bool DeterminedFinalResult();

  // Called once all WebStatePolicyDeciders have been asked for a decision,
  // where `num_decisions_requested` is the number of WebStatePolicyDeciders
  // that have been asked for a decision.
  void FinishedRequestingDecisions(int num_decisions_requested);

 private:
  // Called once, after all WebStatePolicyDeciders have provided a decision or
  // after the decisions received so far have already determined the final
  // result.
  void OnFinalResultDetermined();

  // Called once with the final result.
  base::OnceCallback<void(WebStatePolicyDecider::PolicyDecision)> callback_;

  // The current result, based on the decisions received so far.
  WebStatePolicyDecider::PolicyDecision result_ =
      WebStatePolicyDecider::PolicyDecision::Allow();

  // The number of WebStatePolicyDeciders that have provided a decision.
  int num_decisions_received_ = 0;

  // Called after each decision is received. Initially, this is a no-op. Once
  // all decisions have been requested, this is changed to a BarrierClosure
  // that handles invoking `callback_` after receiving the remaining
  // outstanding decisions.
  base::RepeatingClosure decision_closure_ = base::DoNothing();

  base::WeakPtrFactory<PolicyDecisionStateTracker> weak_ptr_factory_{this};
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_POLICY_DECISION_STATE_TRACKER_H_
