// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ORIGIN_GATING_CHECKER_DELEGATE_IOS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ORIGIN_GATING_CHECKER_DELEGATE_IOS_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/origin_gating/core/client_tool.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"

namespace actor {

// Custom predicates used by the Actor service origin gating checker.
enum class ActorCustomPredicate {
  kSafetyList,
};

class ActorEnterprisePolicyCheckerIOS;

// iOS implementation of OriginGatingChecker::Delegate
class ActorOriginGatingCheckerDelegateIOS
    : public origin_gating::OriginGatingChecker::Delegate {
 public:
  explicit ActorOriginGatingCheckerDelegateIOS(
      const ActorEnterprisePolicyCheckerIOS* policy_checker = nullptr);
  ~ActorOriginGatingCheckerDelegateIOS() override;

  ActorOriginGatingCheckerDelegateIOS(
      const ActorOriginGatingCheckerDelegateIOS&) = delete;
  ActorOriginGatingCheckerDelegateIOS& operator=(
      const ActorOriginGatingCheckerDelegateIOS&) = delete;

  base::WeakPtr<ActorOriginGatingCheckerDelegateIOS> GetWeakPtr();

  // OriginGatingChecker::Delegate overrides:
  void DoesOriginRequireUserConfirmation(
      origin_gating::GatingDecisionContext* context,
      const origin_gating::GateableEvent& event,
      DoesOriginRequireUserConfirmationCallback callback) const override;
  void EvaluateEnterprisePolicy(
      const GURL& destination,
      EvaluateEnterprisePolicyCallback callback) const override;
  void OnNoVerdict(origin_gating::GatingDecisionContext* context,
                   const origin_gating::GateableEvent& event,
                   bool requires_user_confirmation,
                   base::OnceCallback<void(NoVerdictResult)> callback) override;

 private:
  // Pointer to the policy checker that holds enterprise policy state for Actor.
  raw_ptr<const ActorEnterprisePolicyCheckerIOS> policy_checker_ = nullptr;

  base::WeakPtrFactory<ActorOriginGatingCheckerDelegateIOS> weak_ptr_factory_{
      this};
};
}  // namespace actor

template <>
inline const origin_gating::CustomPredicateDomain origin_gating::
    CustomPredicateDomain::kInstance<actor::ActorCustomPredicate> = {};

template <>
inline const origin_gating::ToolDomain
    origin_gating::ToolDomain::kInstance<actor::ToolType> = {};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ORIGIN_GATING_CHECKER_DELEGATE_IOS_H_
