// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENTERPRISE_POLICY_CHECKER_IOS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENTERPRISE_POLICY_CHECKER_IOS_H_

#import "base/callback_list.h"
#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/policy/core/browser/url_list/url_blocklist_manager.h"
#import "components/prefs/pref_change_registrar.h"

class GURL;
class PrefService;

namespace actor {

// Outcome of evaluating a URL against the Actor enterprise policies.
enum class EnterprisePolicyUrlBlockReason {
  // The URL is not matched by enterprise allow/block lists and the default
  // policy allows actuation; defer to downstream checks (e.g. safety list).
  kNotBlocked,
  // The URL is explicitly on the enterprise allowlist.
  kExplicitlyAllowed,
  // The URL is explicitly on the enterprise blocklist, or the default policy
  // disables actuation and the URL is not on the allowlist.
  kExplicitlyBlocked,
};

// Evaluates enterprise policies (`GeminiActOnWebSettings`,
// `GeminiActOnWebAllowedForURLs`, and `GeminiActOnWebBlockedForURLs`) for
// Actor tasks on iOS.
class ActorEnterprisePolicyCheckerIOS {
 public:
  using CanActOnWebChangedCallback = base::RepeatingCallback<void(bool)>;

  explicit ActorEnterprisePolicyCheckerIOS(PrefService* pref_service);
  ~ActorEnterprisePolicyCheckerIOS();

  ActorEnterprisePolicyCheckerIOS(const ActorEnterprisePolicyCheckerIOS&) =
      delete;
  ActorEnterprisePolicyCheckerIOS& operator=(
      const ActorEnterprisePolicyCheckerIOS&) = delete;

  // Returns whether Actor is allowed to act on the web in general (either
  // enabled by default, or disabled by default with a non-empty URL allowlist).
  bool CanActOnWeb() const;

  // Evaluates whether the Actor is allowed or blocked from actuating on `url`
  // according to the enterprise policies.
  EnterprisePolicyUrlBlockReason Evaluate(const GURL& url) const;

  // Registers `callback` to be notified whenever `CanActOnWeb()` changes due to
  // a dynamic policy update.
  base::CallbackListSubscription AddCanActOnWebChangedCallback(
      CanActOnWebChangedCallback callback);

 private:
  // Recomputes `CanActOnWeb()` and notifies subscribers if the value changed.
  void OnPolicyUpdated();

  // Returns the current effective value of `CanActOnWeb()` from
  // `pref_service_`.
  bool ComputeCanActOnWeb() const;

  raw_ptr<PrefService> pref_service_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;
  policy::URLBlocklistManager url_blocklist_manager_;
  base::CallbackListSubscription url_blocklist_subscription_;

  bool cached_can_act_on_web_ = true;
  base::RepeatingCallbackList<void(bool)> can_act_on_web_callbacks_;

  base::WeakPtrFactory<ActorEnterprisePolicyCheckerIOS> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_ACTOR_ENTERPRISE_POLICY_CHECKER_IOS_H_
