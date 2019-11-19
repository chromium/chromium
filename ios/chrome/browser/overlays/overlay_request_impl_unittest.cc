// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/overlay_request_impl.h"

#include "base/bind.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#include "testing/platform_test.h"

using OverlayRequestImplTest = PlatformTest;

// Tests that OverlayRequestImpls execute their callbacks upon destruction.
TEST_F(OverlayRequestImplTest, ExecuteCallback) {
  void* kResponseData = &kResponseData;
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequestImpl* request_impl =
      static_cast<OverlayRequestImpl*>(request.get());
  __block bool callback_executed = false;
  OverlayCallback callback =
      base::BindOnce(base::RetainBlock(^(OverlayResponse* response) {
        callback_executed =
            response &&
            response->GetInfo<FakeOverlayUserData>()->value() == kResponseData;
      }));
  request_impl->set_callback(std::move(callback));
  request->set_response(
      OverlayResponse::CreateWithInfo<FakeOverlayUserData>(kResponseData));
  request = nullptr;
  EXPECT_TRUE(callback_executed);
}
