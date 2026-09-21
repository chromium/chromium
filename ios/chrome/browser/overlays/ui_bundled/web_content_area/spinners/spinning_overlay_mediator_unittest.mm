// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_mediator.h"

#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/spinning_overlay_request_config.h"
#import "testing/platform_test.h"

namespace {

NSString* const kLabelText = @"Downloading...";

}  // namespace

using SpinningOverlayMediatorTest = PlatformTest;

// Tests that tapping a non-cancellable overlay does not set a completion
// response.
TEST_F(SpinningOverlayMediatorTest, TapNonCancellable) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<SpinningOverlayRequestConfig>(
          kLabelText, /*is_cancellable=*/false);

  SpinningOverlayMediator* mediator =
      [[SpinningOverlayMediator alloc] initWithRequest:request.get()];

  [mediator spinningOverlayViewDidTap:nil];

  EXPECT_FALSE(request->GetCallbackManager()->GetCompletionResponse());
}

// Tests that tapping a cancellable overlay sets a cancellation response.
TEST_F(SpinningOverlayMediatorTest, TapCancellable) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<SpinningOverlayRequestConfig>(
          kLabelText, /*is_cancellable=*/true);

  SpinningOverlayMediator* mediator =
      [[SpinningOverlayMediator alloc] initWithRequest:request.get()];

  [mediator spinningOverlayViewDidTap:nil];

  OverlayResponse* response =
      request->GetCallbackManager()->GetCompletionResponse();
  ASSERT_TRUE(response);
  SpinningOverlayResponse* info = response->GetInfo<SpinningOverlayResponse>();
  ASSERT_TRUE(info);
  EXPECT_TRUE(info->canceled());
}
