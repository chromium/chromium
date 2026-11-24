// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_trusted_vault_client.h"

#import <Foundation/Foundation.h>

#import <cstdint>
#import <vector>

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller.h"
#import "ios/web_view/public/cwv_trusted_vault_key.h"
#import "ios/web_view/public/cwv_trusted_vault_provider.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeBaseTrustedVaultProvider : NSObject <CWVTrustedVaultProvider>
@property(nonatomic, strong) CWVIdentity* lastFetchedIdentity;
@end

@implementation FakeBaseTrustedVaultProvider

- (void)addTrustedVaultObserver:(CWVTrustedVaultObserver*)observer {
}

- (void)removeTrustedVaultObserver:(CWVTrustedVaultObserver*)observer {
}

- (void)markLocalKeysAsStaleForIdentity:(CWVIdentity*)identity
                             completion:(void (^)(NSError*))completion {
  completion(nil);
}

- (void)isRecoverabilityDegradedForIdentity:(CWVIdentity*)identity
                                 completion:
                                     (void (^)(BOOL, NSError*))completion {
  completion(NO, nil);
}

- (void)clearLocalDataForForIdentity:(CWVIdentity*)identity {
}

@end

// Provider implementing both selectors to verify that
// `-fetchKeysWithVersionsForIdentity:completion:` takes precedence.
@interface FakeModernTrustedVaultProvider : FakeBaseTrustedVaultProvider
@property(nonatomic, copy) NSArray<CWVTrustedVaultKey*>* keys;
@property(nonatomic, strong) NSError* error;
@end

@implementation FakeModernTrustedVaultProvider

- (void)fetchKeysWithVersionsForIdentity:(CWVIdentity*)identity
                              completion:
                                  (void (^)(NSArray<CWVTrustedVaultKey*>*,
                                            NSError*))completion {
  self.lastFetchedIdentity = identity;
  completion(self.keys, self.error);
}

- (void)fetchKeysForIdentity:(CWVIdentity*)identity
                  completion:(void (^)(NSArray<NSData*>*, NSError*))completion {
  ADD_FAILURE() << "Legacy selector should not be called when "
                   "fetchKeysWithVersionsForIdentity:completion: is "
                   "implemented.";
}

@end

// Provider implementing only the legacy `-fetchKeysForIdentity:completion:`
// selector.
@interface FakeLegacyTrustedVaultProvider : FakeBaseTrustedVaultProvider
@property(nonatomic, copy) NSArray<NSData*>* keys;
@property(nonatomic, strong) NSError* error;
@end

@implementation FakeLegacyTrustedVaultProvider

- (void)fetchKeysForIdentity:(CWVIdentity*)identity
                  completion:(void (^)(NSArray<NSData*>*, NSError*))completion {
  self.lastFetchedIdentity = identity;
  completion(self.keys, self.error);
}

@end

