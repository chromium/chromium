// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher.h"

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/variations/seed_response.h"
#import "components/variations/variations_switches.h"
#import "components/variations/variations_url_constants.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/variations/model/constants.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"
#import "net/http/http_status_code.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// The following headers should be imported after their non-testing
// counterparts.
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher+testing.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+testing.h"

namespace {

using variations::kDefaultServerUrl;
using variations::switches::kFakeVariationsChannel;
using variations::switches::kVariationsServerURL;

// Histogram names for seed fetch time and result.
const char kSeedFetchResultHistogram[] =
    "IOS.Variations.FirstRun.SeedFetchResult";
const char kSeedFetchTimeHistogram[] = "IOS.Variations.FirstRun.SeedFetchTime";

// Fake server url.
const NSString* test_server = @"https://test.finch.server";

// Type definition of a block that retrieves the value of an HTTP header from a
// custom NSDictionary instead of actual NSURLHTTPRequest header.
typedef void (^MockValueForHTTPHeaderField)(NSInvocation*);

// Retrieve the block that could substitute for -[NSHTTPURLResponse
// valueForHTTPHeaderField:].
MockValueForHTTPHeaderField GetMockMethodWithHeader(
    NSDictionary<NSString*, NSString*>* headers) {
  void (^output_block)(NSInvocation*) = ^(NSInvocation* invocation) {
    __weak NSString* arg;
    [invocation getArgument:&arg atIndex:2];
    __weak NSString* value = headers[arg];
    [invocation setReturnValue:&value];
  };
  return output_block;
}

}  // namespace

#pragma mark - Tests

// Tests the IOSChromeVariationsSeedFetcher.
class IOSChromeVariationsSeedFetcherTest : public PlatformTest {
 protected:
  void TearDown() override {
    [IOSChromeVariationsSeedStore resetForTesting];
    [IOSChromeVariationsSeedFetcher resetFetchingStatusForTesting];
    PlatformTest::TearDown();
  }
};

// Tests that the request to the finch server would not be made when seed
// fetching is not enabled.
//
// Note: this would happen only when build is NOT Google Chrome branded.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testThatRequestIsNotMadeWhenFetchSeedNotEnabled) {
  // Attach mock delegate.
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  OCMExpect([delegate variationsSeedFetcherDidCompleteFetchWithSuccess:NO]);
  // Start fetching seed from fetcher. Seed fetching is disabled by default in
  // tests.
  base::HistogramTester histogram_tester;
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  fetcher.delegate = delegate;
  [fetcher startSeedFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(delegate);
  histogram_tester.ExpectTotalCount(kSeedFetchTimeHistogram, 0);
  histogram_tester.ExpectTotalCount(kSeedFetchResultHistogram, 0);
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests that the request to the finch server would be made when seed fetching
// is enabled.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testThatRequestIsMadeWhenFetchSeedEnabled) {
  // Instantiate mocks.
  BOOL (^request_matcher)(NSMutableURLRequest* request) =
      ^BOOL(NSMutableURLRequest* request) {
        // The NSString method `hasPrefix` does not take parameter of type
        // 'const NSString *__strong`.
        NSString* prefix = [NSString stringWithFormat:@"%@", test_server];
        return [request.URL.absoluteString hasPrefix:prefix] &&
               [request.allHTTPHeaderFields[@"A-IM"] isEqualToString:@"gzip"];
      };
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  OCMExpect([mock_url_session
      dataTaskWithRequest:[OCMArg checkWithBlock:request_matcher]
        completionHandler:[OCMArg any]]);
  // Start fetching seed from fetcher. Pass a fake value for
  // `kVariationsServerURL` to enable testing.
  NSString* argument =
      [NSString stringWithFormat:@"--%@=%@",
                                 base::SysUTF8ToNSString(kVariationsServerURL),
                                 test_server];
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[ argument ]];
  [fetcher startSeedFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(mock_url_session);
}

