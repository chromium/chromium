// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_request_support.h"

#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/platform_test.h"

namespace {
// Fake request config types for use in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FirstConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(SecondConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(ThirdConfig);
}  // namespace

using OverlayRequestSupportTest = PlatformTest;

// Tests that OverlayRequestSupport::All() supports arbitrary requests.
TEST_F(OverlayRequestSupportTest, SupportAll) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FirstConfig>();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<SecondConfig>();

  const OverlayRequestSupport* support = OverlayRequestSupport::All();
  EXPECT_TRUE(support->IsRequestSupported(first_request.get()));
  EXPECT_TRUE(support->IsRequestSupported(second_request.get()));
}

// Tests that OverlayRequestSupport::None() does not support config types.
TEST_F(OverlayRequestSupportTest, SupportNone) {
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FirstConfig>();
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<SecondConfig>();

  const OverlayRequestSupport* support = OverlayRequestSupport::None();
  EXPECT_FALSE(support->IsRequestSupported(first_request.get()));
  EXPECT_FALSE(support->IsRequestSupported(second_request.get()));
}

// Tests that the SupportsOverlayRequest template returns true only when
// IsRequestSupported() is called with a request with the config type used to
// create the template specialization.
TEST_F(OverlayRequestSupportTest, SupportsRequestTemplate) {
  std::unique_ptr<OverlayRequestSupport> support =
      std::make_unique<SupportsOverlayRequest<FirstConfig>>();

  // Verify that FirstConfig requests are supported.
  std::unique_ptr<OverlayRequest> supported_request =
      OverlayRequest::CreateWithConfig<FirstConfig>();
  EXPECT_TRUE(support->IsRequestSupported(supported_request.get()));

  // Verify that SecondConfig requests are not supported.
  std::unique_ptr<OverlayRequest> unsupported_request =
      OverlayRequest::CreateWithConfig<SecondConfig>();
  EXPECT_FALSE(support->IsRequestSupported(unsupported_request.get()));
}

// Tests that the vector constructor aggregates support.
TEST_F(OverlayRequestSupportTest, AggregateSupport) {
  // Create an aggregate request support for FirstConfig and SecondConfig, then
  // verify that OverlayResponses created with these two configs are supported.
  OverlayRequestSupport support(
      {FirstConfig::RequestSupport(), SecondConfig::RequestSupport()});
  std::unique_ptr<OverlayRequest> first_request =
      OverlayRequest::CreateWithConfig<FirstConfig>();
  EXPECT_TRUE(support.IsRequestSupported(first_request.get()));
  std::unique_ptr<OverlayRequest> second_request =
      OverlayRequest::CreateWithConfig<SecondConfig>();
  EXPECT_TRUE(support.IsRequestSupported(second_request.get()));

  // Check that OverlayResponses created with a different config are not
  // supported.
  std::unique_ptr<OverlayRequest> unsupported_request =
      OverlayRequest::CreateWithConfig<ThirdConfig>();
  EXPECT_FALSE(support.IsRequestSupported(unsupported_request.get()));
}
