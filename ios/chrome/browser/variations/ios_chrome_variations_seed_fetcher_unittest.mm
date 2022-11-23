// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_variations_seed_fetcher.h"

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "components/variations/seed_response.h"
#import "components/variations/variations_switches.h"
#import "components/variations/variations_url_constants.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_seed_fetcher+testing.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_seed_store.h"
#import "net/http/http_status_code.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using variations::kDefaultServerUrl;
using variations::switches::kFakeVariationsChannel;
using variations::switches::kVariationsServerURL;

// Histogram names for seed fetch time and result.
const char kSeedFetchResultHistogram[] =
    "IOS.Variations.FirstRun.SeedFetchResult";
const char kSeedFetchTimeHistogram[] = "IOS.Variations.FirstRun.SeedFetchTime";

// Fake server url.
const NSString* testServer = @"https://test.finch.server";

// Type definition of a block that retrieves the value of an HTTP header from a
// custom NSDictionary instead of actual NSURLHTTPRequest header.
typedef void (^MockValueForHTTPHeaderField)(NSInvocation*);

// Retrieve the block that could substitute for -[NSHTTPURLResponse
// valueForHTTPHeaderField:].
MockValueForHTTPHeaderField GetMockMethodWithHeader(
    NSDictionary<NSString*, NSString*>* headers) {
  void (^outputBlock)(NSInvocation*) = ^(NSInvocation* invocation) {
    __weak NSString* arg;
    [invocation getArgument:&arg atIndex:2];
    __weak NSString* value = headers[arg];
    [invocation setReturnValue:&value];
  };
  return outputBlock;
}

}  // namespace

// A test implementation of IOSChromeVariationsSeedFetcher to be used by
// test cases. This avoids writing to global variable for the downloaded seed.
// TODO(crbug.com/1379016): Refactor code so that `fetchingEnabled` could be
// overridden in a test init method of `IOSChromeVariationsSeedFetcher`.
@interface TestVariationsSeedFetcher : IOSChromeVariationsSeedFetcher

// Exposure of parent class property.
@property(nonatomic, assign) BOOL fetchingEnabled;

// Initializer with designated arguments that substitutes for command line args.
- (instancetype)initWithCommandLineArgsForTesting:
    (NSArray<NSString*>*)arguments;

@end

@implementation TestVariationsSeedFetcher

- (instancetype)initWithCommandLineArgsForTesting:
    (NSArray<NSString*>*)arguments {
  self = [super init];
  if (self) {
    self.fetchingEnabled = NO;
    // This overrides `self.fetchingEnabled` if variations server URL is set in
    // the argument.
    [self applySwitchesFromArguments:arguments];
  }
  return self;
}

@end

#pragma mark - Tests

// Tests the IOSChromeVariationsSeedFetcher.
class IOSChromeVariationsSeedFetcherTest : public PlatformTest {
 protected:
  void TearDown() override {
    [IOSChromeVariationsSeedStore popSeed];
    [IOSChromeVariationsSeedFetcher resetFetchingStatusForTesting];
    PlatformTest::TearDown();
  }
};

// Tests that the variations url is correct under different inputs in the
// command line.
TEST_F(IOSChromeVariationsSeedFetcherTest, TestVariationsUrl) {
  NSString* testChannel = @"fake_channel";
  NSString* testServerArgument =
      [NSString stringWithFormat:@"--%@=%@",
                                 base::SysUTF8ToNSString(kVariationsServerURL),
                                 testServer];
  NSString* testChannelArgument = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       testChannel];
  // No arguments; use default value.
  TestVariationsSeedFetcher* fetcherWithDefaultArgs =
      [[TestVariationsSeedFetcher alloc] initWithCommandLineArgsForTesting:@[]];
  NSString* expectedUrlPrefix =
      [NSString stringWithFormat:@"%@?osname=ios&milestone=",
                                 base::SysUTF8ToNSString(kDefaultServerUrl)];
  EXPECT_TRUE([fetcherWithDefaultArgs.variationsUrl.absoluteString
      hasPrefix:expectedUrlPrefix]);
  // Valid server url and channel arguments.
  NSString* expectedUrl = [NSString
      stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@", testServer,
                       base::SysUTF8ToNSString(
                           version_info::GetMajorVersionNumber()),
                       testChannel];
  TestVariationsSeedFetcher* fetcherWithValidArgs =
      [[TestVariationsSeedFetcher alloc] initWithCommandLineArgsForTesting:@[
        testServerArgument, testChannelArgument
      ]];
  EXPECT_NSEQ(fetcherWithValidArgs.variationsUrl.absoluteString, expectedUrl);
  // Valid server url argument; no channel argument.
  expectedUrlPrefix = [NSString stringWithFormat:@"%@?osname=ios", testServer];
  fetcherWithValidArgs = [[TestVariationsSeedFetcher alloc]
      initWithCommandLineArgsForTesting:@[ testServerArgument ]];
  EXPECT_TRUE([fetcherWithValidArgs.variationsUrl.absoluteString
      hasPrefix:expectedUrlPrefix]);
  EXPECT_FALSE([fetcherWithValidArgs.variationsUrl.absoluteString
      containsString:testChannel]);
  // Valid channel argument; no server url argument.
  expectedUrl =
      [NSString stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@",
                                 base::SysUTF8ToNSString(kDefaultServerUrl),
                                 base::SysUTF8ToNSString(
                                     version_info::GetMajorVersionNumber()),
                                 testChannel];
  fetcherWithValidArgs = [[TestVariationsSeedFetcher alloc]
      initWithCommandLineArgsForTesting:@[ testChannelArgument ]];
  EXPECT_NSEQ(fetcherWithValidArgs.variationsUrl.absoluteString, expectedUrl);
}

