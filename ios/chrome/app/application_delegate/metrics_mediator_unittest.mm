// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/app/application_delegate/metrics_mediator_testing.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_block.h"
#include "components/metrics/metrics_service.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/metrics/previous_session_info_private.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface_provider.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "net/base/network_change_notifier.h"
#include "testing/platform_test.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - connectionTypeChanged tests.

// Mock class for testing MetricsMediator.
@interface MetricsMediatorMock : MetricsMediator
@property(nonatomic) NSInteger reportingValue;
@property(nonatomic) NSInteger breakpadUpload;
- (void)reset;
- (void)setReporting:(BOOL)enableReporting;
@end

@implementation MetricsMediatorMock
@synthesize reportingValue = _reportingValue;
@synthesize breakpadUpload = _breakpadUpload;

- (void)reset {
  _reportingValue = -1;
  _breakpadUpload = -1;
}
- (void)setBreakpadUploadingEnabled:(BOOL)enableUploading {
  _breakpadUpload = enableUploading ? 1 : 0;
}
- (void)setReporting:(BOOL)enableReporting {
  _reportingValue = enableReporting ? 1 : 0;
}
- (BOOL)isMetricsReportingEnabledWifiOnly {
  return YES;
}
- (BOOL)areMetricsEnabled {
  return YES;
}

@end

// Gives the differents net::NetworkChangeNotifier::ConnectionType based on
// scenario number.
net::NetworkChangeNotifier::ConnectionType getConnectionType(int number) {
  switch (number) {
    case 0:
      return net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
    case 1:
      return net::NetworkChangeNotifier::CONNECTION_ETHERNET;
    case 2:
      return net::NetworkChangeNotifier::CONNECTION_WIFI;
    case 3:
      return net::NetworkChangeNotifier::CONNECTION_2G;
    case 4:
      return net::NetworkChangeNotifier::CONNECTION_3G;
    case 5:
      return net::NetworkChangeNotifier::CONNECTION_4G;
    case 6:
      return net::NetworkChangeNotifier::CONNECTION_NONE;
    case 7:
      return net::NetworkChangeNotifier::CONNECTION_BLUETOOTH;
    default:
      return net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }
}

// Gives the differents expected value based on scenario number.
int getExpectedValue(int number) {
  if (number > 2 && number < 6)
    return 0;
  return 1;
}

using MetricsMediatorTest = PlatformTest;

// Verifies that connectionTypeChanged correctly enables or disables the
// uploading in the breakpad and in the metrics service.
TEST_F(MetricsMediatorTest, connectionTypeChanged) {
  [[PreviousSessionInfo sharedInstance] setIsFirstSessionAfterUpgrade:NO];
  MetricsMediatorMock* mock_metrics_helper = [[MetricsMediatorMock alloc] init];

  // Checks all different scenarios.
  for (int i = 0; i < 8; ++i) {
    [mock_metrics_helper reset];
    [mock_metrics_helper connectionTypeChanged:getConnectionType(i)];
    EXPECT_EQ(getExpectedValue(i), [mock_metrics_helper reportingValue]);
    EXPECT_EQ(getExpectedValue(i), [mock_metrics_helper breakpadUpload]);
  }

  // Checks that no new ConnectionType has been added.
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_BLUETOOTH,
            net::NetworkChangeNotifier::CONNECTION_LAST);
}

#pragma mark - logLaunchMetrics tests.

// A block that takes as arguments the caller and the arguments from
// UserActivityHandler +handleStartupParameters and returns nothing.
typedef void (^logLaunchMetricsBlock)(id, const char*, int);

class MetricsMediatorLogLaunchTest : public PlatformTest {
 protected:
  MetricsMediatorLogLaunchTest()
      : has_been_called_(FALSE),
        web_state_list_(
            std::make_unique<WebStateList>(&web_state_list_delegate_)) {}

