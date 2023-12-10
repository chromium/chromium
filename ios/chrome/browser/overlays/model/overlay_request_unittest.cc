// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/public/overlay_request.h"

#include "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#include "testing/platform_test.h"

using OverlayRequestTest = PlatformTest;

// Tests that OverlayRequests can be created.
TEST_F(OverlayRequestTest, CreateWithConfig) {
  int value = 0;
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(&value);
  FakeOverlayUserData* config = request->GetConfig<FakeOverlayUserData>();
  ASSERT_TRUE(config);
  EXPECT_EQ(config->value(), &value);
}
