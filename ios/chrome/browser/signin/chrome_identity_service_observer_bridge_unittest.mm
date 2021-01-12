// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"

#include <memory>

#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestChromeIdentityServiceObserver
    : NSObject<ChromeIdentityServiceObserver>
@property(nonatomic) BOOL onIdentityListChangedCalled;
@property(nonatomic) BOOL onAccessTokenRefreshFailedCalled;
@property(nonatomic) BOOL onProfileUpdateCalled;
@property(nonatomic) BOOL onChromeIdentityServiceWillBeDestroyedCalled;
@property(nonatomic, weak) ChromeIdentity* identity;
@property(weak, nonatomic, readonly) NSDictionary* userInfo;
@property(nonatomic, readonly)
    ios::ChromeIdentityService::Observer* observerBridge;
@end

@implementation TestChromeIdentityServiceObserver {
  std::unique_ptr<ios::ChromeIdentityService::Observer> _observer_bridge;
}

@synthesize onIdentityListChangedCalled = _onIdentityListChangedCalled;
@synthesize onAccessTokenRefreshFailedCalled =
    _onAccessTokenRefreshFailedCalled;
@synthesize onProfileUpdateCalled = _onProfileUpdateCalled;
@synthesize onChromeIdentityServiceWillBeDestroyedCalled =
    _onChromeIdentityServiceWillBeDestroyedCalled;
@synthesize identity = _identity;
@synthesize userInfo = _userInfo;

- (instancetype)init {
  if (self == [super init]) {
    _observer_bridge.reset(new ChromeIdentityServiceObserverBridge(self));
  }
  return self;
}

- (ios::ChromeIdentityService::Observer*)observerBridge {
  return _observer_bridge.get();
}

#pragma mark - ios::ChromeIdentityService::Observer

- (void)identityListChanged {
  _onIdentityListChangedCalled = YES;
}

- (void)accessTokenRefreshFailed:(ChromeIdentity*)identity
                        userInfo:(NSDictionary*)userInfo {
  _onAccessTokenRefreshFailedCalled = YES;
  _userInfo = userInfo;
  _identity = identity;
}

- (void)profileUpdate:(ChromeIdentity*)identity {
  _onProfileUpdateCalled = YES;
  _identity = identity;
}

- (void)chromeIdentityServiceWillBeDestroyed {
  _onChromeIdentityServiceWillBeDestroyedCalled = YES;
}

@end

#pragma mark - ChromeIdentityServiceObserverBridgeTest

class ChromeIdentityServiceObserverBridgeTest : public PlatformTest {
 protected:
  ChromeIdentityServiceObserverBridgeTest()
      : test_observer_([[TestChromeIdentityServiceObserver alloc] init]) {}

  ios::ChromeIdentityService::Observer* GetObserverBridge() {
    return [test_observer_ observerBridge];
  }

  TestChromeIdentityServiceObserver* GetTestObserver() {
    return test_observer_;
  }

 private:
  TestChromeIdentityServiceObserver* test_observer_;
};

// Tests that |onIdentityListChanged| is forwarded.
TEST_F(ChromeIdentityServiceObserverBridgeTest, onIdentityListChanged) {
  ASSERT_FALSE(GetTestObserver().onIdentityListChangedCalled);
  GetObserverBridge()->OnIdentityListChanged(false);
  EXPECT_TRUE(GetTestObserver().onIdentityListChangedCalled);
  EXPECT_FALSE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  EXPECT_FALSE(GetTestObserver().onProfileUpdateCalled);
  EXPECT_FALSE(GetTestObserver().onChromeIdentityServiceWillBeDestroyedCalled);
}

// Tests that |onAccessTokenRefreshFailed| is forwarded.
TEST_F(ChromeIdentityServiceObserverBridgeTest, onAccessTokenRefreshFailed) {
  ChromeIdentity* identity = [[ChromeIdentity alloc] init];
  NSDictionary* userInfo = [NSDictionary dictionary];
  ASSERT_FALSE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  GetObserverBridge()->OnAccessTokenRefreshFailed(identity, userInfo);
  EXPECT_FALSE(GetTestObserver().onIdentityListChangedCalled);
  EXPECT_TRUE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  EXPECT_FALSE(GetTestObserver().onProfileUpdateCalled);
  EXPECT_FALSE(GetTestObserver().onChromeIdentityServiceWillBeDestroyedCalled);
  EXPECT_EQ(identity, GetTestObserver().identity);
  EXPECT_NSEQ(userInfo, GetTestObserver().userInfo);
}

// Tests that |onProfileUpdate| is forwarded.
TEST_F(ChromeIdentityServiceObserverBridgeTest, onProfileUpdate) {
  ChromeIdentity* identity = [[ChromeIdentity alloc] init];
  ASSERT_FALSE(GetTestObserver().onProfileUpdateCalled);
  GetObserverBridge()->OnProfileUpdate(identity);
  EXPECT_FALSE(GetTestObserver().onIdentityListChangedCalled);
  EXPECT_FALSE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  EXPECT_TRUE(GetTestObserver().onProfileUpdateCalled);
  EXPECT_FALSE(GetTestObserver().onChromeIdentityServiceWillBeDestroyedCalled);
  EXPECT_EQ(identity, GetTestObserver().identity);
}

// Tests that |onChromeIdentityServiceWillBeDestroyed| is forwarded.
TEST_F(ChromeIdentityServiceObserverBridgeTest,
       onChromeIdentityServiceWillBeDestroyed) {
  ASSERT_FALSE(GetTestObserver().onChromeIdentityServiceWillBeDestroyedCalled);
  GetObserverBridge()->OnChromeIdentityServiceWillBeDestroyed();
  EXPECT_FALSE(GetTestObserver().onIdentityListChangedCalled);
  EXPECT_FALSE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  EXPECT_FALSE(GetTestObserver().onProfileUpdateCalled);
  EXPECT_TRUE(GetTestObserver().onChromeIdentityServiceWillBeDestroyedCalled);
}
