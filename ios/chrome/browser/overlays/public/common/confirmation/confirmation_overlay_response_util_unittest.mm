// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/common/confirmation/confirmation_overlay_response_util.h"

#include "base/bind.h"
#import "ios/chrome/browser/overlays/public/common/confirmation/confirmation_overlay_response.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertResponse;
using alert_overlays::ResponseConverter;

namespace {
// The OK button index used for tests.
size_t kOkButtonIndex = 0;
}

// Test fixture for ConfirmationOverlayResonse util functions.
using ConfirmationOverlayResponseUtilTest = PlatformTest;

// Tests that converting an AlertResponse where the OK button is tapped creates
// a ConfirmationOverlayResponse with confirmed() set to true.
TEST_F(ConfirmationOverlayResponseUtilTest, AlertConfirmConversion) {
  ResponseConverter converter =
      GetConfirmationResponseConverter(kOkButtonIndex);

  std::unique_ptr<OverlayResponse> confirmed_alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(kOkButtonIndex, nil);
  std::unique_ptr<OverlayResponse> confirmed_response =
      converter.Run(std::move(confirmed_alert_response));
  ASSERT_TRUE(confirmed_response);
  ConfirmationOverlayResponse* confirmed_info =
      confirmed_response->GetInfo<ConfirmationOverlayResponse>();
  ASSERT_TRUE(confirmed_info);
  EXPECT_TRUE(confirmed_info->confirmed());
}

// Tests that converting an AlertResponse where a button that is not the OK
// button is tapped creates a ConfirmationOverlayResponse with confirmed() set
// to false.
TEST_F(ConfirmationOverlayResponseUtilTest, AlertDenyConversion) {
  ResponseConverter converter =
      GetConfirmationResponseConverter(kOkButtonIndex);

  std::unique_ptr<OverlayResponse> denied_alert_response =
      OverlayResponse::CreateWithInfo<AlertResponse>(kOkButtonIndex + 1, nil);
  std::unique_ptr<OverlayResponse> denied_response =
      converter.Run(std::move(denied_alert_response));
  ASSERT_TRUE(denied_response);
  ConfirmationOverlayResponse* denied_info =
      denied_response->GetInfo<ConfirmationOverlayResponse>();
  ASSERT_TRUE(denied_info);
  EXPECT_FALSE(denied_info->confirmed());
}
