// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/session_state/web_session_state_cache.h"

#import <Foundation/Foundation.h>

#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

constexpr base::TimeDelta kRemoveSessionStateDataDelay = base::Seconds(15);

class WebSessionStateCacheTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    sessionCache_ = [[WebSessionStateCache alloc]
        initWithBrowserState:chrome_browser_state_.get()];

    web::WebState::CreateParams createParams(chrome_browser_state_.get());
    web_state_ = web::WebState::Create(createParams);

    session_cache_directory_ = chrome_browser_state_->GetStatePath().Append(
        kWebSessionCacheDirectoryName);
  }

  bool StorageExists() {
    NSString* sessionID = web_state_.get()->GetStableIdentifier();
    base::FilePath filePath =
        session_cache_directory_.Append(base::SysNSStringToUTF8(sessionID));
    return base::PathExists(filePath);
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
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  WebSessionStateCache* sessionCache_;
};

// Tests that the expected webState storage file is added and removed as
// expected.
TEST_F(WebSessionStateCacheTest, CacheAddAndRemove) {
  WebSessionStateCache* cache = GetSessionCache();
  const char data_str[] = "foo";
  NSData* data = [NSData dataWithBytes:data_str length:strlen(data_str)];
  EXPECT_FALSE(StorageExists());
  [cache persistSessionStateData:data forWebState:web_state_.get()];
  FlushRunLoops();
  EXPECT_TRUE(StorageExists());
  NSData* data_back = [cache sessionStateDataForWebState:web_state_.get()];
  EXPECT_EQ(data_str,
            std::string(reinterpret_cast<const char*>(data_back.bytes),
                        data_back.length));
  [cache removeSessionStateDataForWebState:web_state_.get()];
  FlushRunLoops();
  EXPECT_FALSE(StorageExists());
}

// Tests that the expected webState storage file is removed on a delay.
TEST_F(WebSessionStateCacheTest, CacheDelayRemove) {
  WebSessionStateCache* cache = GetSessionCache();
  const char data_str[] = "foo";
  NSData* data = [NSData dataWithBytes:data_str length:strlen(data_str)];
  EXPECT_FALSE(StorageExists());
  [cache persistSessionStateData:data forWebState:web_state_.get()];
  FlushRunLoops();
  EXPECT_TRUE(StorageExists());
  [cache setDelayRemove:true];
  [cache removeSessionStateDataForWebState:web_state_.get()];
  FlushRunLoops();
  [cache setDelayRemove:false];
  EXPECT_TRUE(StorageExists());
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kRemoveSessionStateDataDelay, ^bool {
    FlushRunLoops();
    return !StorageExists();
  }));
}

}  // namespace
