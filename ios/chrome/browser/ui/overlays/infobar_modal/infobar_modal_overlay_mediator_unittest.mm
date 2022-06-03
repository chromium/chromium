// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/infobar_modal_overlay_mediator.h"

#import "base/bind.h"
#include "ios/chrome/browser/overlays/public/infobar_modal/infobar_modal_overlay_responses.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/public/overlay_response_support.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Request ConfigType used for tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(ModalConfig);
}

// Mediator used in tests.
@interface FakeInfobarModalOverlayMediator : InfobarModalOverlayMediator
@end

@implementation FakeInfobarModalOverlayMediator
+ (const OverlayRequestSupport*)requestSupport {
  return ModalConfig::RequestSupport();
}
@end

// Test fixture for InfobarModalOverlayMediator.
class InfobarModalOverlayMediatorTest : public PlatformTest {
 public:
  InfobarModalOverlayMediatorTest()
      : callback_installer_(
            &callback_receiver_,
            {InfobarModalMainActionResponse::ResponseSupport()}),
        request_(OverlayRequest::CreateWithConfig<ModalConfig>()),
        delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))),
        mediator_([[FakeInfobarModalOverlayMediator alloc]
            initWithRequest:request_.get()]) {
    mediator_.delegate = delegate_;
    callback_installer_.InstallCallbacks(request_.get());
  }
  ~InfobarModalOverlayMediatorTest() override {
    // |callback_receiver_|'s completion callback is guaranteed to be called
    // when the test fixture is torn down.  This functionality is already tested
    // in OverlayRequestCallbackInstaller's unittests.  This EXPECT_CALL() for
    // the completion callback is added here instead of in individual tests
    // since the execution of the completion callback is not functionality
    // specific to the InfobarModalOverlayMediator.
    EXPECT_CALL(callback_receiver_, CompletionCallback(request_.get()));
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

 protected:
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
  std::unique_ptr<OverlayRequest> request_;
  id<OverlayRequestMediatorDelegate> delegate_ = nil;
  FakeInfobarModalOverlayMediator* mediator_ = nil;
};

// Tests that |-dismissInfobarModal| triggers dismissal via the delegate.
TEST_F(InfobarModalOverlayMediatorTest, DismissInfobarModal) {
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarModal:nil];
}

// Tests that |-modalInfobarButtonWasAccepted| dispatches a main action response
// then dismisses the modal.
TEST_F(InfobarModalOverlayMediatorTest, ModalInfobarButtonWasAccepted) {
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(),
                       InfobarModalMainActionResponse::ResponseSupport()));
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ modalInfobarButtonWasAccepted:nil];
}
