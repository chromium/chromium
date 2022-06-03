// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_response_support.h"

#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/platform_test.h"

namespace {
// Fake response info types for use in tests.
DEFINE_TEST_OVERLAY_RESPONSE_INFO(FirstInfo);
DEFINE_TEST_OVERLAY_RESPONSE_INFO(SecondInfo);
DEFINE_TEST_OVERLAY_RESPONSE_INFO(ThirdInfo);
}  // namespace

using OverlayResponseSupportTest = PlatformTest;

// Tests that OverlayResponseSupport::All() supports arbitrary responses.
TEST_F(OverlayResponseSupportTest, SupportAll) {
  std::unique_ptr<OverlayResponse> first_response =
      OverlayResponse::CreateWithInfo<FirstInfo>();
  std::unique_ptr<OverlayResponse> second_response =
      OverlayResponse::CreateWithInfo<SecondInfo>();

  const OverlayResponseSupport* support = OverlayResponseSupport::All();
  EXPECT_TRUE(support->IsResponseSupported(first_response.get()));
  EXPECT_TRUE(support->IsResponseSupported(second_response.get()));
}

// Tests that OverlayResponseSupport::None() does not support arbitrary
// responses.
TEST_F(OverlayResponseSupportTest, SupportNone) {
  std::unique_ptr<OverlayResponse> first_response =
      OverlayResponse::CreateWithInfo<FirstInfo>();
  std::unique_ptr<OverlayResponse> second_response =
      OverlayResponse::CreateWithInfo<SecondInfo>();

  const OverlayResponseSupport* support = OverlayResponseSupport::None();
  EXPECT_FALSE(support->IsResponseSupported(first_response.get()));
  EXPECT_FALSE(support->IsResponseSupported(second_response.get()));
}

// Tests that the SupportsOverlayResponse template returns true only when
// IsResponseSupported() is called with a response with the config type used to
// create the template specialization.
TEST_F(OverlayResponseSupportTest, SupportsResponseTemplate) {
  std::unique_ptr<OverlayResponseSupport> support =
      std::make_unique<SupportsOverlayResponse<FirstInfo>>();

  // Verify that FirstInfo responses are supported.
  std::unique_ptr<OverlayResponse> supported_response =
      OverlayResponse::CreateWithInfo<FirstInfo>();
  EXPECT_TRUE(support->IsResponseSupported(supported_response.get()));

  // Verify that SecondInfo responses are not supported.
  std::unique_ptr<OverlayResponse> unsupported_response =
      OverlayResponse::CreateWithInfo<SecondInfo>();
  EXPECT_FALSE(support->IsResponseSupported(unsupported_response.get()));
}

// Tests that the vector constructor aggregates support.
TEST_F(OverlayResponseSupportTest, AggregateSupport) {
  // Create an aggregate support for FirstInfo and SecondInfo, then verify that
  // OverlayResponses created with these infos are supported.
  OverlayResponseSupport support(
      {FirstInfo::ResponseSupport(), SecondInfo::ResponseSupport()});
  std::unique_ptr<OverlayResponse> first_response =
      OverlayResponse::CreateWithInfo<FirstInfo>();
  EXPECT_TRUE(support.IsResponseSupported(first_response.get()));
  std::unique_ptr<OverlayResponse> second_response =
      OverlayResponse::CreateWithInfo<SecondInfo>();
  EXPECT_TRUE(support.IsResponseSupported(second_response.get()));

  // Verify that OveralyResponses created with an unsupported info type are not
  // supported.
  std::unique_ptr<OverlayResponse> unsupported_response =
      OverlayResponse::CreateWithInfo<ThirdInfo>();
  EXPECT_FALSE(support.IsResponseSupported(unsupported_response.get()));
}
