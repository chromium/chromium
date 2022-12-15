// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_capabilities_fetcher_ios.h"

#import "base/callback.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/signin/capabilities_dict.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A version of ChromeIdentityService which, when fetching, always return
// `capabilities`.
class ChromeIdentityServiceFake : public ios::ChromeIdentityService {
 public:
  ChromeIdentityServiceFake(ios::CapabilitiesDict* capabilities)
      : capabilities_(capabilities) {}

  void FetchCapabilities(
      id<SystemIdentity> identity,
      NSArray<NSString*>* capabilities,
      ios::ChromeIdentityCapabilitiesFetchCompletionBlock completion) override {
    completion(capabilities_, nullptr);
  }

 private:
  ios::CapabilitiesDict* capabilities_;
};

void CheckHaveEmailAddressDisplayed(
    signin::Tribool capability_expected,
    base::RunLoop* run_loop,
    const CoreAccountId& core_accound_id,
    const absl::optional<AccountCapabilities>& account_capabilities) {
  ASSERT_TRUE(account_capabilities.has_value());
  ASSERT_EQ(account_capabilities->can_have_email_address_displayed(),
            capability_expected);
  run_loop->Quit();
}

class AccountCapabilitiesFetcherIOSTest : public PlatformTest {
 public:
  AccountCapabilitiesFetcherIOSTest() {
    identity_ = [FakeSystemIdentity identityWithEmail:@"foo@bar.com"
                                               gaiaID:@"foo_bar_id"
                                                 name:@"Foo"];
  }

  ~AccountCapabilitiesFetcherIOSTest() override = default;

 protected:
  // Ensure that callback gets `capability_enabled` on
  // `kCanHaveEmailAddressDisplayedCapabilityName`.
  void testCapabilityValueFetchedIsReceived(
      ios::ChromeIdentityCapabilityResult capability_fetched,
      signin::Tribool capability_expected) {
    base::test::SingleThreadTaskEnvironment task_environment;
    base::RunLoop run_loop;
    CoreAccountInfo account_info;
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback =
        base::BindOnce(&CheckHaveEmailAddressDisplayed, capability_expected,
                       &run_loop);
    ios::CapabilitiesDict* capabilities = @{
      @(kCanHaveEmailAddressDisplayedCapabilityName) :
          @(static_cast<int>(capability_fetched))
    };
    ChromeIdentityServiceFake chrome_identity_service =
        ChromeIdentityServiceFake(capabilities);

    ios::AccountCapabilitiesFetcherIOS fetcher(
        account_info, std::move(on_complete_callback), &chrome_identity_service,
        identity_);
    fetcher.Start();
    run_loop.Run();
  }

  FakeSystemIdentity* identity_ = nil;
};

// Check that a capability set to True is received as True.
TEST_F(AccountCapabilitiesFetcherIOSTest, CheckTrueCapability) {
  testCapabilityValueFetchedIsReceived(
      ios::ChromeIdentityCapabilityResult::kTrue, signin::Tribool::kTrue);
}

// Check that a capability set to False is received as False.
TEST_F(AccountCapabilitiesFetcherIOSTest, CheckFalseCapability) {
  testCapabilityValueFetchedIsReceived(
      ios::ChromeIdentityCapabilityResult::kFalse, signin::Tribool::kFalse);
}

// Check that a capability set to Unknown is received as Unknown.
TEST_F(AccountCapabilitiesFetcherIOSTest, CheckUnknownCapability) {
  testCapabilityValueFetchedIsReceived(
      ios::ChromeIdentityCapabilityResult::kUnknown, signin::Tribool::kUnknown);
}
