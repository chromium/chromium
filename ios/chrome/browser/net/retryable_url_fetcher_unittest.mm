// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/retryable_url_fetcher.h"

#import "base/strings/sys_string_conversions.h"
#include "base/test/task_environment.h"
#include "ios/web/public/test/test_web_thread.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// An arbitrary text string for a fake response.
NSString* const kFakeResponseString = @"Something interesting here.";
}

// Delegate object to provide data for RetryableURLFetcher and
// handles the callback when URL is fetched.
@interface TestRetryableURLFetcherDelegate
    : NSObject<RetryableURLFetcherDelegate>
// Counts the number of times that a successful response has been processed.
@property(nonatomic, assign) NSUInteger responsesProcessed;
@end

@implementation TestRetryableURLFetcherDelegate
@synthesize responsesProcessed;

- (NSString*)urlToFetch {
  return @"http://www.google.com";
}

- (void)processSuccessResponse:(NSString*)response {
  if (response) {
    EXPECT_NSEQ(kFakeResponseString, response);
    ++responsesProcessed;
  }
}

@end

@interface TestFailingURLFetcherDelegate : NSObject<RetryableURLFetcherDelegate>
@property(nonatomic, assign) NSUInteger responsesProcessed;
@end

@implementation TestFailingURLFetcherDelegate
@synthesize responsesProcessed;

- (NSString*)urlToFetch {
  return nil;
}

- (void)processSuccessResponse:(NSString*)response {
  EXPECT_FALSE(response);
  ++responsesProcessed;
}

@end

namespace {

class RetryableURLFetcherTest : public PlatformTest {
 public:
  RetryableURLFetcherTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    test_delegate_ = [[TestRetryableURLFetcherDelegate alloc] init];
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  TestRetryableURLFetcherDelegate* test_delegate_;
};

TEST_F(RetryableURLFetcherTest, TestResponse200) {
  RetryableURLFetcher* retryableFetcher = [[RetryableURLFetcher alloc]
      initWithURLLoaderFactory:test_shared_url_loader_factory_
                      delegate:test_delegate_
                 backoffPolicy:nil];

  [test_delegate_ setResponsesProcessed:0U];
  std::string url = base::SysNSStringToUTF8([test_delegate_ urlToFetch]);
  test_url_loader_factory_.AddResponse(
      url, base::SysNSStringToUTF8(kFakeResponseString), net::HTTP_OK);

  [retryableFetcher startFetch];
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1U, [test_delegate_ responsesProcessed]);
}

TEST_F(RetryableURLFetcherTest, TestResponse404) {
  RetryableURLFetcher* retryableFetcher = [[RetryableURLFetcher alloc]
      initWithURLLoaderFactory:test_shared_url_loader_factory_
                      delegate:test_delegate_
                 backoffPolicy:nil];

  [test_delegate_ setResponsesProcessed:0U];
  std::string url = base::SysNSStringToUTF8([test_delegate_ urlToFetch]);
  test_url_loader_factory_.AddResponse(url, "", net::HTTP_NOT_FOUND);

  [retryableFetcher startFetch];
  task_environment_.RunUntilIdle();

  EXPECT_EQ(0U, [test_delegate_ responsesProcessed]);
}

// Tests that response callback method is called if delegate returns an
// invalid URL.
TEST_F(RetryableURLFetcherTest, TestFailingURLNoRetry) {
  TestFailingURLFetcherDelegate* failing_delegate =
      [[TestFailingURLFetcherDelegate alloc] init];
  RetryableURLFetcher* retryableFetcher = [[RetryableURLFetcher alloc]
      initWithURLLoaderFactory:test_shared_url_loader_factory_
                      delegate:failing_delegate
                 backoffPolicy:nil];

  [failing_delegate setResponsesProcessed:0U];

  [retryableFetcher startFetch];
  task_environment_.RunUntilIdle();

  // Verify that a response has been received, even if the failing delegate
  // received a nil in processSuccessResponse to indicate the failure.
  EXPECT_EQ(1U, [failing_delegate responsesProcessed]);
}

}  // namespace
