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
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
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
#import "url/url_util.h"

// The following headers should be imported after their non-testing
// counterparts.
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher+testing.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+testing.h"

namespace {

using variations::kDefaultServerUrl;
using variations::switches::kFakeVariationsChannel;
using variations::switches::kFakeVariationsPlatform;
using variations::switches::kVariationsServerURL;

// Histogram names for seed fetch time and result.
constexpr char kSeedFetchResultHistogram[] =
    "IOS.Variations.FirstRun.SeedFetchResult";
constexpr char kSeedFetchTimeHistogram[] =
    "IOS.Variations.FirstRun.SeedFetchTime";

// Fake server url.
NSString* const kTestServerUrl = @"https://test.finch.server/";

// Fake channel value for testing.
NSString* const kTestChannel = @"fake_channel";

// Fake value for "Date" header for testing.
NSString* const kTestDateHeaderValue = @"Tue, 15 Nov 1994 12:45:26 GMT";

// Type definition of a block that retrieves the value of an HTTP header from a
// custom NSDictionary instead of actual NSURLHTTPRequest header.
typedef void (^MockValueForHTTPHeaderField)(NSInvocation*);

// Retrieve the block that could substitute for -[NSHTTPURLResponse
// valueForHTTPHeaderField:].
MockValueForHTTPHeaderField GetMockMethodWithHeader(
    NSDictionary<NSString*, NSString*>* headers) {
  void (^output_block)(NSInvocation*) = ^(NSInvocation* invocation) {
    __unsafe_unretained NSString* arg;
    [invocation getArgument:&arg atIndex:2];
    __unsafe_unretained NSString* value = headers[arg];
    [invocation setReturnValue:&value];
  };
  return output_block;
}

}  // namespace

#pragma mark - Tests

// Tests the IOSChromeVariationsSeedFetcher.
class IOSChromeVariationsSeedFetcherTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  }

  void TearDown() override {
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
    [IOSChromeVariationsSeedStore resetForTesting];
    [IOSChromeVariationsSeedFetcher resetFetchingStatusForTesting];
    PlatformTest::TearDown();
  }

  // Runs a synchronous fetch on `fetcher` using a mocked `NSURLSession` and
  // returns the request URL passed to `dataTaskWithRequest:completionHandler:`.
  NSURL* FetchVariationsURL(IOSChromeVariationsSeedFetcher* fetcher) {
    __block NSURL* captured_url = nil;
    id mock_url_session = OCMClassMock([NSURLSession class]);
    OCMStub([mock_url_session sharedSession]).andReturn(mock_url_session);
    OCMExpect([mock_url_session
        dataTaskWithRequest:[OCMArg checkWithBlock:^BOOL(
                                        NSMutableURLRequest* request) {
          captured_url = request.URL;
          return
              [request.allHTTPHeaderFields[@"A-IM"] isEqualToString:@"gzip"] &&
              [[request valueForHTTPHeaderField:@"X-Seed-Key-Ids"]
                  isEqualToString:@"1,2,3,0"];
        }]
          completionHandler:[OCMArg any]]);
    [fetcher doActualFetch];
    EXPECT_OCMOCK_VERIFY(mock_url_session);
    [mock_url_session stopMocking];
    [IOSChromeVariationsSeedFetcher resetFetchingStatusForTesting];
    return captured_url;
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
  EXPECT_OCMOCK_VERIFY(delegate);
}
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Tests that the request to the finch server would be made when seed fetching
// is enabled.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testThatRequestIsMadeWhenFetchSeedEnabled) {
  // Instantiate mocks.
  BOOL (^request_matcher)(NSMutableURLRequest* request) =
      ^BOOL(NSMutableURLRequest* request) {
        return [request.URL.absoluteString hasPrefix:kTestServerUrl] &&
               [request.allHTTPHeaderFields[@"A-IM"] isEqualToString:@"gzip"] &&
               [[request valueForHTTPHeaderField:@"X-Seed-Key-Ids"]
                   isEqualToString:@"1,2,3,0"];
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
                                 kTestServerUrl];
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

