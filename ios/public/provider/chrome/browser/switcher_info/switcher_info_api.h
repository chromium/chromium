// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SWITCHER_INFO_SWITCHER_INFO_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SWITCHER_INFO_SWITCHER_INFO_API_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "base/functional/callback.h"

@protocol SystemIdentity;

namespace ios::provider {

// Indicates whether a user is an Android switcher.
enum class SwitcherStatus {
  kUnknown = 0,
  kNotSwitcher = 1,
  kSwitcher = 2,
};

// Information about a user's switcher status and Chrome usage.
struct SwitcherInfoResult {
  SwitcherStatus switcher_status = SwitcherStatus::kUnknown;
  bool is_chrome_user = false;
};

// Callback invoked when fetching switcher information completes. Passes
// `std::nullopt` if an error occurred.
using SwitcherInfoCallback =
    base::OnceCallback<void(std::optional<SwitcherInfoResult> result)>;

// Fetches switcher information for the account associated with `identity`.
void GetSwitcherInfo(id<SystemIdentity> identity,
                     SwitcherInfoCallback callback);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SWITCHER_INFO_SWITCHER_INFO_API_H_
