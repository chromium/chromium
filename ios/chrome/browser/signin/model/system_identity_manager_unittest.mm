// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_identity_manager.h"

#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class MockObserver : public SystemIdentityManagerObserver {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnIdentityRefreshTokenUpdated,
              (id<SystemIdentity> identity),
              (override));
};

// Subclass to be able to instantiate `SystemIdentityManager`.
class TestSystemIdentityManager : public SystemIdentityManager {
 public:
  bool IsSigninSupported() final { NOTREACHED(); }
  bool HandleSessionOpenURLContexts(
      UIScene* scene,
      NSSet<UIOpenURLContext*>* url_contexts) final {
    NOTREACHED();
  }
  void ApplicationDidDiscardSceneSessions(
      NSSet<UISceneSession*>* scene_sessions) final {
    NOTREACHED();
  }
  void DismissDialogs() final {}
  id<SystemIdentityInteractionManager> CreateInteractionManager() final {
    NOTREACHED();
  }
  void IterateOverIdentities(IdentityIteratorCallback callback) final {
    NOTREACHED();
  }
  void ForgetIdentity(id<SystemIdentity> identity,
                      ForgetIdentityCallback callback) final {
    NOTREACHED();
  }
  bool IdentityRemovedByUser(NSString* gaia_id) final { NOTREACHED(); }
  void GetAccessToken(id<SystemIdentity> identity,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) final {
    NOTREACHED();
  }
  void GetAccessToken(id<SystemIdentity> identity,
                      const std::string& client_id,
                      const std::set<std::string>& scopes,
                      AccessTokenCallback callback) final {
    NOTREACHED();
  }
  void FetchAvatarForIdentity(id<SystemIdentity> identity) final {
    NOTREACHED();
  }
  UIImage* GetCachedAvatarForIdentity(id<SystemIdentity> identity) final {
    NOTREACHED();
  }
  void GetHostedDomain(id<SystemIdentity> identity,
                       HostedDomainCallback callback) final {
    NOTREACHED();
  }
  NSString* GetCachedHostedDomainForIdentity(
      id<SystemIdentity> identity) final {
    NOTREACHED();
  }
  void FetchCapabilities(id<SystemIdentity> identity,
                         const std::set<std::string>& names,
                         FetchCapabilitiesCallback callback) final {
    NOTREACHED();
  }
  bool HandleMDMNotification(id<SystemIdentity> identity,
                             NSArray<id<SystemIdentity>>* active_identities,
                             id<RefreshAccessTokenError> error,
                             HandleMDMCallback callback) final {
    NOTREACHED();
  }
  bool IsMDMError(id<SystemIdentity> identity, NSError* error) final {
    NOTREACHED();
  }
  DismissViewCallback PresentAccountDetailsController(
      PresentDialogConfiguration configuration) final {
    NOTREACHED();
  }
  DismissViewCallback PresentWebAndAppSettingDetailsController(
      PresentDialogConfiguration configuration) final {
    NOTREACHED();
  }
  DismissViewCallback PresentLinkedServicesSettingsDetailsController(
      PresentDialogConfiguration configuration) final {
    NOTREACHED();
  }

  void TriggerFireIdentityRefreshTokenUpdated(id<SystemIdentity> identity) {
    FireIdentityRefreshTokenUpdated(identity);
  }
};

}  // namespace

class SystemIdentityManagerTest : public PlatformTest {
 public:
  SystemIdentityManagerTest() {
    system_identity_manager_ = std::make_unique<TestSystemIdentityManager>();
  }

 protected:
  std::unique_ptr<TestSystemIdentityManager> system_identity_manager_;
};

// Tests that `OnIdentityRefreshTokenUpdated()` is called on the observer if
// `FireIdentityRefreshTokenUpdated()` is called.
TEST_F(SystemIdentityManagerTest, TestFireIdentityRefreshTokenUpdated) {
  testing::StrictMock<MockObserver> mock_observer;
  system_identity_manager_->AddObserver(&mock_observer);

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  EXPECT_CALL(mock_observer, OnIdentityRefreshTokenUpdated(identity));
  system_identity_manager_->TriggerFireIdentityRefreshTokenUpdated(identity);
  system_identity_manager_->RemoveObserver(&mock_observer);
}
