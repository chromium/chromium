// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/web_session_state_cache.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

constexpr base::TimeDelta kRemoveSessionStateDataDelay = base::Seconds(15);

class WebSessionStateCacheTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder test_cbs_builder;
    profile_ = std::move(test_cbs_builder).Build();
    sessionCache_ =
        [[WebSessionStateCache alloc] initWithBrowserState:profile_.get()];

    web::WebState::CreateParams createParams(profile_.get());
    web_state_ = web::WebState::Create(createParams);

    session_cache_directory_ =
        profile_->GetStatePath().Append(kLegacyWebSessionsDirname);
  }

  bool StorageExists() {
    std::string identifier = base::StringPrintf(
        "%08u",
        static_cast<uint32_t>(web_state_->GetUniqueIdentifier().identifier()));

    base::FilePath file_path = session_cache_directory_.Append(identifier);
    return base::PathExists(file_path);
  }

  void TearDown() override {
    [sessionCache_ shutdown];
    sessionCache_ = nil;
    PlatformTest::TearDown();
  }

  WebSessionStateCache* GetSessionCache() { return sessionCache_; }

  // Flushes all the runloops internally used by the cache.
  void FlushRunLoops() {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath session_cache_directory_;
  std::unique_ptr<web::WebState> web_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  WebSessionStateCache* sessionCache_;
};

// Tests that the expected webState storage file is added and removed as
// expected.
TEST_F(WebSessionStateCacheTest, CacheAddAndRemove) {
  WebSessionStateCache* cache = GetSessionCache();
  const char data_str[] = "foo";
  NSData* data = [NSData dataWithBytes:data_str length:strlen(data_str)];
  EXPECT_FALSE(StorageExists());
  [cache persistSessionStateData:data
                   forWebStateID:web_state_->GetUniqueIdentifier()];
  FlushRunLoops();
  EXPECT_TRUE(StorageExists());
  NSData* data_back =
      [cache sessionStateDataForWebStateID:web_state_->GetUniqueIdentifier()];
  EXPECT_EQ(data_str,
            std::string(reinterpret_cast<const char*>(data_back.bytes),
                        data_back.length));
  [cache removeSessionStateDataForWebStateID:web_state_->GetUniqueIdentifier()
                                   incognito:web_state_->GetBrowserState()
                                                 ->IsOffTheRecord()];
  FlushRunLoops();
  EXPECT_FALSE(StorageExists());
}

// Tests that the expected webState storage file is removed on a delay.
TEST_F(WebSessionStateCacheTest, CacheDelayRemove) {
  WebSessionStateCache* cache = GetSessionCache();
  const char data_str[] = "foo";
  NSData* data = [NSData dataWithBytes:data_str length:strlen(data_str)];
  EXPECT_FALSE(StorageExists());
  [cache persistSessionStateData:data
                   forWebStateID:web_state_->GetUniqueIdentifier()];
  FlushRunLoops();
  EXPECT_TRUE(StorageExists());
  [cache setDelayRemove:true];
  [cache removeSessionStateDataForWebStateID:web_state_->GetUniqueIdentifier()
                                   incognito:web_state_->GetBrowserState()
                                                 ->IsOffTheRecord()];
  FlushRunLoops();
  [cache setDelayRemove:false];
  EXPECT_TRUE(StorageExists());
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kRemoveSessionStateDataDelay, ^bool {
    FlushRunLoops();
    return !StorageExists();
  }));
}

}  // namespace
