// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/user_permission_service_ios.h"

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/test/task_environment.h"
#import "components/device_signals/core/browser/mock_user_delegate.h"
#import "components/device_signals/core/browser/pref_names.h"
#import "components/device_signals/core/browser/user_permission_service.h"
#import "components/policy/core/common/management/management_service.h"
#import "components/prefs/testing_pref_service.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using testing::AnyNumber;
using testing::Return;

namespace {

// A ManagementService without any management status provider, which represents
// an unmanaged device.
class TestManagementService : public policy::ManagementService {
 public:
  TestManagementService() : ManagementService({}) {}
};

}  // namespace

class UserPermissionServiceIOSTest : public PlatformTest {
 protected:
  UserPermissionServiceIOSTest() {
    device_signals::RegisterProfilePrefs(test_prefs_.registry());

    auto mock_user_delegate = std::make_unique<
        testing::StrictMock<device_signals::MockUserDelegate>>();
    mock_user_delegate_ = mock_user_delegate.get();

    // Mirrors `UserDelegateIOS::IsCollectSignalsConsentRequired()`.
    EXPECT_CALL(*mock_user_delegate_, IsCollectSignalsConsentRequired())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));

    permission_service_ = std::make_unique<UserPermissionServiceIOS>(
        &management_service_, std::move(mock_user_delegate), &test_prefs_);
  }

  ~UserPermissionServiceIOSTest() override { mock_user_delegate_ = nullptr; }

  void SetManagedUser(bool is_managed) {
    EXPECT_CALL(*mock_user_delegate_, IsManagedUser())
        .Times(AnyNumber())
        .WillRepeatedly(Return(is_managed));
  }

  base::test::TaskEnvironment task_environment_;
  TestManagementService management_service_;
  TestingPrefServiceSimple test_prefs_;
  raw_ptr<testing::StrictMock<device_signals::MockUserDelegate>>
      mock_user_delegate_;
  std::unique_ptr<UserPermissionServiceIOS> permission_service_;
};

// Tests that consent is never collected interactively on iOS for an unmanaged
// user.
TEST_F(UserPermissionServiceIOSTest, ShouldCollectConsent_UnmanagedUser) {
  SetManagedUser(/*is_managed=*/false);
  EXPECT_FALSE(permission_service_->ShouldCollectConsent());
}

// Tests that consent is never collected interactively on iOS for a managed
// user.
TEST_F(UserPermissionServiceIOSTest, ShouldCollectConsent_ManagedUser) {
  SetManagedUser(/*is_managed=*/true);
  EXPECT_FALSE(permission_service_->ShouldCollectConsent());
}

// Tests that an unmanaged user without a consent pref has not consented, and
// therefore cannot collect signals.
TEST_F(UserPermissionServiceIOSTest, HasUserConsented_UnmanagedUser) {
  SetManagedUser(/*is_managed=*/false);
  EXPECT_FALSE(permission_service_->HasUserConsented());
  EXPECT_EQ(permission_service_->CanCollectSignals(),
            device_signals::UserPermission::kMissingConsent);
}

// Tests that a managed user implicitly consents to signals collection.
TEST_F(UserPermissionServiceIOSTest, HasUserConsented_ManagedUser) {
  SetManagedUser(/*is_managed=*/true);
  EXPECT_TRUE(permission_service_->HasUserConsented());
  EXPECT_EQ(permission_service_->CanCollectSignals(),
            device_signals::UserPermission::kGranted);
}

// Tests that the consent pref is still honored for an unmanaged user.
TEST_F(UserPermissionServiceIOSTest, HasUserConsented_ConsentPref) {
  SetManagedUser(/*is_managed=*/false);
  test_prefs_.SetBoolean(device_signals::prefs::kDeviceSignalsConsentReceived,
                         true);
  EXPECT_TRUE(permission_service_->HasUserConsented());
  EXPECT_EQ(permission_service_->CanCollectSignals(),
            device_signals::UserPermission::kGranted);
}

// Tests that the permanent consent pref is still honored for an unmanaged user.
TEST_F(UserPermissionServiceIOSTest, HasUserConsented_PermanentConsentPref) {
  SetManagedUser(/*is_managed=*/false);
  test_prefs_.SetBoolean(
      device_signals::prefs::kDeviceSignalsPermanentConsentReceived, true);
  EXPECT_TRUE(permission_service_->HasUserConsented());
  EXPECT_EQ(permission_service_->CanCollectSignals(),
            device_signals::UserPermission::kGranted);
}