// Tests that the variations url is correct with a fake channel and platform.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       TestVariationsURLWithFakeChannelAndPlatform) {
  NSString* test_channel = @"fake_channel";
  NSString* test_platform = @"android";
  // Instantiate mocks.
  BOOL (^request_matcher)(NSMutableURLRequest* request) =
      ^BOOL(NSMutableURLRequest* request) {
        NSString* expected_url = [NSString
            stringWithFormat:@"%@?osname=%@&milestone=%@&channel=%@",
                             base::SysUTF8ToNSString(kDefaultServerUrl),
                             test_platform,
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

  // Pass fake channel and platform values.
  NSString* test_channel_arg = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       test_channel];
  NSString* test_platform_arg = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsPlatform),
                       test_platform];
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_arg, test_platform_arg ]];
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
  BOOL (^request_matcher)(NSMutableURLRequest* request) =
      ^BOOL(NSMutableURLRequest* request) {
        NSString* expected_url = [NSString
            stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@",
                             kTestServerUrl,
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
                                 kTestServerUrl];

  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_argument, test_server_argument ]];
  [fetcher doActualFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(mock_url_session);
}

// Tests that the managed `VariationsRestrictParameter` policy value is appended
// as the `restrict` query parameter to the variations seed request URL.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       TestVariationsURLWithRestrictParameter) {
  NSString* policy_key =
      base::SysUTF8ToNSString(policy::key::kVariationsRestrictParameter);
  [[NSUserDefaults standardUserDefaults]
      setObject:@{policy_key : @"restricted_corp"}
         forKey:kPolicyLoaderIOSConfigurationKey];

  NSString* test_channel_arg = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       kTestChannel];
  IOSChromeVariationsSeedFetcher* fetcher_with_channel =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_arg ]];
  NSURL* url_with_channel = FetchVariationsURL(fetcher_with_channel);
  ASSERT_NSNE(nil, url_with_channel);
  NSString* default_server_url = base::SysUTF8ToNSString(kDefaultServerUrl);
  NSString* expected_url = [NSString
      stringWithFormat:
          @"%@?osname=ios&milestone=%@&channel=%@&restrict=restricted_corp",
          default_server_url,
          base::SysUTF8ToNSString(version_info::GetMajorVersionNumber()),
          kTestChannel];
  EXPECT_NSEQ(expected_url, url_with_channel.absoluteString);

  IOSChromeVariationsSeedFetcher* default_fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] init];
  NSURL* default_url = FetchVariationsURL(default_fetcher);
  ASSERT_NSNE(nil, default_url);
  NSString* default_url_string = default_url.absoluteString;
  NSString* expected_prefix = [NSString
      stringWithFormat:@"%@?osname=ios&milestone=", default_server_url];
  EXPECT_TRUE([default_url_string hasPrefix:expected_prefix]);
  EXPECT_TRUE([default_url_string hasSuffix:@"&restrict=restricted_corp"]);
}

// Tests that surrounding ASCII whitespace is trimmed and special or non-ASCII
// characters in `VariationsRestrictParameter` and `kFakeVariationsChannel` are
// URL-escaped when constructing the seed request URL.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       TestVariationsURLWithEscapedRestrictParameter) {
  NSString* policy_key =
      base::SysUTF8ToNSString(policy::key::kVariationsRestrictParameter);
  [[NSUserDefaults standardUserDefaults]
      setObject:@{policy_key : @"  corp&foo=bar baz+100%/é  "}
         forKey:kPolicyLoaderIOSConfigurationKey];

  NSString* fake_channel_switch =
      base::SysUTF8ToNSString(kFakeVariationsChannel);
  NSString* test_channel_arg =
      [NSString stringWithFormat:@"--%@=%@", fake_channel_switch, kTestChannel];
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_arg ]];
  NSURL* url = FetchVariationsURL(fetcher);
  ASSERT_NSNE(nil, url);
  NSString* milestone =
      base::SysUTF8ToNSString(version_info::GetMajorVersionNumber());
  NSString* expected_restrict = @"corp%26foo%3Dbar+baz%2B100%25%2F%C3%A9";
  NSString* expected_url = [NSString
      stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@&restrict=%@",
                       base::SysUTF8ToNSString(kDefaultServerUrl), milestone,
                       kTestChannel, expected_restrict];
  EXPECT_NSEQ(expected_url, url.absoluteString);

  // Verify escaping at URL construction time when `--variations-server-url`
  // already carries a query parameter and `--fake-variations-channel` contains
  // URL-escapable characters.
  NSString* server_arg =
      [NSString stringWithFormat:@"--%@=https://test.finch.server?existing=1",
                                 base::SysUTF8ToNSString(kVariationsServerURL)];
  NSString* escaped_channel_arg =
      [NSString stringWithFormat:@"--%@=  dev&canary  ", fake_channel_switch];
  IOSChromeVariationsSeedFetcher* custom_fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ server_arg, escaped_channel_arg ]];
  NSURL* custom_url = FetchVariationsURL(custom_fetcher);
  ASSERT_NSNE(nil, custom_url);
  NSString* expected_custom_url =
      [NSString stringWithFormat:@"https://test.finch.server/"
                                 @"?existing=1&osname=ios&milestone=%@&channel="
                                 @"dev%%26canary&restrict=%@",
                                 milestone, expected_restrict];
  EXPECT_NSEQ(expected_custom_url, custom_url.absoluteString);
}

