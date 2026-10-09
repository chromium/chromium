// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_SWITCHER_INFO_TEST_SWITCHER_INFO_H_
#define IOS_CHROME_TEST_PROVIDERS_SWITCHER_INFO_TEST_SWITCHER_INFO_H_

#import <optional>

#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"

namespace ios::provider::test {

// Sets the result returned by `GetSwitcherInfo()` in tests.
void SetSwitcherInfoResult(std::optional<SwitcherInfoResult> result);

// Resets the test switcher info state back to default (`std::nullopt`).
void ResetSwitcherInfoState();

}  // namespace ios::provider::test

#endif  // IOS_CHROME_TEST_PROVIDERS_SWITCHER_INFO_TEST_SWITCHER_INFO_H_
