// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/page_transition_types.h"

namespace actor {

ActorWebStatePolicyDecider::ActorWebStatePolicyDecider(
    web::WebState* web_state,
    origin_gating::OriginGatingService* gating_service,
    origin_gating::CheckerId gating_checker_id,
    ActorTaskId task_id,
    NavigationBlockedCallback navigation_blocked_callback)
    : web::WebStatePolicyDecider(web_state),
      gating_service_(gating_service),
      gating_checker_id_(gating_checker_id),
      navigation_blocked_callback_(std::move(navigation_blocked_callback)) {}

ActorWebStatePolicyDecider::~ActorWebStatePolicyDecider() = default;

void ActorWebStatePolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  // If the feature is disabled, bypass origin gating.
  if (!IsActorOriginGatingForImplicitNavigationEnabled()) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  // Only gate primary main frame navigations.
  if (!request_info.target_frame_is_main) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  // Bypass explicit navigations initiated by NavigateTool, but only for the
  // initial request. If it redirects, we still check the redirected URL.
  if (!ui::PageTransitionIsRedirect(request_info.transition_type) &&
      ui::PageTransitionCoreTypeIs(
          request_info.transition_type,
          ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL)) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  const GURL destination_url = net::GURLWithNSURL(request.URL);
  if (!destination_url.is_valid()) {
    std::move(callback).Run(PolicyDecision::Cancel());
    return;
  }

  origin_gating::OriginGatingChecker* gating_checker =
      gating_service_ ? gating_service_->GetChecker(gating_checker_id_)
                      : nullptr;

  // Feature is enabled: checker is required. Fail closed if missing.
  if (!gating_checker) {
    std::move(callback).Run(PolicyDecision::Cancel());
    if (navigation_blocked_callback_) {
      navigation_blocked_callback_.Run(
          mojom::ActionResultCode::kTriggeredNavigationBlocked);
    }
    return;
  }

  const GURL source_url =
      web_state() ? web_state()->GetLastCommittedURL() : GURL();
  auto context = std::make_unique<origin_gating::GatingDecisionContext>();

  gating_checker->ComputeGatingDecision(
      std::move(context), origin_gating::GateableEvent::kNavigationRequest,
      source_url, destination_url,
      base::BindOnce(&ActorWebStatePolicyDecider::OnGatingDecisionComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorWebStatePolicyDecider::OnGatingDecisionComputed(
    web::WebStatePolicyDecider::PolicyDecisionCallback callback,
    std::unique_ptr<origin_gating::GatingDecisionContext> context,
    origin_gating::GatingDecision decision) {
  if (decision.is_allowed) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  // Tell WebKit to cancel the HTTP/navigation request.
  std::move(callback).Run(PolicyDecision::Cancel());

  // Notify the task/engine that the triggered navigation was blocked
  if (navigation_blocked_callback_) {
    navigation_blocked_callback_.Run(
        mojom::ActionResultCode::kTriggeredNavigationBlocked);
  }
}
}  // namespace actor