// Tests that the `restrict` query parameter is omitted when
// `VariationsRestrictParameter` is unset, empty, whitespace-only, or not an
// `NSString`.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       TestVariationsURLWithoutRestrictParameterWhenEmptyOrInvalid) {
  NSString* test_channel_arg = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       kTestChannel];
  NSString* expected_unrestricted_url =
      [NSString stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@",
                                 base::SysUTF8ToNSString(kDefaultServerUrl),
                                 base::SysUTF8ToNSString(
                                     version_info::GetMajorVersionNumber()),
                                 kTestChannel];

  // Unset baseline (`kPolicyLoaderIOSConfigurationKey` removed in `SetUp()`).
  IOSChromeVariationsSeedFetcher* baseline_fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_arg ]];
  NSURL* baseline_url = FetchVariationsURL(baseline_fetcher);
  ASSERT_NSNE(nil, baseline_url);
  NSString* baseline_url_string = baseline_url.absoluteString;
  EXPECT_NSEQ(expected_unrestricted_url, baseline_url_string);
  EXPECT_FALSE([baseline_url_string containsString:@"restrict"]);

  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];

  // Empty or non-dictionary top-level managed configuration entries.
  for (id config in @[ @{}, @"not_a_dict" ]) {
    [user_defaults setObject:config forKey:kPolicyLoaderIOSConfigurationKey];
    IOSChromeVariationsSeedFetcher* fetcher =
        [[IOSChromeVariationsSeedFetcher alloc]
            initWithArguments:@[ test_channel_arg ]];
    NSURL* url = FetchVariationsURL(fetcher);
    ASSERT_NSNE(nil, url);
    EXPECT_NSEQ(baseline_url_string, url.absoluteString);
  }

  // Empty, whitespace-only, or non-`NSString` policy values.
  NSString* policy_key =
      base::SysUTF8ToNSString(policy::key::kVariationsRestrictParameter);
  NSArray* invalid_values =
      @[ @"", @"   ", @42, @YES, @[ @"restricted" ], @{@"k" : @"restricted"} ];
  for (id invalid_value in invalid_values) {
    [user_defaults setObject:@{policy_key : invalid_value}
                      forKey:kPolicyLoaderIOSConfigurationKey];
    IOSChromeVariationsSeedFetcher* fetcher =
        [[IOSChromeVariationsSeedFetcher alloc]
            initWithArguments:@[ test_channel_arg ]];
    NSURL* url = FetchVariationsURL(fetcher);
    ASSERT_NSNE(nil, url);
    EXPECT_NSEQ(baseline_url_string, url.absoluteString);
  }
}

// Tests that building the seed request URL does not use the URL scheme
// registry, since first-run fetches start before web schemes are registered.
// Only DCHECK-enabled builds can detect registry use.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       TestVariationsURLDoesNotUseSchemeRegistry) {
  NSString* test_channel_arg = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       kTestChannel];
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ test_channel_arg ]];

  // Marks the scheme registry as unused, as it is during early startup.
  url::ScopedSchemeRegistryForTests scoped_registry;
  NSURL* url = FetchVariationsURL(fetcher);
  ASSERT_NSNE(nil, url);

  // Adding a scheme hits a DCHECK if the registry was read while building the
  // request URL, mirroring the crash in `web::RegisterWebSchemes()`.
  url::AddStandardScheme("variations-seed-fetcher-test", url::SCHEME_WITH_HOST);
}

