// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_persistent_state.h"

#import <Foundation/Foundation.h>

#import "base/time/time.h"
#import "components/version_info/version_info.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::Eq;

// Helper used to clear added keys from NSUserDefaults using RAII.
class UserDefaultsHelper {
 public:
  explicit UserDefaultsHelper(NSUserDefaults* defaults) : defaults_(defaults) {
    keys_ = [NSSet setWithArray:[[defaults dictionaryRepresentation] allKeys]];
  }

  UserDefaultsHelper(const UserDefaultsHelper&) = delete;
  UserDefaultsHelper& operator=(const UserDefaultsHelper&) = delete;

  ~UserDefaultsHelper() {
    for (NSString* key in [[defaults_ dictionaryRepresentation] allKeys]) {
      if (![keys_ containsObject:key]) {
        [defaults_ removeObjectForKey:key];
      }
    }
  }

  // Returns the number of new keys in the NSUserDefaults.
  NSUInteger CountOfNewKeys() const {
    NSUInteger count = 0;
    for (NSString* key in [[defaults_ dictionaryRepresentation] allKeys]) {
      if (![keys_ containsObject:key]) {
        ++count;
      }
    }
    return count;
  }

 private:
  NSUserDefaults* defaults_;
  NSSet<NSString*>* keys_;
};

}  // namespace

class OmahaPersistentStateTest : public PlatformTest {
 public:
  // Returns the number of keys in the NSUserDefaults.
  NSUInteger CountOfNewKeys() const { return helper_.CountOfNewKeys(); }

 private:
  UserDefaultsHelper helper_{[NSUserDefaults standardUserDefaults]};
};

// Tests that OmahaPersistentState serialisation can round-trip.
TEST_F(OmahaPersistentStateTest, Serialization) {
  const base::Time now = base::Time::Now();
  const OmahaPersistentState original_state = {
      .next_ping_time = now + base::Hours(8),
      .last_ping_time = now - base::Seconds(30),
      .last_response_time = now,
      .last_sent_version = version_info::GetVersion(),
      .current_request_id = "request-id",
      .number_of_failures = 5,
      .last_server_date = 3,
  };

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  OmahaPersistentState::SaveTo(defaults, original_state);
  EXPECT_THAT(CountOfNewKeys(), Eq(7u));

  EXPECT_THAT(OmahaPersistentState::LoadFrom(defaults), Eq(original_state));
}

// Tests that OmahaPersistentState serialization clears NSUserDefaults
// when a value is equals to the default value.
TEST_F(OmahaPersistentStateTest, RemoveKeyForDefaultValue) {
  const base::Time now = base::Time::Now();
  const OmahaPersistentState original_state = {
      .next_ping_time = now + base::Hours(8),
      .last_ping_time = now - base::Seconds(30),
      .last_response_time = now,
      .last_sent_version = version_info::GetVersion(),
      .current_request_id = "request-id",
      .number_of_failures = 5,
      .last_server_date = 3,
  };

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  OmahaPersistentState::SaveTo(defaults, original_state);
  EXPECT_THAT(CountOfNewKeys(), Eq(7u));

  OmahaPersistentState::SaveTo(defaults, OmahaPersistentState{});
  EXPECT_THAT(CountOfNewKeys(), Eq(0u));
}
