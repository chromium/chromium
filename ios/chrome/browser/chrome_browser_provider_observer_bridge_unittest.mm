// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/chrome_browser_provider_observer_bridge.h"

#import <memory>

#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestChromeBrowserProviderObserver
    : NSObject<ChromeBrowserProviderObserver>
@property(nonatomic) BOOL chromeIdentityServiceDidChangeCalled;
@property(nonatomic) BOOL chromeBrowserProviderWillBeDestroyedCalled;
@property(nonatomic) ios::ChromeIdentityService* identityService;
@property(nonatomic, readonly)
    ios::ChromeBrowserProvider::Observer* observerBridge;
@end

@implementation TestChromeBrowserProviderObserver {
  std::unique_ptr<ios::ChromeBrowserProvider::Observer> _observerBridge;
}

@synthesize chromeIdentityServiceDidChangeCalled =
    _chromeIdentityServiceDidChangeCalled;
@synthesize chromeBrowserProviderWillBeDestroyedCalled =
    _chromeBrowserProviderWillBeDestroyedCalled;
@synthesize identityService = _identityService;

- (instancetype)init {
  if (self == [super init]) {
    _observerBridge =
        std::make_unique<ChromeBrowserProviderObserverBridge>(self);
  }
  return self;
}

- (ios::ChromeBrowserProvider::Observer*)observerBridge {
  return _observerBridge.get();
}

#pragma mark - ios::ChromeBrowserProvider::Observer

- (void)chromeIdentityServiceDidChange:
    (ios::ChromeIdentityService*)identityService {
  _chromeIdentityServiceDidChangeCalled = YES;
  _identityService = identityService;
}

- (void)chromeBrowserProviderWillBeDestroyed {
  _chromeBrowserProviderWillBeDestroyedCalled = YES;
}

@end

#pragma mark - ChromeBrowserProviderObserverBridgeTest

class ChromeBrowserProviderObserverBridgeTest : public PlatformTest {
 public:
  ChromeBrowserProviderObserverBridgeTest(
      const ChromeBrowserProviderObserverBridgeTest&) = delete;
  ChromeBrowserProviderObserverBridgeTest& operator=(
      const ChromeBrowserProviderObserverBridgeTest&) = delete;

 protected:
  ChromeBrowserProviderObserverBridgeTest()
      : test_observer_([[TestChromeBrowserProviderObserver alloc] init]) {}

  ios::ChromeBrowserProvider::Observer* GetObserverBridge() {
    return [test_observer_ observerBridge];
  }

  TestChromeBrowserProviderObserver* GetTestObserver() {
    return test_observer_;
  }

 private:
  TestChromeBrowserProviderObserver* test_observer_;
};

// Tests that `OnChromeIdentityServiceDidChange` is forwarded.
TEST_F(ChromeBrowserProviderObserverBridgeTest,
       ChromeIdentityServiceDidChange) {
  std::unique_ptr<ios::ChromeIdentityService> identity_service;
  ASSERT_FALSE(GetTestObserver().chromeIdentityServiceDidChangeCalled);
  GetObserverBridge()->OnChromeIdentityServiceDidChange(identity_service.get());
  EXPECT_TRUE(GetTestObserver().chromeIdentityServiceDidChangeCalled);
  EXPECT_FALSE(GetTestObserver().chromeBrowserProviderWillBeDestroyedCalled);
  EXPECT_EQ(identity_service.get(), GetTestObserver().identityService);
}

// Tests that `OnChromeBrowserProviderWillBeDestroyed` is forwarded.
TEST_F(ChromeBrowserProviderObserverBridgeTest,
       ChromeBrowserProviderWillBeDestroyed) {
  ASSERT_FALSE(GetTestObserver().chromeBrowserProviderWillBeDestroyedCalled);
  GetObserverBridge()->OnChromeBrowserProviderWillBeDestroyed();
  EXPECT_FALSE(GetTestObserver().chromeIdentityServiceDidChangeCalled);
  EXPECT_TRUE(GetTestObserver().chromeBrowserProviderWillBeDestroyedCalled);
}
