// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/serializable_user_data_manager.h"

#import "ios/web/public/test/fakes/test_web_state.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  web::TestWebState web_state_;
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
  std::unique_ptr<web::SerializableUserData> user_data =
      manager()->CreateSerializableUserData();

  // Archive the serializable user data.
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
  user_data->Encode(archiver);
  [archiver finishEncoding];
  NSData* data = [archiver encodedData];

  // Create a new SerializableUserData by unarchiving.
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  std::unique_ptr<web::SerializableUserData> decoded_data =
      web::SerializableUserData::Create();
  decoded_data->Decode(unarchiver);

  // Add the decoded user data to a new WebState and verify its contents.
  web::TestWebState decoded_web_state;
  web::SerializableUserDataManager* decoded_manager =
      web::SerializableUserDataManager::FromWebState(&decoded_web_state);
  decoded_manager->AddSerializableUserData(decoded_data.get());
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

  std::unique_ptr<web::SerializableUserData> user_data =
      web::SerializableUserData::Create();

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  unarchiver.requiresSecureCoding = NO;
  user_data->Decode(unarchiver);

  web::TestWebState web_state;
  web::SerializableUserDataManager* user_data_manager =
      web::SerializableUserDataManager::FromWebState(&web_state);
  user_data_manager->AddSerializableUserData(user_data.get());

  id value = user_data_manager->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(nil, value);

  user_data_manager->AddSerializableData(kTestUserData, kTestUserDataKey);
  value = user_data_manager->GetValueForSerializationKey(kTestUserDataKey);
  EXPECT_NSEQ(kTestUserData, value);
}
