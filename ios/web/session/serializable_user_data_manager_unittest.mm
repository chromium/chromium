// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/serializable_user_data_manager.h"

#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
// User Data and Key to use for tests.
NSString* const kTestUserData = @"TestUserData";
NSString* const kTestUserDataKey = @"TestUserDataKey";
}  // namespace

class SerializableUserDataManagerTest : public PlatformTest {
 protected:
  // Convenience getter for the user data manager.
  web::SerializableUserDataManager* manager() {
    return web::SerializableUserDataManager::FromWebState(&web_state_);
  }

  web::FakeWebState web_state_;
};

// Tests that serializable data can be successfully added and read.
TEST_F(SerializableUserDataManagerTest, SetAndReadData) {
  manager()->AddSerializableData(kTestUserData, kTestUserDataKey);
  id value = manager()->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(value, kTestUserData);
}

// Tests that SerializableUserData can successfully encode and decode.
TEST_F(SerializableUserDataManagerTest, EncodeDecode) {
  // Create a SerializableUserData instance for the test data.
  manager()->AddSerializableData(kTestUserData, kTestUserDataKey);
  CRWSessionUserData* user_data = manager()->GetUserDataForSession();

  // Archive the serializable user data.
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
  [archiver encodeObject:user_data forKey:NSKeyedArchiveRootObjectKey];
  [archiver finishEncoding];
  NSData* data = [archiver encodedData];

  // Create a new SerializableUserData by unarchiving.
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  CRWSessionUserData* decoded_data =
      [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];

  // Add the decoded user data to a new WebState and verify its contents.
  web::FakeWebState decoded_web_state;
  web::SerializableUserDataManager* decoded_manager =
      web::SerializableUserDataManager::FromWebState(&decoded_web_state);
  decoded_manager->SetUserDataFromSession(decoded_data);
  id decoded_value =
      decoded_manager->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(decoded_value, kTestUserData);
}

// Check that if serialized data does not include user data, then restored
// SerializableUserDataManager still allow reading and writing user data
// (see http://crbug.com/699249 for details).
TEST_F(SerializableUserDataManagerTest, DecodeNoData) {
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
  [archiver finishEncoding];
  NSData* data = [archiver encodedData];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  CRWSessionUserData* user_data =
      [unarchiver decodeObjectForKey:NSKeyedArchiveRootObjectKey];

  web::FakeWebState web_state;
  web::SerializableUserDataManager* user_data_manager =
      web::SerializableUserDataManager::FromWebState(&web_state);
  user_data_manager->SetUserDataFromSession(user_data);

  id value = user_data_manager->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(nil, value);

  user_data_manager->AddSerializableData(kTestUserData, kTestUserDataKey);
  value = user_data_manager->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(kTestUserData, value);
}