// Tests that the default variations url is correct.
TEST_F(IOSChromeVariationsSeedFetcherTest, TestDefaultVariationsURL) {
  // Instantiate mocks and expectations.
  BOOL (^request_matcher)(NSMutableURLRequest* request) =
      ^BOOL(NSMutableURLRequest* request) {
        NSString* expected_url_prefix = [NSString
            stringWithFormat:@"%@?osname=ios&milestone=",
                             base::SysUTF8ToNSString(kDefaultServerUrl)];
        return [request.URL.absoluteString hasPrefix:expected_url_prefix];
      };
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  OCMExpect([mock_url_session
      dataTaskWithRequest:[OCMArg checkWithBlock:request_matcher]
        completionHandler:[OCMArg any]]);
  // Fetch and check.
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  [fetcher doActualFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(mock_url_session);
}

// Tests that the variations url is correct with a fake channel.
TEST_F(IOSChromeVariationsSeedFetcherTest, TestVariationsURLWithFakeChannel) {
  NSString* test_channel = @"fake_channel";
  // Instantiate mocks.
  BOOL (^request_matcher)(NSMutableURLRequest* request) =
      ^BOOL(NSMutableURLRequest* request) {
        NSString* expected_url = [NSString
            stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@",
                             base::SysUTF8ToNSString(kDefaultServerUrl),
                             base::SysUTF8ToNSString(
                                 version_info::GetMajorVersionNumber()),
                             test_channel];
        return [request.URL.absoluteString isEqualToString:expected_url];
      };
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  OCMExpect([mock_url_session
      dataTaskWithRequest:[OCMArg checkWithBlock:request_matcher]
        completionHandler:[OCMArg any]]);

  // Pass a fake value for `kFakeVariationsChannel` to make sure it's
  // represented in the URL.
  NSString* test_channel_argument = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       test_channel];
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_argument ]];
  [fetcher doActualFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(mock_url_session);
}

// Tests that the variations url is correct with a fake channel and fake
// server.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       TestVariationsURLWithFakeChannelAndFakeServer) {
  NSString* test_channel = @"fake_channel";
  // Instantiate mocks.
  BOOL (^request_matcher)(NSMutableURLRequest* request) = ^BOOL(
      NSMutableURLRequest* request) {
    NSString* expected_url = [NSString
        stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@", test_server,
                         base::SysUTF8ToNSString(
                             version_info::GetMajorVersionNumber()),
                         test_channel];
    return [request.URL.absoluteString isEqualToString:expected_url];
  };
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  OCMExpect([mock_url_session
      dataTaskWithRequest:[OCMArg checkWithBlock:request_matcher]
        completionHandler:[OCMArg any]]);

  // Pass a fake value for `kFakeVariationsChannel` to make sure it's
  // represented in the URL.
  NSString* test_channel_argument = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       test_channel];
  // Pass a fake value for `kVariationsServerURL` to enable testing.
  NSString* test_server_argument =
      [NSString stringWithFormat:@"--%@=%@",
                                 base::SysUTF8ToNSString(kVariationsServerURL),
                                 test_server];

  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_argument, test_server_argument ]];
  [fetcher doActualFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(mock_url_session);
}

// Tests that the see won't be created when an HTTP timeout error occurs, and
// that the delegate would be notified so.
TEST_F(IOSChromeVariationsSeedFetcherTest, testHTTPTimeoutError) {
  base::HistogramTester histogram_tester;
  // Instantiate the mocks
  id timeout_error = OCMClassMock([NSError class]);
  OCMStub([timeout_error code]).andReturn(NSURLErrorTimedOut);
  id response_ok = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response_ok statusCode]).andReturn(net::HTTP_OK);
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  OCMExpect([delegate variationsSeedFetcherDidCompleteFetchWithSuccess:NO]);
  // Invalidate the NSURLSession so the original request completion handler
  // would not be executed.
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  // Start the test.
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  fetcher.delegate = delegate;
  [fetcher doActualFetch];
  [fetcher seedRequestDidCompleteWithData:nil
                                 response:response_ok
                                    error:timeout_error];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ([IOSChromeVariationsSeedStore popSeed], nil);
  histogram_tester.ExpectTotalCount(kSeedFetchTimeHistogram, 0);
  histogram_tester.ExpectUniqueSample(
      kSeedFetchResultHistogram, IOSSeedFetchException::kHTTPSRequestTimeout,
      1);
}

// Tests that the see won't be created when the response code is unexpected, and
// that the delegate would be notified so.
TEST_F(IOSChromeVariationsSeedFetcherTest, testHTTPResponseCodeError) {
  base::HistogramTester histogram_tester;
  // Instantiate mocks.
  id response_error = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response_error statusCode]).andReturn(net::HTTP_NOT_FOUND);
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  OCMExpect([delegate variationsSeedFetcherDidCompleteFetchWithSuccess:NO]);
  // Invalidate the NSURLSession so the original request completion handler
  // would not be executed.
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  // Start the test.
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  fetcher.delegate = delegate;
  [fetcher doActualFetch];
  [fetcher seedRequestDidCompleteWithData:nil
                                 response:response_error
                                    error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ([IOSChromeVariationsSeedStore popSeed], nil);
  EXPECT_OCMOCK_VERIFY(delegate);
  histogram_tester.ExpectTotalCount(kSeedFetchTimeHistogram, 0);
  histogram_tester.ExpectUniqueSample(kSeedFetchResultHistogram,
                                      net::HTTP_NOT_FOUND, 1);
}

