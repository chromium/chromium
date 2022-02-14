// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/chrome_trusted_vault_service.h"

#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

namespace {

class TestChromeTrustedVaultService : public ChromeTrustedVaultService {
 public:
  TestChromeTrustedVaultService() = default;
  ~TestChromeTrustedVaultService() override = default;

  // Expose publicly for testing.
  using ChromeTrustedVaultService::NotifyKeysChanged;
  using ChromeTrustedVaultService::NotifyRecoverabilityChanged;

  // ChromeTrustedVaultService overrides.
  void FetchKeys(ChromeIdentity* chrome_identity,
                 base::OnceCallback<void(const TrustedVaultSharedKeyList&)>
                     callback) override {}
  void MarkLocalKeysAsStale(ChromeIdentity* chrome_identity,
                            base::OnceClosure callback) override {}
  void GetDegradedRecoverabilityStatus(
      ChromeIdentity* chrome_identity,
      base::OnceCallback<void(bool)> callback) override {}
  void FixDegradedRecoverability(ChromeIdentity* chrome_identity,
                                 UIViewController* presentingViewController,
                                 void (^callback)(BOOL success,
                                                  NSError* error)) override {}
  void Reauthentication(ChromeIdentity* chrome_identity,
                        UIViewController* presentingViewController,
                        void (^callback)(BOOL success,
                                         NSError* error)) override {}
  void CancelDialog(BOOL animated, void (^callback)(void)) override {}
  void ClearLocalDataForIdentity(ChromeIdentity* chrome_identity,
                                 void (^callback)(BOOL success,
                                                  NSError* error)) override {}
};

class MockObserver : public syncer::TrustedVaultClient::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD0(OnTrustedVaultKeysChanged, void());
  MOCK_METHOD0(OnTrustedVaultRecoverabilityChanged, void());
};

using ChromeTrustedVaultServiceTest = PlatformTest;

TEST_F(ChromeTrustedVaultServiceTest, ShouldNotifyKeysChanged) {
  testing::NiceMock<MockObserver> observer;
  TestChromeTrustedVaultService service;
  service.AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged());
  service.NotifyKeysChanged();
}

TEST_F(ChromeTrustedVaultServiceTest, ShouldNotifyRecoverabilityChanged) {
  testing::NiceMock<MockObserver> observer;
  TestChromeTrustedVaultService service;
  service.AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultRecoverabilityChanged());
  service.NotifyRecoverabilityChanged();
}

TEST_F(ChromeTrustedVaultServiceTest, ShouldRemoveObserver) {
  testing::NiceMock<MockObserver> observer;
  TestChromeTrustedVaultService service;
  service.AddObserver(&observer);
  service.RemoveObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged()).Times(0);
  service.NotifyKeysChanged();
}

}  // namespace

}  // namespace ios