// Tests that the request to the finch server would not be made when seed
// fetching is not enabled.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testThatRequestIsNotMadeWhenFetchSeedNotEnabled) {
  // Attach mock delegate.
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  // Start fetching seed from fetcher. Seed fetching is disabled by default in
  // tests.
  base::HistogramTester histogramTester;
  TestVariationsSeedFetcher* fetcher =
      [[TestVariationsSeedFetcher alloc] initWithCommandLineArgsForTesting:@[]];
  fetcher.delegate = delegate;
  [fetcher startSeedFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(delegate);
  histogramTester.ExpectTotalCount(kSeedFetchTimeHistogram, 0);
  histogramTester.ExpectTotalCount(kSeedFetchResultHistogram, 0);
}

// Tests that the request to the finch server would be made when seed fetching
// is enabled.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testThatRequestIsMadeWhenFetchSeedEnabled) {
  // Instantiate mocks.
  BOOL (^requestMatcher)(NSMutableURLRequest* request) =
      ^BOOL(NSMutableURLRequest* request) {
        // The NSString method `hasPrefix` does not take parameter of type
        // 'const NSString *__strong`.
        NSString* prefix = [NSString stringWithFormat:@"%@", testServer];
        return [request.URL.absoluteString hasPrefix:prefix] &&
               [request.allHTTPHeaderFields[@"A-IM"] isEqualToString:@"gzip"];
      };
  id mockURLSession = OCMClassMock([NSURLSession class]);
  OCMStub([mockURLSession sharedSession]).andReturn(mockURLSession);
  OCMExpect([mockURLSession
      dataTaskWithRequest:[OCMArg checkWithBlock:requestMatcher]
        completionHandler:[OCMArg any]]);
  // Start fetching seed from fetcher. Pass a fake value for
  // `kVariationsServerURL` to enable testing.
  NSString* argument =
      [NSString stringWithFormat:@"--%@=%@",
                                 base::SysUTF8ToNSString(kVariationsServerURL),
                                 testServer];
  TestVariationsSeedFetcher* fetcher = [[TestVariationsSeedFetcher alloc]
      initWithCommandLineArgsForTesting:@[ argument ]];
  [fetcher startSeedFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(mockURLSession);
}

// Tests that the seed would not be created when there is a request with the
// HTTP response, and that the delegate would be notified so.
TEST_F(IOSChromeVariationsSeedFetcherTest, testHTTPResponseError) {
  // Instantiate mocks and expectation.
  id timeoutError = OCMClassMock([NSError class]);
  OCMStub([timeoutError code]).andReturn(NSURLErrorTimedOut);
  id responseOk = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseOk statusCode]).andReturn(net::HTTP_OK);
  id responseError = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseError statusCode]).andReturn(net::HTTP_NOT_FOUND);
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  // Test if onSeedRequestCompletedWithData:response:error correctly handles
  // NSError and unexpected response code.
  TestVariationsSeedFetcher* fetcher =
      [[TestVariationsSeedFetcher alloc] initWithCommandLineArgsForTesting:@[]];
  fetcher.delegate = delegate;
  fetcher.startTimeOfOngoingSeedRequest = base::Time::Now();
  // Test timeout error.
  base::HistogramTester histogramTesterTimeoutError;
  [fetcher onSeedRequestCompletedWithData:nil
                                 response:responseOk
                                    error:timeoutError];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ([IOSChromeVariationsSeedStore popSeed], nil);
  EXPECT_TRUE(fetcher.startTimeOfOngoingSeedRequest.is_null());
  histogramTesterTimeoutError.ExpectTotalCount(kSeedFetchTimeHistogram, 0);
  histogramTesterTimeoutError.ExpectUniqueSample(
      kSeedFetchResultHistogram, IOSSeedFetchException::kHTTPSRequestTimeout,
      1);
  // Test response code error.
  base::HistogramTester histogramTesterNotFound;
  fetcher.startTimeOfOngoingSeedRequest = base::Time::Now();
  [fetcher onSeedRequestCompletedWithData:nil response:responseError error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ([IOSChromeVariationsSeedStore popSeed], nil);
  EXPECT_TRUE(fetcher.startTimeOfOngoingSeedRequest.is_null());
  EXPECT_OCMOCK_VERIFY(delegate);
  histogramTesterNotFound.ExpectTotalCount(kSeedFetchTimeHistogram, 0);
  histogramTesterNotFound.ExpectUniqueSample(kSeedFetchResultHistogram,
                                             net::HTTP_NOT_FOUND, 1);
}

