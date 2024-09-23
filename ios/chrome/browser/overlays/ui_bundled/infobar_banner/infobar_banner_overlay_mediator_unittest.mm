// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_dispatch_callback.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
// Request ConfigType used in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FakeConfig);
}

// InfobarBannerOverlayMediator subclass used for testing.
@interface FakeInfobarBannerOverlayMediator : InfobarBannerOverlayMediator
@end

@implementation FakeInfobarBannerOverlayMediator
+ (const OverlayRequestSupport*)requestSupport {
  return FakeConfig::RequestSupport();
}
@end

// Test fixture for InfobarBannerOverlayMediator.
class InfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  InfobarBannerOverlayMediatorTest()
      : callback_installer_(
            &callback_receiver_,
            {InfobarBannerMainActionResponse::ResponseSupport(),
             InfobarBannerShowModalResponse::ResponseSupport(),
             InfobarBannerUserInitiatedDismissalResponse::ResponseSupport()}),
        request_(OverlayRequest::CreateWithConfig<FakeConfig>()),
        delegate_(
            OCMStrictProtocolMock(@protocol(OverlayRequestMediatorDelegate))),
        mediator_([[FakeInfobarBannerOverlayMediator alloc]
            initWithRequest:request_.get()]) {
    mediator_.delegate = delegate_;
    callback_installer_.InstallCallbacks(request_.get());
  }
  ~InfobarBannerOverlayMediatorTest() override {
    // `callback_receiver_`'s completion callback is guaranteed to be called
    // when the test fixture is torn down.  This functionality is already tested
    // in OverlayRequestCallbackInstaller's unittests.  This EXPECT_CALL() for
    // the completion callback is added here instead of in individual tests
    // since the execution of the completion callback is not functionality
    // specific to the InfobarBannerOverlayMediator.
    EXPECT_CALL(callback_receiver_, CompletionCallback(request_.get()));
    EXPECT_OCMOCK_VERIFY(delegate_);
  }

 protected:
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
  std::unique_ptr<OverlayRequest> request_;
  id<OverlayRequestMediatorDelegate> delegate_ = nil;
  FakeInfobarBannerOverlayMediator* mediator_ = nil;
};

// Tests that an InfobarBannerOverlayMediator correctly dispatches a response
// for confirm button taps before stopping itself.
TEST_F(InfobarBannerOverlayMediatorTest, ConfirmButtonTapped) {
  // Notify the mediator of the button tap via its InfobarBannerDelegate
  // implementation and verify that the confirm button callback was executed and
  // that the mediator's delegate was instructed to stop.
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(),
                       InfobarBannerMainActionResponse::ResponseSupport()));
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Tests that an InfobarBannerOverlayMediator correctly dispatches a response
// for modal button taps before stopping itself.
TEST_F(InfobarBannerOverlayMediatorTest, ModalButtonTapped) {
  // Notify the mediator of the button tap via its InfobarBannerDelegate
  // implementation and verify that the modal button callback was executed.
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request_.get(),
                       InfobarBannerShowModalResponse::ResponseSupport()));
  [mediator_ presentInfobarModalFromBanner];
}

// Tests that an InfobarBannerOverlayMediator correctly sets the completion
// response for user-initiated dismissals triggered by the banner UI.
TEST_F(InfobarBannerOverlayMediatorTest, UserInitiatedDismissal) {
  // Notify the mediator of the dismissal via its InfobarBannerDelegate
  // implementation and verify that the completion callback was executed with
  // the correct info and that the mediator's delegate was instructed to stop.
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(
          request_.get(),
          InfobarBannerUserInitiatedDismissalResponse::ResponseSupport()));
  OCMExpect([delegate_ stopOverlayForMediator:mediator_]);
  [mediator_ dismissInfobarBannerForUserInteraction:YES];
}
