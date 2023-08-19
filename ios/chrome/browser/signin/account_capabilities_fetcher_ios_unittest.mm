// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_capabilities_fetcher_ios.h"

#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/capabilities_dict.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const char kTestEmail[] = "janedoe@chromium.org";

void CheckHaveEmailAddressDisplayed(
    signin::Tribool capability_expected,
    const CoreAccountId& account_id,
    const absl::optional<AccountCapabilities>& capabilities) {
  ASSERT_TRUE(capabilities.has_value());
  ASSERT_EQ(capabilities->can_have_email_address_displayed(),
            capability_expected);
}

}  // anonymous namespace

class AccountCapabilitiesFetcherIOSTest : public PlatformTest {
 public:
  AccountCapabilitiesFetcherIOSTest()
      : chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {}

  ~AccountCapabilitiesFetcherIOSTest() override = default;

  void SetUp() override {
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
  }

  // Ensure that callback gets `capability_enabled` on
  // `kCanHaveEmailAddressDisplayedCapabilityName`.
  void TestCapabilityValueFetchedIsReceived(
      absl::optional<SystemIdentityCapabilityResult> capability_fetched,
      signin::Tribool capability_expected) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());

    CoreAccountInfo account_info =
        identity_test_environment_.MakeAccountAvailable(kTestEmail);

    // Register a fake identity and set the expected capabilities.
    id<SystemIdentity> identity = [FakeSystemIdentity
        identityWithEmail:base::SysUTF8ToNSString(account_info.email)
                   gaiaID:base::SysUTF8ToNSString(account_info.gaia)
                     name:@"Jane Doe"];
    system_identity_manager->AddIdentity(identity);

    if (capability_fetched.has_value() &&
        capability_fetched.value() !=
            SystemIdentityCapabilityResult::kUnknown) {
      AccountCapabilitiesTestMutator* mutator =
          system_identity_manager->GetCapabilitiesMutator(identity);
      bool has_capability =
          capability_fetched.value() == SystemIdentityCapabilityResult::kTrue;
      mutator->set_can_have_email_address_displayed(has_capability);
    }

    // Check that the capabilities are correctly converted.
    base::RunLoop run_loop;
    ios::AccountCapabilitiesFetcherIOS fetcher(
        account_info, account_manager_service_,
        base::BindOnce(&CheckHaveEmailAddressDisplayed, capability_expected)
            .Then(run_loop.QuitClosure()));

    fetcher.Start();
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  ChromeAccountManagerService* account_manager_service_;
};

// Check that a capability set to True is received as True.
TEST_F(AccountCapabilitiesFetcherIOSTest, CheckTrueCapability) {
  TestCapabilityValueFetchedIsReceived(SystemIdentityCapabilityResult::kTrue,
                                       signin::Tribool::kTrue);
}

// Check that a capability set to False is received as False.
TEST_F(AccountCapabilitiesFetcherIOSTest, CheckFalseCapability) {
  TestCapabilityValueFetchedIsReceived(SystemIdentityCapabilityResult::kFalse,
                                       signin::Tribool::kFalse);
}

// Check that a capability set to Unknown is received as Unknown.
TEST_F(AccountCapabilitiesFetcherIOSTest, CheckUnknownCapability) {
  TestCapabilityValueFetchedIsReceived(SystemIdentityCapabilityResult::kUnknown,
                                       signin::Tribool::kUnknown);
}

// Check that an unset capability is received as Unknown.
TEST_F(AccountCapabilitiesFetcherIOSTest, CheckUnsetCapability) {
  TestCapabilityValueFetchedIsReceived(absl::nullopt,
                                       signin::Tribool::kUnknown);
}
