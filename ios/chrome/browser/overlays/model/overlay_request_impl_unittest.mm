// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/overlay_request_impl.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#import "testing/platform_test.h"

using OverlayRequestImplTest = PlatformTest;

// Tests that OverlayRequestImpls execute their callbacks upon destruction.
TEST_F(OverlayRequestImplTest, ExecuteCallback) {
  void* kResponseData = &kResponseData;
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>();
  __block bool callback_executed = false;
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(^(OverlayResponse* response) {
        callback_executed =
            response &&
            response->GetInfo<FakeOverlayUserData>()->value() == kResponseData;
      }));
  request->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<FakeOverlayUserData>(kResponseData));
  request = nullptr;
  EXPECT_TRUE(callback_executed);
}