  void initiateMetricsMediator(BOOL coldStart, int tabCount) {
    id mainTabModel = [OCMockObject mockForClass:[TabModel class]];
    [[[mainTabModel stub] andReturnValue:@(tabCount)] count];
    WebStateList* web_state_list = web_state_list_.get();
    [[[mainTabModel stub] andReturnValue:OCMOCK_VALUE(web_state_list)]
        webStateList];
    StubBrowserInterfaceProvider* concreteProvider =
        [[StubBrowserInterfaceProvider alloc] init];
    concreteProvider.mainInterface.tabModel = mainTabModel;
    interface_provider_ = concreteProvider;

    swizzle_block_ = [^(id self, int numTab) {
      has_been_called_ = YES;
      // Tests.
      EXPECT_EQ(tabCount, numTab);
    } copy];
    if (coldStart) {
      uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordNumTabAtStartup:),
          swizzle_block_));
    } else {
      uma_histogram_swizzler_.reset(new ScopedBlockSwizzler(
          [MetricsMediator class], @selector(recordNumTabAtResume:),
          swizzle_block_));
    }
  }

  void verifySwizzleHasBeenCalled() { EXPECT_TRUE(has_been_called_); }

  id<BrowserInterfaceProvider> getInterfaceProvider() {
    return interface_provider_;
  }

 private:
  id<BrowserInterfaceProvider> interface_provider_;
  __block BOOL has_been_called_;
  logLaunchMetricsBlock swizzle_block_;
  std::unique_ptr<ScopedBlockSwizzler> uma_histogram_swizzler_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
};

// Verifies that the log of the number of open tabs is sent and verifies
TEST_F(MetricsMediatorLogLaunchTest,
       logLaunchMetricsWithEnteredBackgroundDate) {
  // Setup.
  BOOL coldStart = YES;
  initiateMetricsMediator(coldStart, 23);

  const NSTimeInterval kFirstUserActionTimeout = 30.0;

  id startupInformation =
      [OCMockObject niceMockForProtocol:@protocol(StartupInformation)];
  [[[startupInformation stub] andReturnValue:@(coldStart)] isColdStart];
  [[startupInformation expect]
      expireFirstUserActionRecorderAfterDelay:kFirstUserActionTimeout];

  [[NSUserDefaults standardUserDefaults]
      setObject:[NSDate date]
         forKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  // Action.
  [MetricsMediator
      logLaunchMetricsWithStartupInformation:startupInformation
                           interfaceProvider:getInterfaceProvider()];

  // Tests.
  NSDate* dateStored = [[NSUserDefaults standardUserDefaults]
      objectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];
  EXPECT_EQ(nil, dateStored);
  verifySwizzleHasBeenCalled();
  EXPECT_OCMOCK_VERIFY(startupInformation);
}

// Verifies that +logLaunchMetrics logs of the number of open tabs and nothing
// more if the background date is not set;
TEST_F(MetricsMediatorLogLaunchTest, logLaunchMetricsNoBackgroundDate) {
  // Setup.
  BOOL coldStart = NO;
  initiateMetricsMediator(coldStart, 32);

  id startupInformation =
      [OCMockObject mockForProtocol:@protocol(StartupInformation)];
  [[[startupInformation stub] andReturnValue:@(coldStart)] isColdStart];

  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  // Action.
  [MetricsMediator
      logLaunchMetricsWithStartupInformation:startupInformation
                           interfaceProvider:getInterfaceProvider()];

  // Tests.
  verifySwizzleHasBeenCalled();
}

using MetricsMediatorNoFixtureTest = PlatformTest;

// Tests that +logDateInUserDefaults logs the date in UserDefaults.
TEST_F(MetricsMediatorNoFixtureTest, logDateInUserDefaultsTest) {
  // Setup.
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  NSDate* lastAppClose = [[NSUserDefaults standardUserDefaults]
      objectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];

  ASSERT_EQ(nil, lastAppClose);

  // Action.
  [MetricsMediator logDateInUserDefaults];

  // Setup.
  lastAppClose = [[NSUserDefaults standardUserDefaults]
      objectForKey:metrics_mediator::kAppEnteredBackgroundDateKey];
  EXPECT_NE(nil, lastAppClose);
}