// Tests that the seed creation would be attempted when there is a request with
// the HTTP response, and that the delegate would be notified if it fails.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testValidHTTPResponseWithFailingSeedCreation) {
  // Setup.
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  TestVariationsSeedFetcher* rawfetcher =
      [[TestVariationsSeedFetcher alloc] initWithCommandLineArgsForTesting:@[]];
  rawfetcher.delegate = delegate;
  rawfetcher.startTimeOfOngoingSeedRequest = base::Time::NowFromSystemTime();
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response statusCode]).andReturn(net::HTTP_OK);
  // Execute test.
  id fetcherWithSeed = OCMPartialMock(rawfetcher);
  OCMStub([fetcherWithSeed seedResponseForHTTPResponse:response data:nil])
      .andDo(nil);
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  base::HistogramTester histogramTester;
  [fetcherWithSeed onSeedRequestCompletedWithData:nil
                                         response:response
                                            error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_TRUE([fetcherWithSeed startTimeOfOngoingSeedRequest].is_null());
  EXPECT_EQ([IOSChromeVariationsSeedStore popSeed], nil);
  EXPECT_OCMOCK_VERIFY(delegate);
  histogramTester.ExpectTotalCount(kSeedFetchTimeHistogram, 1);
  histogramTester.ExpectUniqueSample(
      kSeedFetchResultHistogram, IOSSeedFetchException::kInvalidIMHeader, 1);
}

// Tests that the seed would not be created when the instance manipulation
// header format is wrong.
TEST_F(IOSChromeVariationsSeedFetcherTest, testInvalidInstanceManipulation) {
  NSString* signature = [NSDate now].description;
  NSString* country = @"US";
  NSDictionary<NSString*, NSString*>* headersWithoutIM = @{
    @"X-Seed-Signature" : signature,
    @"X-Country" : country,
  };
  TestVariationsSeedFetcher* fetcher =
      [[TestVariationsSeedFetcher alloc] initWithCommandLineArgsForTesting:@[]];
  // No IM.
  id responseNoIM = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseNoIM valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headersWithoutIM));
  std::unique_ptr<variations::SeedResponse> seed =
      [fetcher seedResponseForHTTPResponse:responseNoIM data:nil];
  EXPECT_EQ(seed, nullptr);
  // Incorrect IM.
  NSMutableDictionary<NSString*, NSString*>* headerWithBadIM =
      [NSMutableDictionary dictionaryWithDictionary:headersWithoutIM];
  headerWithBadIM[@"IM"] = @"not_gzip";
  id responseBadIM = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseBadIM valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headerWithBadIM));
  seed = [fetcher seedResponseForHTTPResponse:responseBadIM data:nil];
  EXPECT_EQ(seed, nullptr);
  // More IM than expected.
  NSMutableDictionary<NSString*, NSString*>* headerWithTwoIMs =
      [NSMutableDictionary dictionaryWithDictionary:headersWithoutIM];
  headerWithTwoIMs[@"IM"] = @"gzip,somethingelse";
  id responseTwoIMs = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseTwoIMs valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headerWithTwoIMs));
  seed = [fetcher seedResponseForHTTPResponse:responseTwoIMs data:nil];
  EXPECT_EQ(seed, nullptr);
}

// Tests that the seed would be created with property values extracted from the
// HTTP response with expected header format, and that the delegate would be
// notified of the successful seed creation.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testValidHTTPResponseWithSuccessfulSeedCreation) {
  NSString* signature = [NSDate now].description;
  NSString* country = @"US";
  NSDictionary<NSString*, NSString*>* headers = @{
    @"X-Seed-Signature" : signature,
    @"X-Country" : country,
    @"IM" : @" gzip"
  };
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response statusCode]).andReturn(net::HTTP_OK);
  OCMStub([response valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headers));
  // Setup.
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  TestVariationsSeedFetcher* fetcherWithSeed =
      [[TestVariationsSeedFetcher alloc] initWithCommandLineArgsForTesting:@[]];
  fetcherWithSeed.delegate = delegate;
  fetcherWithSeed.startTimeOfOngoingSeedRequest =
      base::Time::NowFromSystemTime();
  // Execute test.
  base::HistogramTester histogramTester;
  OCMExpect([delegate didFetchSeedSuccess:YES]);
  [fetcherWithSeed onSeedRequestCompletedWithData:nil
                                         response:response
                                            error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_TRUE([fetcherWithSeed startTimeOfOngoingSeedRequest].is_null());
  auto seed = [IOSChromeVariationsSeedStore popSeed];
  ASSERT_NE(seed, nullptr);
  EXPECT_EQ(seed->signature, base::SysNSStringToUTF8(signature));
  EXPECT_EQ(seed->country, base::SysNSStringToUTF8(country));
  EXPECT_EQ(seed->data, "");
  EXPECT_OCMOCK_VERIFY(delegate);
  histogramTester.ExpectTotalCount(kSeedFetchTimeHistogram, 1);
  histogramTester.ExpectUniqueSample(kSeedFetchResultHistogram, net::HTTP_OK,
                                     1);
}
