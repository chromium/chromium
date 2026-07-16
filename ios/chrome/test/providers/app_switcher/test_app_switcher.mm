// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/app_switcher/test_app_switcher.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/public/provider/chrome/browser/app_switcher/app_switcher_api.h"
#import "url/gurl.h"

namespace {
id<AppSwitcherProviderTestHelper> g_app_switcher_provider_test_helper;
}  // namespace

namespace ios::provider {

void FetchAppSwitcherParams(const GURL& url,
                            std::string_view app_id,
                            AppSwitcherResponseCallback callback) {
  if (g_app_switcher_provider_test_helper) {
    [g_app_switcher_provider_test_helper
        sendAppSwitcherResponseForUrl:url
                                appId:app_id
                           completion:base::CallbackToBlock(
                                          std::move(callback))];
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), AppSwitcherUrlOpeningResult{}));
  }
}

namespace test {

void SetAppSwitcherProviderTestHelper(
    id<AppSwitcherProviderTestHelper> helper) {
  g_app_switcher_provider_test_helper = helper;
}

}  // namespace test

}  // namespace ios::provider
