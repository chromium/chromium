// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/application_mode_fetcher/test_application_mode_fetcher.h"

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/public/provider/chrome/browser/application_mode_fetcher/application_mode_fetcher_api.h"
#import "url/gurl.h"

namespace {
id<ApplicationModeFetcherProviderTestHelper>
    g_application_mode_fetcher_provider_test_helper;
}  // namespace

namespace ios::provider {

void FetchApplicationMode(const GURL& url,
                          NSString* app_id,
                          AppModeFetchingResponse fetching_response) {
  [g_application_mode_fetcher_provider_test_helper
      sendFetchingResponseForUrl:url
                      completion:base::CallbackToBlock(
                                     std::move(fetching_response))];
}

namespace test {

void SetApplicationModeFetcherProviderTestHelper(
    id<ApplicationModeFetcherProviderTestHelper> helper) {
  g_application_mode_fetcher_provider_test_helper = helper;
}

}  // namespace test

}  // namespace ios::provider
