// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_first_run_variations_seed_manager.h"

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/variations/variations_switches.h"
#import "components/variations/variations_url_constants.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/variations/ios_chrome_seed_response.h"
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

#pragma mark - Test Models

// Extraction of private properties and methods in
// IOSChromeFirstRunVariationsSeedManager to be tested.
@interface IOSChromeFirstRunVariationsSeedManager (Testing)

@property(nonatomic, readonly) NSURL* variationsUrl;

@property(nonatomic, strong) NSDate* startTimeOfOngoingSeedRequest;

- (void)applySwitchesFromArguments:(NSArray<NSString*>*)arguments;

- (void)onSeedRequestCompletedWithData:(NSData*)data
                              response:(NSHTTPURLResponse*)httpResponse
                                 error:(NSError*)error;

- (IOSChromeSeedResponse*)seedResponseForHTTPResponse:
                              (NSHTTPURLResponse*)httpResponse
                                                 data:(NSData*)data;

+ (void)resetFetchingStatusForTesting;

@end

// A test implementation of IOSChromeFirstRunVariationsSeedManager to be used by
// test cases. This avoids writing to global variable for the downloaded seed.
@interface TestVariationsSeedManager : IOSChromeFirstRunVariationsSeedManager {
  // Variations seed fetched that is stored in a TestVariationsSeedManager,
  // instead of globally.
  IOSChromeSeedResponse* _seedResponseForTesting;
}

// Initializer with designated arguments that substitutes for command line args.
- (instancetype)initWithCommandLineArgsForTesting:
    (NSArray<NSString*>*)arguments;

@end

@implementation TestVariationsSeedManager

- (instancetype)initWithCommandLineArgsForTesting:
    (NSArray<NSString*>*)arguments {
  self = [super init];
  if (self) {
    _seedResponseForTesting = nil;
    [self applySwitchesFromArguments:arguments];
  }
  return self;
}

- (IOSChromeSeedResponse*)seed {
  return _seedResponseForTesting;
}

// Overrides IOSChromeFirstRunVariationsSeedManager where the seed is stored to
// a global variable.
- (void)updateSharedSeed:(IOSChromeSeedResponse*)seed {
  _seedResponseForTesting = seed;
}

@end

// Tests the IOSChromeFirstRunVariationsSeedManager.
class IOSChromeFirstRunVariationsSeedManagerTest : public PlatformTest {
 protected:
  void TearDown() override {
    [IOSChromeFirstRunVariationsSeedManager resetFetchingStatusForTesting];
    PlatformTest::TearDown();
  }
};

#pragma mark - Tests

// Tests that the variations url is correct under different inputs in the
// command line.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest, TestVariationsUrl) {
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
  TestVariationsSeedManager* managerWithDefaultArgs =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  NSString* expectedUrlPrefix =
      [NSString stringWithFormat:@"%@?osname=ios&milestone=",
                                 base::SysUTF8ToNSString(kDefaultServerUrl)];
  EXPECT_TRUE([managerWithDefaultArgs.variationsUrl.absoluteString
      hasPrefix:expectedUrlPrefix]);
  // Valid server url and channel arguments.
  NSString* expectedUrl = [NSString
      stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@", testServer,
                       base::SysUTF8ToNSString(
                           version_info::GetMajorVersionNumber()),
                       testChannel];
  TestVariationsSeedManager* managerWithValidArgs =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[
        testServerArgument, testChannelArgument
      ]];
  EXPECT_NSEQ(managerWithValidArgs.variationsUrl.absoluteString, expectedUrl);
  // Valid server url argument; no channel argument.
  expectedUrlPrefix = [NSString stringWithFormat:@"%@?osname=ios", testServer];
  managerWithValidArgs = [[TestVariationsSeedManager alloc]
      initWithCommandLineArgsForTesting:@[ testServerArgument ]];
  EXPECT_TRUE([managerWithValidArgs.variationsUrl.absoluteString
      hasPrefix:expectedUrlPrefix]);
  EXPECT_FALSE([managerWithValidArgs.variationsUrl.absoluteString
      containsString:testChannel]);
  // Valid channel argument; no server url argument.
  expectedUrl =
      [NSString stringWithFormat:@"%@?osname=ios&milestone=%@&channel=%@",
                                 base::SysUTF8ToNSString(kDefaultServerUrl),
                                 base::SysUTF8ToNSString(
                                     version_info::GetMajorVersionNumber()),
                                 testChannel];
  managerWithValidArgs = [[TestVariationsSeedManager alloc]
      initWithCommandLineArgsForTesting:@[ testChannelArgument ]];
  EXPECT_NSEQ(managerWithValidArgs.variationsUrl.absoluteString, expectedUrl);
}

