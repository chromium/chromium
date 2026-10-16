// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_enterprise_policy_checker_ios.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "url/gurl.h"

namespace actor {

namespace {

// Policy integer values for `prefs::kGeminiActOnWebSettings`.
constexpr int kGeminiActOnWebSettingsEnabled = 0;

}  // namespace

ActorEnterprisePolicyCheckerIOS::ActorEnterprisePolicyCheckerIOS(
    PrefService* pref_service)
    : pref_service_(pref_service),
      url_blocklist_manager_(pref_service_,
                             prefs::kGeminiActOnWebBlockedForURLs,
                             prefs::kGeminiActOnWebAllowedForURLs) {
  CHECK(pref_service_);
  cached_can_act_on_web_ = ComputeCanActOnWeb();

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kGeminiActOnWebSettings,
      base::BindRepeating(&ActorEnterprisePolicyCheckerIOS::OnPolicyUpdated,
                          weak_ptr_factory_.GetWeakPtr()));

  url_blocklist_subscription_ = url_blocklist_manager_.AddObserver(
      base::BindRepeating(&ActorEnterprisePolicyCheckerIOS::OnPolicyUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

ActorEnterprisePolicyCheckerIOS::~ActorEnterprisePolicyCheckerIOS() = default;

bool ActorEnterprisePolicyCheckerIOS::CanActOnWeb() const {
  return cached_can_act_on_web_;
}

EnterprisePolicyUrlBlockReason ActorEnterprisePolicyCheckerIOS::Evaluate(
    const GURL& url) const {
  const policy::URLBlocklist::URLBlocklistState state =
      url_blocklist_manager_.GetURLBlocklistState(url);
  if (state == policy::URLBlocklist::URL_IN_ALLOWLIST) {
    return EnterprisePolicyUrlBlockReason::kExplicitlyAllowed;
  }
  if (state == policy::URLBlocklist::URL_IN_BLOCKLIST) {
    return EnterprisePolicyUrlBlockReason::kExplicitlyBlocked;
  }

  // If the URL is neither explicitly allowed nor explicitly blocked, fall back
  // to the default `GeminiActOnWebSettings` policy.
  if (pref_service_->GetInteger(prefs::kGeminiActOnWebSettings) !=
      kGeminiActOnWebSettingsEnabled) {
    return EnterprisePolicyUrlBlockReason::kExplicitlyBlocked;
  }

  return EnterprisePolicyUrlBlockReason::kNotBlocked;
}

base::CallbackListSubscription
ActorEnterprisePolicyCheckerIOS::AddCanActOnWebChangedCallback(
    CanActOnWebChangedCallback callback) {
  return can_act_on_web_callbacks_.Add(std::move(callback));
}

#pragma mark - Private

void ActorEnterprisePolicyCheckerIOS::OnPolicyUpdated() {
  const bool can_act = ComputeCanActOnWeb();
  if (can_act == cached_can_act_on_web_) {
    return;
  }
  cached_can_act_on_web_ = can_act;
  can_act_on_web_callbacks_.Notify(cached_can_act_on_web_);
}

bool ActorEnterprisePolicyCheckerIOS::ComputeCanActOnWeb() const {
  if (pref_service_->GetInteger(prefs::kGeminiActOnWebSettings) ==
      kGeminiActOnWebSettingsEnabled) {
    return true;
  }
  // When disabled by default, actuation is still permitted on allowlisted URLs
  // if the allowlist is non-empty.
  return !pref_service_->GetList(prefs::kGeminiActOnWebAllowedForURLs).empty();
}

}  // namespace actor
