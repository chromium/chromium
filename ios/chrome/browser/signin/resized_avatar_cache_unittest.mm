// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/resized_avatar_cache.h"

#import "base/values.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing ResizedAvatarCache class.
class ResizedAvatarCacheTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    resized_avatar_cache_ = [[ResizedAvatarCache alloc]
        initWithIdentityAvatarSize:IdentityAvatarSize::TableViewIcon];
    identity1_ = [FakeSystemIdentity identityWithEmail:@"test1@email.com"
                                                gaiaID:@"gaiaID1"
                                                  name:@"Test Name1"];
    identity2_ = [FakeSystemIdentity identityWithEmail:@"test2@email.com"
                                                gaiaID:@"gaiaID2"
                                                  name:@"Test Name2"];
  }

  ios::FakeChromeIdentityService* identity_service_ = nil;
  ResizedAvatarCache* resized_avatar_cache_ = nil;
  id<SystemIdentity> identity1_ = nil;
  id<SystemIdentity> identity2_ = nil;
};

// Tests that the default avatar is the same between 2 identities.
TEST_F(ResizedAvatarCacheTest, DefaultAvatarSize) {
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
  UIImage* default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_NE(nil, default_avatar);
  // Asking again for the avatar, the same image is expected again (default
  // avatar).
  UIImage* same_default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_EQ(default_avatar, same_default_avatar);
  // Wait for the end of the fetch.
  identity_service_->WaitForServiceCallbacksToComplete();
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
  UIImage* first_default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity1_];
  EXPECT_NE(nil, first_default_avatar);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidReceiveMemoryWarningNotification
                    object:nil];
  UIImage* second_default_avatar =
      [resized_avatar_cache_ resizedAvatarForIdentity:identity2_];
  EXPECT_NE(first_default_avatar, second_default_avatar);
}