// Tests that the request to the finch server would not be made when seed
// fetching is not enabled.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest,
       testThatRequestIsNotMadeWhenFetchSeedNotEnabled) {
  // Attach mock delegate.
  id delegate = OCMProtocolMock(
      @protocol(IOSChromeFirstRunVariationsSeedManagerDelegate));
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  // Start fetching seed from manager. Seed fetching is disabled by default in
  // tests.
  TestVariationsSeedManager* manager =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  manager.delegate = delegate;
  [manager startSeedFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the request to the finch server would be made when seed fetching
// is enabled.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest,
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
  // Start fetching seed from manager. Pass a fake value for
  // `kVariationsServerURL` to enable testing.
  NSString* argument =
      [NSString stringWithFormat:@"--%@=%@",
                                 base::SysUTF8ToNSString(kVariationsServerURL),
                                 testServer];
  TestVariationsSeedManager* manager = [[TestVariationsSeedManager alloc]
      initWithCommandLineArgsForTesting:@[ argument ]];
  [manager startSeedFetch];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_OCMOCK_VERIFY(mockURLSession);
}

// Tests that the seed would not be created when there is a request with the
// HTTP response, and that the delegate would be notified so.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest, testHTTPResponseError) {
  // Instantiate mocks and expectation.
  id error = OCMClassMock([NSError class]);
  id responseOk = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseOk statusCode]).andReturn(net::HTTP_OK);
  id responseError = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseError statusCode]).andReturn(net::HTTP_NOT_FOUND);
  id delegate = OCMProtocolMock(
      @protocol(IOSChromeFirstRunVariationsSeedManagerDelegate));
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  // Test if onSeedRequestCompletedWithData:response:error correctly handles
  // NSError and unexpected response code.
  TestVariationsSeedManager* manager =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  manager.delegate = delegate;
  manager.startTimeOfOngoingSeedRequest = [NSDate now];
  [manager onSeedRequestCompletedWithData:nil response:responseOk error:error];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ(manager.seed, nil);
  EXPECT_EQ(manager.startTimeOfOngoingSeedRequest, nil);
  manager.startTimeOfOngoingSeedRequest = [NSDate now];
  [manager onSeedRequestCompletedWithData:nil response:responseError error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ(manager.seed, nil);
  EXPECT_EQ(manager.startTimeOfOngoingSeedRequest, nil);
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the seed creation would be attempted when there is a request with
// the HTTP response, and that the delegate would be notified if it succeeds.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest,
       testValidHTTPResponseWithSuccessfulSeedCreation) {
  // Setup.
  id delegate = OCMProtocolMock(
      @protocol(IOSChromeFirstRunVariationsSeedManagerDelegate));
  TestVariationsSeedManager* rawManager =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  rawManager.delegate = delegate;
  rawManager.startTimeOfOngoingSeedRequest = [NSDate now];
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response statusCode]).andReturn(net::HTTP_OK);
  IOSChromeSeedResponse* expectedSeed =
      [[IOSChromeSeedResponse alloc] initWithSignature:@""
                                               country:@""
                                                  time:[NSDate now]
                                                  data:nil
                                            compressed:YES];
  // Execute test.
  id managerWithSeed = OCMPartialMock(rawManager);
  OCMStub([managerWithSeed seedResponseForHTTPResponse:response data:nil])
      .andReturn(expectedSeed);
  OCMExpect([delegate didFetchSeedSuccess:YES]);
  [managerWithSeed onSeedRequestCompletedWithData:nil
                                         response:response
                                            error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ([managerWithSeed startTimeOfOngoingSeedRequest], nil);
  EXPECT_EQ([managerWithSeed seed], expectedSeed);
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the seed creation would be attempted when there is a request with
// the HTTP response, and that the delegate would be notified if it fails.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest,
       testValidHTTPResponseWithFailingSeedCreation) {
  // Setup.
  id delegate = OCMProtocolMock(
      @protocol(IOSChromeFirstRunVariationsSeedManagerDelegate));
  TestVariationsSeedManager* rawManager =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  rawManager.delegate = delegate;
  rawManager.startTimeOfOngoingSeedRequest = [NSDate now];
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response statusCode]).andReturn(net::HTTP_OK);
  // Execute test.
  id managerWithSeed = OCMPartialMock(rawManager);
  OCMStub([managerWithSeed seedResponseForHTTPResponse:response data:nil])
      .andDo(nil);
  OCMExpect([delegate didFetchSeedSuccess:NO]);
  [managerWithSeed onSeedRequestCompletedWithData:nil
                                         response:response
                                            error:nil];
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(0.05));
  EXPECT_EQ([managerWithSeed startTimeOfOngoingSeedRequest], nil);
  EXPECT_EQ([managerWithSeed seed], nil);
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that the seed would not be created when the instance manipulation
// header format is wrong.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest,
       testInvalidInstanceManipulation) {
  NSString* signature = [NSDate now].description;
  NSString* country = @"US";
  NSDictionary<NSString*, NSString*>* headersWithoutIM = @{
    @"X-Seed-Signature" : signature,
    @"X-Country" : country,
  };
  TestVariationsSeedManager* manager =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  // No IM.
  id responseNoIM = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseNoIM valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headersWithoutIM));
  IOSChromeSeedResponse* seed =
      [manager seedResponseForHTTPResponse:responseNoIM data:nil];
  EXPECT_EQ(seed, nil);
  // Incorrect IM.
  NSMutableDictionary<NSString*, NSString*>* headerWithBadIM =
      [NSMutableDictionary dictionaryWithDictionary:headersWithoutIM];
  headerWithBadIM[@"IM"] = @"not_gzip";
  id responseBadIM = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseBadIM valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headerWithBadIM));
  seed = [manager seedResponseForHTTPResponse:responseBadIM data:nil];
  EXPECT_EQ(seed, nil);
  // More IM than expected.
  NSMutableDictionary<NSString*, NSString*>* headerWithTwoIMs =
      [NSMutableDictionary dictionaryWithDictionary:headersWithoutIM];
  headerWithTwoIMs[@"IM"] = @"gzip,somethingelse";
  id responseTwoIMs = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([responseTwoIMs valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headerWithTwoIMs));
  seed = [manager seedResponseForHTTPResponse:responseTwoIMs data:nil];
  EXPECT_EQ(seed, nil);
}

