// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_origin_gating_checker_delegate_ios.h"

#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/origin_gating/core/decision.h"
#import "components/origin_gating/core/gateable_event.h"
#import "components/origin_gating/core/gating_decision.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_enterprise_policy_checker_ios.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace actor {

ActorOriginGatingCheckerDelegateIOS::ActorOriginGatingCheckerDelegateIOS(
    const ActorEnterprisePolicyCheckerIOS* policy_checker,
    NavigationConfirmationCallback confirm_navigation_callback,
    UserConfirmationDialogCallback user_confirmation_callback)
    : policy_checker_(policy_checker),
      confirm_navigation_callback_(std::move(confirm_navigation_callback)),
      user_confirmation_callback_(std::move(user_confirmation_callback)) {}

ActorOriginGatingCheckerDelegateIOS::~ActorOriginGatingCheckerDelegateIOS() =
    default;

base::WeakPtr<ActorOriginGatingCheckerDelegateIOS>
ActorOriginGatingCheckerDelegateIOS::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ActorOriginGatingCheckerDelegateIOS::DoesOriginRequireUserConfirmation(
    origin_gating::GatingDecisionContext* context,
    const origin_gating::GateableEvent& event,
    DoesOriginRequireUserConfirmationCallback callback) const {
  // Same-origin responses are handled by `DecisionSource::kAllowSameOrigin`,
  // and uncached cross-origin responses are evaluated in `OnNoVerdict()`.
  // Return false until iOS wires up a sensitive-URL check so cached origins can
  // match `DecisionSource::kCacheWithoutUserConfirmation`.
  std::move(callback).Run(/*requires_user_confirmation=*/false);
}

void ActorOriginGatingCheckerDelegateIOS::EvaluateEnterprisePolicy(
    const GURL& destination,
    EvaluateEnterprisePolicyCallback callback) const {
  // Parity with Desktop when unmanaged: return kNoDecision.
  // (https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/execution_engine.cc;l=1000;drc=5302cb43248b6963cf081a371207f808e73c3d77)
  if (!policy_checker_) {
    std::move(callback).Run(
        DecisionWithMetadata{.decision = origin_gating::Decision::kNoDecision,
                             .bypass_cache = true});
    return;
  }
  origin_gating::Decision decision = origin_gating::Decision::kNoDecision;
  switch (policy_checker_->Evaluate(destination)) {
    case EnterprisePolicyUrlBlockReason::kNotBlocked:
      decision = origin_gating::Decision::kNoDecision;
      break;
    case EnterprisePolicyUrlBlockReason::kExplicitlyAllowed:
      decision = origin_gating::Decision::kAllowed;
      break;
    case EnterprisePolicyUrlBlockReason::kExplicitlyBlocked:
      decision = origin_gating::Decision::kBlocked;
      break;
  }

  std::move(callback).Run(
      DecisionWithMetadata{.decision = decision, .bypass_cache = true});
}

void ActorOriginGatingCheckerDelegateIOS::OnNoVerdict(
    origin_gating::GatingDecisionContext* context,
    const origin_gating::GateableEvent& event,
    bool requires_user_confirmation,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  // Parity with Desktop: Navigation requests fail open so the response can be
  // inspected.
  // (https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/execution_engine.cc;l=1018;drc=5302cb43248b6963cf081a371207f808e73c3d77)
  if (event.is_navigation_request()) {
    std::move(callback).Run(
        {.is_allowed = true, .did_prompt_user = false, .bypass_cache = true});
    return;
  }

  const url::Origin destination_origin =
      url::Origin::Create(event.destination());

  if (requires_user_confirmation) {
    RequestUserConfirmation(destination_origin, std::move(callback));
    return;
  }

  // Allow page actions on non-blocked origins so `OriginGatingChecker` seeds
  // the current tab's origin into `OriginGatingCache`.
  if (event.is_page_action()) {
    std::move(callback).Run(
        {.is_allowed = true, .did_prompt_user = false, .bypass_cache = false});
    return;
  }

  CHECK(event.is_navigation_response());
  if (destination_origin.opaque()) {
    std::move(callback).Run(
        {.is_allowed = false, .did_prompt_user = false, .bypass_cache = false});
    return;
  }

  // Check preconditions before evaluating the feature parameter.
  if (user_confirmation_callback_ &&
      ShouldPromptUserForActorNavigationToNewOrigins()) {
    RequestUserConfirmation(destination_origin, std::move(callback));
    return;
  }

  if (confirm_navigation_callback_) {
    confirm_navigation_callback_.Run(
        destination_origin,
        base::BindOnce(&ActorOriginGatingCheckerDelegateIOS::
                           OnNavigationConfirmationDecision,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  // Uncached cross-origin navigation responses fail closed when no confirmation
  // callback is registered.
  std::move(callback).Run(
      {.is_allowed = false, .did_prompt_user = false, .bypass_cache = false});
}

void ActorOriginGatingCheckerDelegateIOS::RequestUserConfirmation(
    const url::Origin& destination_origin,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  if (!user_confirmation_callback_ || destination_origin.opaque()) {
    std::move(callback).Run(
        {.is_allowed = false, .did_prompt_user = false, .bypass_cache = false});
    return;
  }

  user_confirmation_callback_.Run(
      destination_origin,
      base::BindOnce(
          &ActorOriginGatingCheckerDelegateIOS::OnUserConfirmationDecision,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorOriginGatingCheckerDelegateIOS::OnUserConfirmationDecision(
    base::OnceCallback<void(NoVerdictResult)> callback,
    bool permission_granted) {
  // `did_prompt_user = true` causes `OriginGatingChecker::OnNoVerdictAnswer`
  // to cache approved origins under
  // `DecisionSource::kCacheWithUserConfirmation` (ignored when `is_allowed` is
  // false).
  std::move(callback).Run({.is_allowed = permission_granted,
                           .did_prompt_user = true,
                           .bypass_cache = false});
}

void ActorOriginGatingCheckerDelegateIOS::OnNavigationConfirmationDecision(
    base::OnceCallback<void(NoVerdictResult)> callback,
    bool permission_granted) {
  std::move(callback).Run({.is_allowed = permission_granted,
                           .did_prompt_user = false,
                           .bypass_cache = false});
}
}  // namespace actor
