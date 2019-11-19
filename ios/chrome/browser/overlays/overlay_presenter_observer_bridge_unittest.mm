// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"

#import <Foundation/Foundation.h>

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Fake implementation of OverlayPresenterObserving that records whether the
// callbacks are executed.
@interface FakeOverlayPresenterObserver : NSObject <OverlayPresenterObserving>
// Whether each of the OverlayPresenterObserving callbacks have been called.
@property(nonatomic, readonly) BOOL willShowCalled;
@property(nonatomic, readonly) BOOL didShowCalled;
@property(nonatomic, readonly) BOOL didHideCalled;
@property(nonatomic, readonly) BOOL destroyedCalled;
@end

@implementation FakeOverlayPresenterObserver

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request {
  _willShowCalled = YES;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didShowOverlayForRequest:(OverlayRequest*)request {
  _didShowCalled = YES;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  _didHideCalled = YES;
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  _destroyedCalled = YES;
}

@end

// Test fixture for OverlayPresenterObserverBridge.
class OverlayPresenterObserverBridgeTest : public PlatformTest {
 public:
  OverlayPresenterObserverBridgeTest()
      : observer_([[FakeOverlayPresenterObserver alloc] init]),
        bridge_(observer_) {}

 protected:
  FakeOverlayPresenterObserver* observer_;
  OverlayPresenterObserverBridge bridge_;
};

// Tests that OverlayPresenterObserver::WillShowOverlay() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, WillShowCalled) {
  ASSERT_FALSE(observer_.willShowCalled);
  bridge_.WillShowOverlay(nullptr, nullptr);
  EXPECT_TRUE(observer_.willShowCalled);
}

// Tests that OverlayPresenterObserver::DidShowOverlay() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, DidShowCalled) {
  ASSERT_FALSE(observer_.didShowCalled);
  bridge_.DidShowOverlay(nullptr, nullptr);
  EXPECT_TRUE(observer_.didShowCalled);
}

// Tests that OverlayPresenterObserver::DidHideOverlay() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, DidHideCalled) {
  ASSERT_FALSE(observer_.didHideCalled);
  bridge_.DidHideOverlay(nullptr, nullptr);
  EXPECT_TRUE(observer_.didHideCalled);
}

// Tests that OverlayPresenterObserver::OverlayPresenterDestroyed() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, DestroyedCalled) {
  ASSERT_FALSE(observer_.destroyedCalled);
  bridge_.OverlayPresenterDestroyed(nullptr);
  EXPECT_TRUE(observer_.destroyedCalled);
}