// Tests that the seed creation would be attempted when there is a request
// with the HTTP response, and that the delegate would be notified if it
// fails.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testValidHTTPResponseWithFailingSeedCreation) {
  base::HistogramTester histogram_tester;
  // Setup.
  id delegate =
      OCMProtocolMock(@protocol(IOSChromeVariationsSeedFetcherDelegate));
  OCMExpect([delegate variationsSeedFetcherDidCompleteFetchWithSuccess:NO]);
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  fetcher.delegate = delegate;
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response statusCode]).andReturn(net::HTTP_OK);
  // Invalidate the NSURLSession so the original request completion handler
  // would not be executed.
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  // Execute test.
  [fetcher doActualFetch];
  [fetcher seedRequestDidCompleteWithData:nil response:response error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ([IOSChromeVariationsSeedStore popSeed], nil);
  EXPECT_OCMOCK_VERIFY(delegate);
  histogram_tester.ExpectTotalCount(kSeedFetchTimeHistogram, 1);
  histogram_tester.ExpectUniqueSample(
      kSeedFetchResultHistogram, IOSSeedFetchException::kInvalidIMHeader, 1);
}

// Tests that the seed would not be created when the instance manipulation
// header does not exist.
TEST_F(IOSChromeVariationsSeedFetcherTest, testNoIMHeader) {
  // Set up.
  NSDictionary<NSString*, NSString*>* header = @{
    @"X-Seed-Signature" : [NSDate now].description,
    @"X-Country" : @"US",
  };
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(header));
  // Execute test.
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  std::unique_ptr<variations::SeedResponse> seed =
      [fetcher seedResponseForHTTPResponse:response data:nil];
  EXPECT_EQ(seed, nullptr);
}

// Tests that the seed would not be created when the instance manipulation
// header is not "gzip".
TEST_F(IOSChromeVariationsSeedFetcherTest, testBadIMHeader) {
  // Set up.
  NSDictionary<NSString*, NSString*>* header = @{
    @"X-Seed-Signature" : [NSDate now].description,
    @"X-Country" : @"US",
    @"IM" : @"not_gzip"
  };
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(header));
  // Execute test.
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  std::unique_ptr<variations::SeedResponse> seed =
      [fetcher seedResponseForHTTPResponse:response data:nil];
  EXPECT_EQ(seed, nullptr);
}

// Tests that the seed would not be created when the instance manipulation
// header has more than one value.
TEST_F(IOSChromeVariationsSeedFetcherTest, tesMoreThanOneIMHeaders) {
  // Set up.
  NSDictionary<NSString*, NSString*>* header = @{
    @"X-Seed-Signature" : [NSDate now].description,
    @"X-Country" : @"US",
    @"IM" : @"gzip,somethingelse"
  };
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(header));
  // Execute test.
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  std::unique_ptr<variations::SeedResponse> seed =
      [fetcher seedResponseForHTTPResponse:response data:nil];
  EXPECT_EQ(seed, nullptr);
}

// Tests that the seed would be created with property values extracted from
// the HTTP response with expected header format, and that the delegate would
// be notified of the successful seed creation.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testValidHTTPResponseWithSuccessfulSeedCreation) {
  base::HistogramTester histogram_tester;
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
  OCMExpect([delegate variationsSeedFetcherDidCompleteFetchWithSuccess:YES]);
  IOSChromeVariationsSeedFetcher* fetcher_with_seed =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  fetcher_with_seed.delegate = delegate;
  // Invalidate the NSURLSession so the original request completion handler
  // would not be executed.
  id mock_url_session = OCMClassMock([NSURLSession class]);
  OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
  // Execute test.
  [fetcher_with_seed doActualFetch];
  [fetcher_with_seed seedRequestDidCompleteWithData:nil
                                           response:response
                                              error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  auto seed = [IOSChromeVariationsSeedStore popSeed];
  ASSERT_NE(seed, nullptr);
  EXPECT_EQ(seed->signature, base::SysNSStringToUTF8(signature));
  EXPECT_EQ(seed->country, base::SysNSStringToUTF8(country));
  EXPECT_EQ(seed->data, "");
  EXPECT_OCMOCK_VERIFY(delegate);
  histogram_tester.ExpectTotalCount(kSeedFetchTimeHistogram, 1);
  histogram_tester.ExpectUniqueSample(kSeedFetchResultHistogram, net::HTTP_OK,
                                      1);
}
