// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ORIGIN_GATING_CHECKER_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ORIGIN_GATING_CHECKER_DELEGATE_IOS_H_

#include "base/memory/weak_ptr.h"
#include "components/origin_gating/core/origin_gating_checker.h"

namespace actor {

// iOS implementation of OriginGatingChecker::Delegate
class ActorOriginGatingCheckerDelegateIOS
    : public origin_gating::OriginGatingChecker::Delegate {
 public:
  ActorOriginGatingCheckerDelegateIOS();
  ~ActorOriginGatingCheckerDelegateIOS() override;

  ActorOriginGatingCheckerDelegateIOS(
      const ActorOriginGatingCheckerDelegateIOS&) = delete;
  ActorOriginGatingCheckerDelegateIOS& operator=(
      const ActorOriginGatingCheckerDelegateIOS&) = delete;

  // OriginGatingChecker::Delegate overrides:
  void DoesOriginRequireUserConfirmation(
      origin_gating::GatingDecisionContext* context,
      origin_gating::GateableEvent event,
      const GURL& source,
      const GURL& destination,
      DoesOriginRequireUserConfirmationCallback callback) const override;
  void EvaluateEnterprisePolicy(
      const GURL& destination,
      EvaluateEnterprisePolicyCallback callback) const override;
  void OnNoVerdict(origin_gating::GatingDecisionContext* context,
                   origin_gating::GateableEvent event,
                   const GURL& source,
                   const GURL& destination,
                   bool requires_user_confirmation,
                   base::OnceCallback<void(NoVerdictResult)> callback) override;

 private:
  base::WeakPtrFactory<ActorOriginGatingCheckerDelegateIOS> weak_ptr_factory_{
      this};
};
}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ORIGIN_GATING_CHECKER_DELEGATE_IOS_H_
