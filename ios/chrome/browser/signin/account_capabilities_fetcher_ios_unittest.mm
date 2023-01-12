// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_capabilities_fetcher_ios.h"

#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/signin/capabilities_dict.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

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
  AccountCapabilitiesFetcherIOSTest() = default;
  ~AccountCapabilitiesFetcherIOSTest() override = default;

  // Ensure that callback gets `capability_enabled` on
  // `kCanHaveEmailAddressDisplayedCapabilityName`.
  void TestCapabilityValueFetchedIsReceived(
      absl::optional<SystemIdentityCapabilityResult> capability_fetched,
      signin::Tribool capability_expected) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());

    std::map<std::string, SystemIdentityCapabilityResult> capabilities;
    if (capability_fetched.has_value()) {
      capabilities.insert({kCanHaveEmailAddressDisplayedCapabilityName,
                           capability_fetched.value()});
    }

    // Register a fake identity and set the expected capabilities.
    id<SystemIdentity> identity = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(identity);
    system_identity_manager->SetCapabilities(identity, capabilities);

    // Check that the capabilities are correctly converted.
    base::RunLoop run_loop;
    ios::AccountCapabilitiesFetcherIOS fetcher(
        CoreAccountInfo{},
        base::BindOnce(&CheckHaveEmailAddressDisplayed, capability_expected)
            .Then(run_loop.QuitClosure()),
        identity);

    fetcher.Start();
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
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
