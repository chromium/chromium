// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_APP_SWITCHER_TEST_APP_SWITCHER_H_
#define IOS_CHROME_TEST_PROVIDERS_APP_SWITCHER_TEST_APP_SWITCHER_H_

#import <string_view>

#import "ios/public/provider/chrome/browser/app_switcher/app_switcher_api.h"

using AppSwitcherResponseCompletion =
    void (^)(ios::provider::AppSwitcherUrlOpeningResult result);

// A protocol to replace the App Switcher provider in tests.
@protocol AppSwitcherProviderTestHelper

- (void)sendAppSwitcherResponseForUrl:(const GURL&)url
                                appId:(std::string_view)appId
                           completion:(AppSwitcherResponseCompletion)completion;

@end

namespace ios::provider {
namespace test {

// Sets the global helper for the tests.
// Resets it if `helper` is nil.
void SetAppSwitcherProviderTestHelper(id<AppSwitcherProviderTestHelper> helper);

}  // namespace test
}  // namespace ios::provider

#endif  // IOS_CHROME_TEST_PROVIDERS_APP_SWITCHER_TEST_APP_SWITCHER_H_