// Tests that the seed would be created with property values extracted from the
// HTTP response with expected header format.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest, testValidSeedResponse) {
  NSString* signature = [NSDate now].description;
  NSString* country = @"US";
  NSDictionary<NSString*, NSString*>* headers = @{
    @"X-Seed-Signature" : signature,
    @"X-Country" : country,
    @"IM" : @" gzip , ,"  // Test with comma and surrounding whitespaces and
                          // make sure they are eliminated.
  };
  id response = OCMClassMock([NSHTTPURLResponse class]);
  OCMStub([response valueForHTTPHeaderField:[OCMArg any]])
      .andDo(GetMockMethodWithHeader(headers));
  TestVariationsSeedManager* manager =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  IOSChromeSeedResponse* seed = [manager seedResponseForHTTPResponse:response
                                                                data:nil];
  EXPECT_NE(seed, nil);
  EXPECT_EQ(seed.signature, signature);
  EXPECT_EQ(seed.country, country);
  EXPECT_EQ(seed.data, nil);
}

// Tests that `popSeed` correctly returns the seed and deletes it from the
// manager.
TEST_F(IOSChromeFirstRunVariationsSeedManagerTest, testPopSeed) {
  TestVariationsSeedManager* manager =
      [[TestVariationsSeedManager alloc] initWithCommandLineArgsForTesting:@[]];
  IOSChromeSeedResponse* expectedSeed =
      [[IOSChromeSeedResponse alloc] initWithSignature:@""
                                               country:@""
                                                  time:[NSDate now]
                                                  data:nil
                                            compressed:YES];
  [manager updateSharedSeed:expectedSeed];
  ASSERT_EQ(manager.seed, expectedSeed);
  IOSChromeSeedResponse* poppedSeed = [manager popSeed];
  EXPECT_EQ(manager.seed, nil);
  EXPECT_EQ(poppedSeed, expectedSeed);
}