namespace ios_web_view {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

CoreAccountInfo MakeTestAccount() {
  CoreAccountInfo account;
  account.email = "user@example.com";
  account.gaia = GaiaId("test-gaia-id");
  return account;
}

class WebViewTrustedVaultClientTest : public PlatformTest {
 protected:
  void TearDown() override {
    CWVSyncController.trustedVaultProvider = nil;
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebViewTrustedVaultClientTest, KeyInitializationAndEquality) {
  constexpr uint8_t kBytes1[] = {1, 2, 3};
  constexpr uint8_t kBytes2[] = {4, 5, 6};
  NSData* data1 = [NSData dataWithBytes:kBytes1 length:sizeof(kBytes1)];
  NSData* data2 = [NSData dataWithBytes:kBytes2 length:sizeof(kBytes2)];

  CWVTrustedVaultKey* key1 =
      [[CWVTrustedVaultKey alloc] initWithKeyMaterial:data1 version:7];
  CWVTrustedVaultKey* key1_copy =
      [[CWVTrustedVaultKey alloc] initWithKeyMaterial:data1 version:7];
  CWVTrustedVaultKey* different_material =
      [[CWVTrustedVaultKey alloc] initWithKeyMaterial:data2 version:7];
  CWVTrustedVaultKey* different_version =
      [[CWVTrustedVaultKey alloc] initWithKeyMaterial:data1 version:8];

  EXPECT_NSEQ(data1, key1.keyMaterial);
  EXPECT_EQ(7, key1.version);
  EXPECT_NSEQ(key1, key1_copy);
  EXPECT_EQ(key1.hash, key1_copy.hash);
  EXPECT_NSNE(key1, different_material);
  EXPECT_NSNE(key1, different_version);

  NSString* debug_description = [key1 debugDescription];
  EXPECT_TRUE(
      [debug_description containsString:@"version: 7, keyMaterialLength: 3"]);
  EXPECT_FALSE([debug_description containsString:@"010203"]);
}

TEST_F(WebViewTrustedVaultClientTest, FetchKeysUsesKeysWithVersionsSelector) {
  constexpr uint8_t kBytes1[] = {1, 2, 3};
  constexpr uint8_t kBytes2[] = {4, 5};
  FakeModernTrustedVaultProvider* provider =
      [[FakeModernTrustedVaultProvider alloc] init];
  provider.keys = @[
    [[CWVTrustedVaultKey alloc]
        initWithKeyMaterial:[NSData dataWithBytes:kBytes1
                                           length:sizeof(kBytes1)]
                    version:10],
    [[CWVTrustedVaultKey alloc]
        initWithKeyMaterial:[NSData dataWithBytes:kBytes2
                                           length:sizeof(kBytes2)]
                    version:11],
  ];
  CWVSyncController.trustedVaultProvider = provider;

  WebViewTrustedVaultClient client;
  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
  client.FetchKeys(MakeTestAccount(), future.GetCallback());

  EXPECT_NSEQ(@"test-gaia-id", provider.lastFetchedIdentity.gaiaID);
  EXPECT_NSEQ(@"user@example.com", provider.lastFetchedIdentity.email);
  EXPECT_THAT(future.Get(), ElementsAre(std::vector<uint8_t>{1, 2, 3},
                                        std::vector<uint8_t>{4, 5}));
}

TEST_F(WebViewTrustedVaultClientTest, FetchKeysFallsBackToLegacySelector) {
  constexpr uint8_t kBytes[] = {9, 8, 7};
  FakeLegacyTrustedVaultProvider* provider =
      [[FakeLegacyTrustedVaultProvider alloc] init];
  provider.keys = @[ [NSData dataWithBytes:kBytes length:sizeof(kBytes)] ];
  CWVSyncController.trustedVaultProvider = provider;

  WebViewTrustedVaultClient client;
  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
  client.FetchKeys(MakeTestAccount(), future.GetCallback());

  EXPECT_NSEQ(@"test-gaia-id", provider.lastFetchedIdentity.gaiaID);
  EXPECT_THAT(future.Get(), ElementsAre(std::vector<uint8_t>{9, 8, 7}));
}

TEST_F(WebViewTrustedVaultClientTest, FetchKeysReturnsEmptyOnError) {
  constexpr uint8_t kBytes[] = {1, 2, 3};
  NSData* key_data = [NSData dataWithBytes:kBytes length:sizeof(kBytes)];
  NSError* test_error = [NSError errorWithDomain:@"test" code:1 userInfo:nil];
  WebViewTrustedVaultClient client;

  FakeModernTrustedVaultProvider* modern_provider =
      [[FakeModernTrustedVaultProvider alloc] init];
  modern_provider.keys =
      @[ [[CWVTrustedVaultKey alloc] initWithKeyMaterial:key_data version:10] ];
  modern_provider.error = test_error;
  CWVSyncController.trustedVaultProvider = modern_provider;

  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&>
      modern_future;
  client.FetchKeys(MakeTestAccount(), modern_future.GetCallback());
  EXPECT_THAT(modern_future.Get(), IsEmpty());

  FakeLegacyTrustedVaultProvider* legacy_provider =
      [[FakeLegacyTrustedVaultProvider alloc] init];
  legacy_provider.keys = @[ key_data ];
  legacy_provider.error = test_error;
  CWVSyncController.trustedVaultProvider = legacy_provider;

  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&>
      legacy_future;
  client.FetchKeys(MakeTestAccount(), legacy_future.GetCallback());
  EXPECT_THAT(legacy_future.Get(), IsEmpty());
}

TEST_F(WebViewTrustedVaultClientTest,
       FetchKeysReturnsEmptyWhenProviderImplementsNeitherSelector) {
  FakeBaseTrustedVaultProvider* provider =
      [[FakeBaseTrustedVaultProvider alloc] init];
  CWVSyncController.trustedVaultProvider = provider;

  WebViewTrustedVaultClient client;
  base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
  client.FetchKeys(MakeTestAccount(), future.GetCallback());

  EXPECT_THAT(future.Get(), IsEmpty());
}

}  // namespace
}  // namespace ios_web_view
