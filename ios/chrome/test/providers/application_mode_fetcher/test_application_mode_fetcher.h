// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_APPLICATION_MODE_FETCHER_TEST_APPLICATION_MODE_FETCHER_H_
#define IOS_CHROME_TEST_PROVIDERS_APPLICATION_MODE_FETCHER_TEST_APPLICATION_MODE_FETCHER_H_

#import "ios/public/provider/chrome/browser/application_mode_fetcher/application_mode_fetcher_api.h"

typedef void (^FetchingResponseCompletion)(BOOL isIncognito, NSError* error);

// A protocol to replace the Application Fetcher provide in tests.
@protocol ApplicationModeFetcherProviderTestHelper

- (void)sendFetchingResponseForUrl:(const GURL&)url
                        completion:(FetchingResponseCompletion)completion;
@end

namespace ios::provider {
namespace test {

// Sets the global helper for the tests.
// Resets it if `helper` is nil.
void SetApplicationModeFetcherProviderTestHelper(
    id<ApplicationModeFetcherProviderTestHelper> helper);

}  // namespace test
}  // namespace ios::provider

#endif  // IOS_CHROME_TEST_PROVIDERS_APPLICATION_MODE_FETCHER_TEST_APPLICATION_MODE_FETCHER_H_
