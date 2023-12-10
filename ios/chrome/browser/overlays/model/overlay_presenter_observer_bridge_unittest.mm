// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "testing/platform_test.h"

// Fake implementation of OverlayPresenterObserving that records whether the
// callbacks are executed.
@interface FakeOverlayPresenterObserver : NSObject <OverlayPresenterObserving>
@property(nonatomic) const OverlayRequestSupport* support;
// Whether each of the OverlayPresenterObserving callbacks have been called.
@property(nonatomic, readonly) BOOL willShowCalled;
@property(nonatomic, readonly) BOOL didShowCalled;
@property(nonatomic, readonly) BOOL didHideCalled;
@property(nonatomic, readonly) BOOL destroyedCalled;
@end

@implementation FakeOverlayPresenterObserver

- (const OverlayRequestSupport*)overlayRequestSupportForPresenter:
    (OverlayPresenter*)presenter {
  return self.support;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
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

// Tests that OverlayPresenterObserver::GetRequestSupport() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, GetRequestSupport) {
  std::unique_ptr<OverlayRequestSupport> support =
      std::make_unique<SupportsOverlayRequest<FakeOverlayUserData>>();
  observer_.support = support.get();
  EXPECT_EQ(support.get(), bridge_.GetRequestSupport(/*request=*/nullptr));
}

// Tests that OverlayPresenterObserver::WillShowOverlay() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, WillShowCalled) {
  ASSERT_FALSE(observer_.willShowCalled);
  bridge_.WillShowOverlay(/*presenter=*/nullptr, /*request=*/nullptr,
                          /*initial_presentation=*/true);
  EXPECT_TRUE(observer_.willShowCalled);
}

// Tests that OverlayPresenterObserver::DidShowOverlay() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, DidShowCalled) {
  ASSERT_FALSE(observer_.didShowCalled);
  bridge_.DidShowOverlay(/*presenter=*/nullptr, /*request=*/nullptr);
  EXPECT_TRUE(observer_.didShowCalled);
}

// Tests that OverlayPresenterObserver::DidHideOverlay() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, DidHideCalled) {
  ASSERT_FALSE(observer_.didHideCalled);
  bridge_.DidHideOverlay(/*presenter=*/nullptr, /*request=*/nullptr);
  EXPECT_TRUE(observer_.didHideCalled);
}

// Tests that OverlayPresenterObserver::OverlayPresenterDestroyed() is correctly
// forwarded.
TEST_F(OverlayPresenterObserverBridgeTest, DestroyedCalled) {
  ASSERT_FALSE(observer_.destroyedCalled);
  bridge_.OverlayPresenterDestroyed(/*presenter=*/nullptr);
  EXPECT_TRUE(observer_.destroyedCalled);
}
