// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/public/overlay_response.h"

#include "ios/chrome/browser/overlays/model/test/fake_overlay_user_data.h"
#include "testing/platform_test.h"

using OverlayResponseTest = PlatformTest;

// Tests that OverlayResponses can be constructed.
TEST_F(OverlayResponseTest, CreateWithInfo) {
  int value = 0;
  std::unique_ptr<OverlayResponse> request =
      OverlayResponse::CreateWithInfo<FakeOverlayUserData>(&value);
  FakeOverlayUserData* info = request->GetInfo<FakeOverlayUserData>();
  ASSERT_TRUE(info);
  EXPECT_EQ(info->value(), &value);
}
