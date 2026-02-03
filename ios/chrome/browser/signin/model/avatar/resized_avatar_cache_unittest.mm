// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/avatar/resized_avatar_cache.h"

#import "base/test/task_environment.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

// Test fixture for testing ResizedAvatarCache class.
class ResizedAvatarCacheTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    resized_avatar_cache_ = [[ResizedAvatarCache alloc]
        initWithIdentityAvatarSize:IdentityAvatarSize::TableViewIcon];
    identity1_ = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager()->AddIdentity(identity1_);
    identity2_ = [FakeSystemIdentity fakeIdentity2];
    fake_system_identity_manager()->AddIdentity(identity2_);
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  base::test::TaskEnvironment task_environment_;
  ResizedAvatarCache* resized_avatar_cache_ = nil;
  id<SystemIdentity> identity1_ = nil;
  id<SystemIdentity> identity2_ = nil;
};

// Tests that the default avatar is the same between 2 identities.
TEST_F(ResizedAvatarCacheTest, DefaultAvatarSize) {
  // Resets the avatar to simulate that it was not fetched yet.
  fake_system_identity_manager()->UpdateSystemIdentityAvatar(
      identity1_.gaiaId, nil, /*send_notification=*/false);
  fake_system_identity_manager()->UpdateSystemIdentityAvatar(
      identity2_.gaiaId, nil, /*send_notification=*/false);
  UIImage* avatar1 =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_NE(nil, avatar1);
  // Check the size.
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::TableViewIcon);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar1.size));
  // Asking for an avatar on another identity should return the same image:
  // the default avatar.
  UIImage* avatar2 =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity2_];
  EXPECT_EQ(avatar1, avatar2);
}

// Tests that the avatar is updated after waiting for the fetch.
TEST_F(ResizedAvatarCacheTest, FetchAvatar) {
  // Resets the avatar to simulate that it was not fetched yet.
  fake_system_identity_manager()->UpdateSystemIdentityAvatar(
      identity1_.gaiaId, nil, /*send_notification=*/false);
  UIImage* default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_NE(nil, default_avatar);
  // Asking again for the avatar, the same image is expected again (default
  // avatar).
  UIImage* same_default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_EQ(default_avatar, same_default_avatar);
  // Wait for the end of the fetch.
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  // Asking again, the fecthed avatar is expected (instead of the default
  // avatar)
  UIImage* identity_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_NE(default_avatar, identity_avatar);
  // Check the size.
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::TableViewIcon);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, identity_avatar.size));
}

// Tests that the default avatar is forgotten after a memory warning
// notification.
TEST_F(ResizedAvatarCacheTest, MemoryWarning) {
  // Resets the avatar to simulate that it was not fetched yet.
  fake_system_identity_manager()->UpdateSystemIdentityAvatar(
      identity1_.gaiaId, nil, /*send_notification=*/false);
  fake_system_identity_manager()->UpdateSystemIdentityAvatar(
      identity2_.gaiaId, nil, /*send_notification=*/false);
  UIImage* first_default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_NE(nil, first_default_avatar);
  // Sends the memory warning to drop the default avatar.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidReceiveMemoryWarningNotification
                    object:nil];
  UIImage* second_default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity2_];
  // The default avatar needs to be recomputed, therefore the pointers are
  // different.
  EXPECT_NE(first_default_avatar, second_default_avatar);
}

// Tests that the avatar is reset to the default avatar when identity is removed
// and added back.
TEST_F(ResizedAvatarCacheTest, AddRemoveAndAddAgainIdentity) {
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::TableViewIcon);
  UIImage* first_avatar = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      expected_size, UIColor.whiteColor);
  fake_system_identity_manager()->UpdateSystemIdentityAvatar(
      identity1_.gaiaId, first_avatar, /*send_notification=*/false);
  UIImage* avatar_from_cache =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  // It should be the same instance since the identity avatar is at the right
  // size.
  EXPECT_EQ(avatar_from_cache, first_avatar);

  // Remove the identity, and add it again.
  fake_system_identity_manager()->ForgetIdentity(identity1_, base::DoNothing());
  fake_system_identity_manager()->WaitForServiceCallbacksToComplete();
  fake_system_identity_manager()->AddIdentity(identity1_);
  // Set a different avatar.
  UIImage* second_avatar =
      ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(expected_size,
                                                            UIColor.whiteColor);
  fake_system_identity_manager()->UpdateSystemIdentityAvatar(
      identity1_.gaiaId, second_avatar, /*send_notification=*/false);

  avatar_from_cache =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_EQ(avatar_from_cache, second_avatar);
}
