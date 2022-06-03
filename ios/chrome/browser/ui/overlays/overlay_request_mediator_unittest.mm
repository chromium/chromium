// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

#import "base/bind.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/public/overlay_response_support.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// ConfigType and InfoType used in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FakeConfig);
DEFINE_TEST_OVERLAY_RESPONSE_INFO(DispatchInfo);
}

// OverlayRequestMediator subclass used in tests.
@interface FakeOverlayRequestMediator : OverlayRequestMediator
@end

@implementation FakeOverlayRequestMediator
+ (const OverlayRequestSupport*)requestSupport {
  return FakeConfig::RequestSupport();
}
@end

// Test fixture for OverlayRequestMediator.
class OverlayRequestMediatorTest : public PlatformTest {
 public:
  OverlayRequestMediatorTest()
      : callback_installer_(&callback_receiver_,
                            {DispatchInfo::ResponseSupport()}),
        request_(OverlayRequest::CreateWithConfig<FakeConfig>()),
        delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))),
        mediator_([[FakeOverlayRequestMediator alloc]
            initWithRequest:request_.get()]) {
    mediator_.delegate = delegate_;
    callback_installer_.InstallCallbacks(request_.get());
  }
  ~OverlayRequestMediatorTest() override {
    ResetRequest();
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

  // Destroys |request_|, expecting that the completion callback is executed.
  void ResetRequest() {
    if (!request_)
      return;
    EXPECT_CALL(callback_receiver_, CompletionCallback(request_.get()));
    request_ = nullptr;
  }

 protected:
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
  std::unique_ptr<OverlayRequest> request_;
  id<OverlayRequestMediatorDelegate> delegate_ = nil;
  FakeOverlayRequestMediator* mediator_ = nil;
};

// Tests that the mediator's request is reset after the request's destruction.
TEST_F(OverlayRequestMediatorTest, ResetRequestAfterDestruction) {
  EXPECT_EQ(request_.get(), mediator_.request);
  ResetRequest();
  EXPECT_EQ(nullptr, mediator_.request);
}

// Tests that |-dispatchResponse:| correctly dispatches the response.
TEST_F(OverlayRequestMediatorTest, DispatchResponse) {
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(), DispatchInfo::ResponseSupport()));
  [mediator_ dispatchResponse:OverlayResponse::CreateWithInfo<DispatchInfo>()];
}

// Tests that |-dismissOverlay| stops the overlay.
TEST_F(OverlayRequestMediatorTest, DismissOverlay) {
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissOverlay];
}
