// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_WEB_STATE_POLICY_DECIDER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_WEB_STATE_POLICY_DECIDER_H_

#import "base/feature_list.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/actor/public/mojom/actor_types.mojom-forward.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"

namespace origin_gating {
class OriginGatingChecker;
struct GatingDecision;
class GatingDecisionContext;
}  // namespace origin_gating

namespace actor {

// Callback invoked when a navigation is blocked by origin gating.
using NavigationBlockedCallback =
    base::RepeatingCallback<void(mojom::ActionResultCode)>;

// A policy decider that gates implicit navigations (such as link clicks and
// redirects) on WebStates controlled by the ActorTask.
class ActorWebStatePolicyDecider : public web::WebStatePolicyDecider {
 public:
  ActorWebStatePolicyDecider(
      web::WebState* web_state,
      origin_gating::OriginGatingChecker* gating_checker,
      ActorTaskId task_id,
      NavigationBlockedCallback navigation_blocked_callback);
  ~ActorWebStatePolicyDecider() override;

  ActorWebStatePolicyDecider(const ActorWebStatePolicyDecider&) = delete;
  ActorWebStatePolicyDecider& operator=(const ActorWebStatePolicyDecider&) =
      delete;

  // web::WebStatePolicyDecider:
  void ShouldAllowRequest(
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info,
      web::WebStatePolicyDecider::PolicyDecisionCallback callback) override;

 private:
  void OnGatingDecisionComputed(
      web::WebStatePolicyDecider::PolicyDecisionCallback callback,
      std::unique_ptr<origin_gating::GatingDecisionContext> context,
      origin_gating::GatingDecision decision);

  raw_ptr<origin_gating::OriginGatingChecker> gating_checker_ = nullptr;
  const ActorTaskId task_id_;
  NavigationBlockedCallback navigation_blocked_callback_;

  base::WeakPtrFactory<ActorWebStatePolicyDecider> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_WEB_STATE_POLICY_DECIDER_H_
