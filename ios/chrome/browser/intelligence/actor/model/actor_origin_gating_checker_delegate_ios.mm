// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_origin_gating_checker_delegate_ios.h"

#import "base/functional/callback.h"
#import "components/origin_gating/core/types.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace actor {

ActorOriginGatingCheckerDelegateIOS::ActorOriginGatingCheckerDelegateIOS() =
    default;

ActorOriginGatingCheckerDelegateIOS::~ActorOriginGatingCheckerDelegateIOS() =
    default;

void ActorOriginGatingCheckerDelegateIOS::DoesOriginRequireUserConfirmation(
    origin_gating::GatingDecisionContext* context,
    origin_gating::GateableEvent event,
    const GURL& source,
    const GURL& destination,
    DoesOriginRequireUserConfirmationCallback callback) const {
  // Parity with Desktop: Navigation requests never prompt the user.
  // (https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/execution_engine.cc;l=982;drc=5302cb43248b6963cf081a371207f808e73c3d77)
  if (event == origin_gating::GateableEvent::kNavigationRequest) {
    std::move(callback).Run(/*requires_user_confirmation=*/false);
    return;
  }

  // Cross-origin transitions require confirmation.
  const url::Origin source_origin = url::Origin::Create(source);
  const url::Origin dest_origin = url::Origin::Create(destination);
  const bool requires_confirmation =
      !source_origin.IsSameOriginWith(dest_origin);
  std::move(callback).Run(requires_confirmation);
}

void ActorOriginGatingCheckerDelegateIOS::EvaluateEnterprisePolicy(
    const GURL& destination,
    EvaluateEnterprisePolicyCallback callback) const {
  // Parity with Desktop when unmanaged: return kNoDecision.
  // (https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/execution_engine.cc;l=1000;drc=5302cb43248b6963cf081a371207f808e73c3d77)
  // TODO(crbug.com/556689100): Implement enterprise policy evaluation for iOS.
  std::move(callback).Run(DecisionWithMetadata{
      .decision = origin_gating::Decision::kNoDecision, .bypass_cache = true});
}

void ActorOriginGatingCheckerDelegateIOS::OnNoVerdict(
    origin_gating::GatingDecisionContext* context,
    origin_gating::GateableEvent event,
    const GURL& source,
    const GURL& destination,
    bool requires_user_confirmation,
    base::OnceCallback<void(NoVerdictResult)> callback) {
  // Parity with Desktop: Navigation requests fail open so the response can be
  // inspected.
  // (https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/actor/execution_engine.cc;l=1018;drc=5302cb43248b6963cf081a371207f808e73c3d77)
  if (event == origin_gating::GateableEvent::kNavigationRequest) {
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
