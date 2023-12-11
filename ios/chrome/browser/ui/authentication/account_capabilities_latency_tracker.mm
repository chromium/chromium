// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_capabilities_latency_tracker.h"

#import "base/time/time.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"

namespace signin {

namespace {

void RecordImmediateAvailability() {
  base::UmaHistogramTimes("Signin.AccountCapabilities.UserVisibleLatency",
                          base::Seconds(0));
  base::UmaHistogramBoolean("Signin.AccountCapabilities.ImmediatelyAvailable",
                            true);
}

void RecordNoImmediateAvailability() {
  base::UmaHistogramBoolean("Signin.AccountCapabilities.ImmediatelyAvailable",
                            false);
}

void RecordFetchLatency(const base::TimeDelta& latency) {
  base::UmaHistogramTimes("Signin.AccountCapabilities.UserVisibleLatency",
                          latency);
  base::UmaHistogramTimes("Signin.AccountCapabilities.FetchLatency", latency);
}

}  // namespace

AccountCapabilitiesLatencyTracker::AccountCapabilitiesLatencyTracker(
    IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  if (!HasCapability()) {
    RecordNoImmediateAvailability();
    return;
  };

  RecordImmediateAvailability();
  capabilities_already_fetched_ = true;
}

bool AccountCapabilitiesLatencyTracker::HasCapability() const {
  CoreAccountInfo primaryAccount =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo accountInfo =
      identity_manager_->FindExtendedAccountInfo(primaryAccount);
  // TODO(b/309953195): Replace with target capability. Current check is an
  // approximation.
  return accountInfo.capabilities.is_subject_to_parental_controls() !=
         signin::Tribool::kUnknown;
}

void AccountCapabilitiesLatencyTracker::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  if (capabilities_already_fetched_) {
    return;
  }
  if (!HasCapability()) {
    return;
  }

  RecordFetchLatency(timer_.Elapsed());
  capabilities_already_fetched_ = true;
}

}  // namespace signin
