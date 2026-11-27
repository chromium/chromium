// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/test/gtest_util.h"
#import "components/trusted_vault/trusted_vault_key_and_version.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Fake `PasskeyKeychainProviderBridgeDelegate` for testing key fetches and
// reauthentication flows without UI.
@interface FakePasskeyKeychainProviderBridgeDelegate
    : NSObject <PasskeyKeychainProviderBridgeDelegate>

@property(nonatomic, readonly) BOOL providerDidCompleteReauthenticationCalled;

@end

@implementation FakePasskeyKeychainProviderBridgeDelegate

#pragma mark - PasskeyKeychainProviderBridgeDelegate

- (void)performUserVerificationIfNeeded:
    (UserVerificationCompletionBlock)completion {
  completion(YES);
}

- (void)showWelcomeScreenWithPurpose:
            (webauthn::PasskeyWelcomeScreenPurpose)purpose
                          completion:
                              (webauthn::PasskeyWelcomeScreenAction)completion {
  completion(/*navigationController=*/nil);
}

- (void)providerDidCompleteReauthentication {
  _providerDidCompleteReauthenticationCalled = YES;
}

@end

namespace {

NSString* const kTestGaiaId = @"gaia_id";

// Fake `PasskeyKeychainProvider` returning configurable keys for `FetchKeys()`
// and `Reauthenticate()`.
class FakePasskeyKeychainProvider : public PasskeyKeychainProvider {
 public:
  FakePasskeyKeychainProvider(
      std::vector<trusted_vault::TrustedVaultKeyAndVersion> fetch_keys_result,
      std::vector<trusted_vault::TrustedVaultKeyAndVersion>
          reauthenticate_result)
      : PasskeyKeychainProvider(/*metrics_reporting_enabled=*/false),
        fetch_keys_result_(std::move(fetch_keys_result)),
        reauthenticate_result_(std::move(reauthenticate_result)) {}

  ~FakePasskeyKeychainProvider() override = default;

  // `PasskeyKeychainProvider`:
  void CheckEnrolled(NSString* gaia, CheckEnrolledCallback callback) override {
    std::move(callback).Run(/*is_enrolled=*/YES, /*error=*/nil);
  }

  void FetchKeys(NSString* gaia,
                 webauthn::ReauthenticatePurpose purpose,
                 webauthn::KeysWithVersionsFetchedCallback callback) override {
    std::move(callback).Run(fetch_keys_result_, /*error=*/nil);
  }

  void Reauthenticate(
      NSString* gaia,
      UINavigationController* navigation_controller,
      UIView* navigation_item_title_view,
      webauthn::ReauthenticatePurpose purpose,
      webauthn::KeysWithVersionsFetchedCallback callback) override {
    std::move(callback).Run(reauthenticate_result_, /*error=*/nil);
  }

  void CheckDegradedRecoverability(
      NSString* gaia,
      CheckDegradedRecoverabilityCallback callback) override {
    std::move(callback).Run(/*in_degraded_recoverability=*/NO, /*error=*/nil);
  }

