// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_CAPABILITIES_LATENCY_TRACKER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_CAPABILITIES_LATENCY_TRACKER_H_

#import "base/metrics/histogram_functions.h"
#import "base/timer/elapsed_timer.h"
#import "components/signin/public/identity_manager/identity_manager.h"

namespace signin {

// Disposable utility to record the latency of the account capabilities fetch.
//
// Instances of this class verify whether the Account Capabilities are available
// immediately. If not, use OnExtendedAccountInfoUpdated to see whether incoming
// AccountInfo has capability. Once the capability was recorded, the instancess
// will stop reacting to invocations of OnExtendedAccountInfoUpdated silently.
//
// See org.chromium.chrome.browser.ui.signin.AccountCapabilitiesLatencyTracker
// for Android equivalent.
class AccountCapabilitiesLatencyTracker {
 public:
  AccountCapabilitiesLatencyTracker() = delete;
  explicit AccountCapabilitiesLatencyTracker(IdentityManager* identity_manager);
  AccountCapabilitiesLatencyTracker(const AccountCapabilitiesLatencyTracker&) =
      delete;
  AccountCapabilitiesLatencyTracker& operator=(
      const AccountCapabilitiesLatencyTracker&) = delete;

  // Checks whether incoming AccountInfo contains the required capabilities, and
  // records time difference since this instance has been created.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info);

 private:
  bool HasCapability() const;

  IdentityManager* identity_manager_;
  base::ElapsedTimer timer_;
  bool capabilities_already_fetched_{false};
};

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_CAPABILITIES_LATENCY_TRACKER_H_