// Tests that query parameters already in `--variations-server-url` are kept,
// including ones containing `=`, and that a matching parameter is replaced.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       TestVariationsURLWithExistingServerQueryParameters) {
  NSString* server_arg = [NSString
      stringWithFormat:@"--%@=https://test.finch.server/seed?a=b=c&osname=old",
                       base::SysUTF8ToNSString(kVariationsServerURL)];
  NSString* test_channel_arg = [NSString
      stringWithFormat:@"--%@=%@",
                       base::SysUTF8ToNSString(kFakeVariationsChannel),
                       kTestChannel];
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc]
          initWithArguments:@[ server_arg, test_channel_arg ]];
  NSURL* url = FetchVariationsURL(fetcher);
  ASSERT_NSNE(nil, url);
  NSString* expected_url = [NSString
      stringWithFormat:
          @"https://test.finch.server/seed?a=b=c&osname=ios&milestone=%@"
          @"&channel=%@",
          base::SysUTF8ToNSString(version_info::GetMajorVersionNumber()),
          kTestChannel];
  EXPECT_NSEQ(expected_url, url.absoluteString);
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
  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_OCMOCK_VERIFY(timeout_error);
  EXPECT_OCMOCK_VERIFY(response_ok);
  EXPECT_OCMOCK_VERIFY(mock_url_session);
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
  EXPECT_OCMOCK_VERIFY(response_error);
  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_OCMOCK_VERIFY(mock_url_session);
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
    @"Date" : kTestDateHeaderValue,
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
  EXPECT_OCMOCK_VERIFY(response);
}

// Tests that the seed would not be created when the instance manipulation
// header is not "gzip".
TEST_F(IOSChromeVariationsSeedFetcherTest, testBadIMHeader) {
  // Set up.
  NSDictionary<NSString*, NSString*>* header = @{
    @"Date" : kTestDateHeaderValue,
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
  EXPECT_OCMOCK_VERIFY(response);
}

// Tests that the seed would not be created when the instance manipulation
// header has more than one value.
TEST_F(IOSChromeVariationsSeedFetcherTest, tesMoreThanOneIMHeaders) {
  // Set up.
  NSDictionary<NSString*, NSString*>* header = @{
    @"Date" : kTestDateHeaderValue,
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
  EXPECT_OCMOCK_VERIFY(response);
}

// Tests that the seed would be created with property values extracted from
// the HTTP response with expected header format, and that the delegate would
// be notified of the successful seed creation.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testValidHTTPResponseWithSuccessfulSeedCreation) {
  base::HistogramTester histogram_tester;
  NSString* signature = [NSDate now].description;
  NSString* country = @"US";
  NSString* geo_level1 = @"us-ca";
  NSDictionary<NSString*, NSString*>* headers = @{
    @"Date" : kTestDateHeaderValue,
    @"X-Seed-Signature" : signature,
    @"X-Country" : country,
    @"X-Geo-Level-1" : geo_level1,
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
  EXPECT_EQ(seed->geo_level1, base::SysNSStringToUTF8(geo_level1));
  EXPECT_EQ(seed->data, "");
  base::Time::Exploded date_exploded;
  seed->date.UTCExplode(&date_exploded);
  EXPECT_EQ(date_exploded.year, 1994);
  EXPECT_EQ(date_exploded.month, 11);
  EXPECT_EQ(date_exploded.day_of_month, 15);
  EXPECT_EQ(date_exploded.hour, 12);
  EXPECT_OCMOCK_VERIFY(delegate);
  histogram_tester.ExpectTotalCount(kSeedFetchTimeHistogram, 1);
  histogram_tester.ExpectUniqueSample(kSeedFetchResultHistogram, net::HTTP_OK,
                                      1);
  EXPECT_OCMOCK_VERIFY(response);
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the seed is still created with an empty geo_level1 string when
// the X-Geo-Level-1 header is omitted from the HTTP response.
TEST_F(IOSChromeVariationsSeedFetcherTest,
       testValidHTTPResponseWithoutGeoLevel1Header) {
  NSDictionary<NSString*, NSString*>* headers = @{
    @"Date" : kTestDateHeaderValue,
    @"X-Seed-Signature" : @"signature",
    @"X-Country" : @"US",
    @"IM" : @"gzip"
  };
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headers));
  IOSChromeVariationsSeedFetcher* fetcher =
      [[IOSChromeVariationsSeedFetcher alloc] initWithArguments:@[]];
  std::unique_ptr<variations::SeedResponse> seed =
      [fetcher seedResponseForHTTPResponse:response data:nil];
  ASSERT_NE(seed, nullptr);
  EXPECT_EQ(seed->country, "US");
  EXPECT_EQ(seed->geo_level1, "");
  EXPECT_OCMOCK_VERIFY(response);
}