 private:
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> fetch_keys_result_;
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> reauthenticate_result_;
};

using PasskeyKeychainProviderBridgeTest = PlatformTest;

// Tests that `fetchTrustedVaultKeysForGaia:` forwards keys returned in
// ascending version order by `FetchKeys()`.
TEST_F(PasskeyKeychainProviderBridgeTest, FetchKeysReturnsAscendingKeys) {
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> expected_keys = {
      trusted_vault::TrustedVaultKeyAndVersion({1, 2, 3}, /*version=*/2),
      trusted_vault::TrustedVaultKeyAndVersion({4, 5, 6}, /*version=*/5),
  };
  PasskeyKeychainProviderBridge* bridge = [[PasskeyKeychainProviderBridge alloc]
      initWithPasskeyKeychainProvider:
          std::make_unique<FakePasskeyKeychainProvider>(
              expected_keys,
              std::vector<trusted_vault::TrustedVaultKeyAndVersion>{})];

  __block bool completion_called = false;
  __block std::vector<trusted_vault::TrustedVaultKeyAndVersion> fetched_keys;
  [bridge fetchTrustedVaultKeysForGaia:kTestGaiaId
                            credential:nil
                               purpose:webauthn::ReauthenticatePurpose::kEncrypt
                               allowUI:NO
                            completion:^(
                                std::vector<
                                    trusted_vault::TrustedVaultKeyAndVersion>
                                    trusted_vault_keys,
                                NSError* error) {
                              completion_called = true;
                              EXPECT_FALSE(error);
                              fetched_keys = std::move(trusted_vault_keys);
                            }];

  EXPECT_TRUE(completion_called);
  EXPECT_EQ(fetched_keys, expected_keys);
}

// Tests that `fetchTrustedVaultKeysForGaia:` triggers a `CHECK` crash if
// `FetchKeys()` returns keys that are not sorted by ascending version.
TEST_F(PasskeyKeychainProviderBridgeTest, FetchKeysUnorderedCrashes) {
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> unsorted_keys = {
      trusted_vault::TrustedVaultKeyAndVersion({4, 5, 6}, /*version=*/5),
      trusted_vault::TrustedVaultKeyAndVersion({1, 2, 3}, /*version=*/2),
  };
  PasskeyKeychainProviderBridge* bridge = [[PasskeyKeychainProviderBridge alloc]
      initWithPasskeyKeychainProvider:
          std::make_unique<FakePasskeyKeychainProvider>(
              std::move(unsorted_keys),
              std::vector<trusted_vault::TrustedVaultKeyAndVersion>{})];

  FetchTrustedVaultKeysCompletionBlock noop_completion =
      ^(std::vector<trusted_vault::TrustedVaultKeyAndVersion>, NSError*) {
      };
  EXPECT_CHECK_DEATH([bridge
      fetchTrustedVaultKeysForGaia:kTestGaiaId
                        credential:nil
                           purpose:webauthn::ReauthenticatePurpose::kEncrypt
                           allowUI:NO
                        completion:noop_completion]);
}

// Tests that `fetchTrustedVaultKeysForGaia:` forwards keys returned in
// ascending version order by `Reauthenticate()`.
TEST_F(PasskeyKeychainProviderBridgeTest, ReauthenticateReturnsAscendingKeys) {
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> expected_keys = {
      trusted_vault::TrustedVaultKeyAndVersion({1, 2, 3}, /*version=*/2),
      trusted_vault::TrustedVaultKeyAndVersion({4, 5, 6}, /*version=*/5),
  };
  PasskeyKeychainProviderBridge* bridge = [[PasskeyKeychainProviderBridge alloc]
      initWithPasskeyKeychainProvider:
          std::make_unique<FakePasskeyKeychainProvider>(
              std::vector<trusted_vault::TrustedVaultKeyAndVersion>{},
              expected_keys)];
  FakePasskeyKeychainProviderBridgeDelegate* delegate =
      [[FakePasskeyKeychainProviderBridgeDelegate alloc] init];
  bridge.delegate = delegate;

  __block bool completion_called = false;
  __block std::vector<trusted_vault::TrustedVaultKeyAndVersion> fetched_keys;
  [bridge fetchTrustedVaultKeysForGaia:kTestGaiaId
                            credential:nil
                               purpose:webauthn::ReauthenticatePurpose::kDecrypt
                               allowUI:YES
                            completion:^(
                                std::vector<
                                    trusted_vault::TrustedVaultKeyAndVersion>
                                    trusted_vault_keys,
                                NSError* error) {
                              completion_called = true;
                              EXPECT_FALSE(error);
                              fetched_keys = std::move(trusted_vault_keys);
                            }];

  EXPECT_TRUE(delegate.providerDidCompleteReauthenticationCalled);
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(fetched_keys, expected_keys);
}

// Tests that `fetchTrustedVaultKeysForGaia:` triggers a `CHECK` crash if
// `Reauthenticate()` returns keys that are not sorted by ascending version.
TEST_F(PasskeyKeychainProviderBridgeTest, ReauthenticateUnorderedCrashes) {
  std::vector<trusted_vault::TrustedVaultKeyAndVersion> unsorted_keys = {
      trusted_vault::TrustedVaultKeyAndVersion({4, 5, 6}, /*version=*/5),
      trusted_vault::TrustedVaultKeyAndVersion({1, 2, 3}, /*version=*/2),
  };
  PasskeyKeychainProviderBridge* bridge = [[PasskeyKeychainProviderBridge alloc]
      initWithPasskeyKeychainProvider:
          std::make_unique<FakePasskeyKeychainProvider>(
              std::vector<trusted_vault::TrustedVaultKeyAndVersion>{},
              std::move(unsorted_keys))];
  FakePasskeyKeychainProviderBridgeDelegate* delegate =
      [[FakePasskeyKeychainProviderBridgeDelegate alloc] init];
  bridge.delegate = delegate;

  FetchTrustedVaultKeysCompletionBlock noop_completion =
      ^(std::vector<trusted_vault::TrustedVaultKeyAndVersion>, NSError*) {
      };
  EXPECT_CHECK_DEATH([bridge
      fetchTrustedVaultKeysForGaia:kTestGaiaId
                        credential:nil
                           purpose:webauthn::ReauthenticatePurpose::kDecrypt
                           allowUI:YES
                        completion:noop_completion]);
}

}  // namespace
