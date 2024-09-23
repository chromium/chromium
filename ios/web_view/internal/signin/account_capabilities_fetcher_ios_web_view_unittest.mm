// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/account_capabilities_fetcher_ios_web_view.h"

#import <optional>

#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const char kTestEmail[] = "janedoe@chromium.org";

void CheckCapabilityFetchEmpty(
    const CoreAccountId& account_id,
    const std::optional<AccountCapabilities>& capabilities) {
  ASSERT_FALSE(capabilities.has_value());
}

}  // anonymous namespace

class AccountCapabilitiesFetcherIOSWebViewTest : public PlatformTest {
 public:
  ~AccountCapabilitiesFetcherIOSWebViewTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

// Check that account capability fetch is disabled.
TEST_F(AccountCapabilitiesFetcherIOSWebViewTest, CheckCapabilityFetchDisabled) {
  AccountInfo account_info =
      identity_test_environment_.MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_can_have_email_address_displayed(true);
  identity_test_environment_.UpdateAccountInfoForAccount(account_info);

  base::RunLoop run_loop;
  ios_web_view::AccountCapabilitiesFetcherIOSWebView fetcher(
      account_info, AccountCapabilitiesFetcher::FetchPriority::kForeground,
      base::BindOnce(&CheckCapabilityFetchEmpty).Then(run_loop.QuitClosure()));

  fetcher.Start();
  run_loop.Run();
}
