// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_origin_gating_checker_delegate_ios.h"

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/callback.h"
#import "components/origin_gating/core/decision.h"
#import "components/origin_gating/core/gateable_event.h"
#import "components/origin_gating/core/gating_decision.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_enterprise_policy_checker_ios.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace actor {

ActorOriginGatingCheckerDelegateIOS::ActorOriginGatingCheckerDelegateIOS(
    const ActorEnterprisePolicyCheckerIOS* policy_checker)
    : policy_checker_(policy_checker) {}

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
  // Parity with Desktop: Navigation requests and in-page actions never prompt
  // the user.
  // (https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/execution_engine.cc;l=982;drc=5302cb43248b6963cf081a371207f808e73c3d77)
  if (event.is_navigation_request() || event.is_page_action()) {
    std::move(callback).Run(/*requires_user_confirmation=*/false);
    return;
  }

  CHECK(event.is_navigation_response());

  // Cross-origin transitions require confirmation.
  const url::Origin source_origin =
      url::Origin::Create(CHECK_DEREF(event.source()));
  const url::Origin dest_origin = url::Origin::Create(event.destination());
  const bool requires_confirmation =
      !source_origin.IsSameOriginWith(dest_origin);
  std::move(callback).Run(requires_confirmation);
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

  // When confirmation is requried but iOS UI prompt support is pending, block
  // by default.
  if (requires_user_confirmation) {
    // TODO(crbug.com/556689818): Trigger iOS user confirmation prompt dialog
    std::move(callback).Run(
        {.is_allowed = false, .did_prompt_user = false, .bypass_cache = false});
    return;
  }

  std::move(callback).Run(
      {.is_allowed = true, .did_prompt_user = false, .bypass_cache = false});
}
}  // namespace actor
