// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/public/overlay_dispatch_callback.h"

#include "base/bind.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#include "testing/platform_test.h"

namespace {
// Response info types used in tests.
DEFINE_TEST_OVERLAY_RESPONSE_INFO(FirstInfo);
DEFINE_TEST_OVERLAY_RESPONSE_INFO(SecondInfo);
}  // namespace

// Test fixture for OverlayDispatchCallback.
class OverlayDispatchCallbackTest : public PlatformTest {
 public:
  // Test function to be used as a dispatch callback.  Counts number of times
  // function was called and exposes that count via execution_count().
  void TestDispatchCallback(OverlayResponse* response) { ++execution_count_; }

  // Returns the number of times TestCompletionCallback() has been executed.
  size_t execution_count() const { return execution_count_; }

 private:
  size_t execution_count_ = 0;
};

// Tests that the OverlayDispatchCallbacks constructed with a specified
// OverlaySupport is executed when run with a supported response type.
TEST_F(OverlayDispatchCallbackTest, SupportedResponse) {
  OverlayDispatchCallback callback(
      base::BindRepeating(&OverlayDispatchCallbackTest::TestDispatchCallback,
                          base::Unretained(this)),
      FirstInfo::ResponseSupport());
  std::unique_ptr<OverlayResponse> supported_response =
      OverlayResponse::CreateWithInfo<FirstInfo>();
  callback.Run(supported_response.get());
  callback.Run(supported_response.get());
  EXPECT_EQ(2U, execution_count());
}

// Tests that the OverlayDispatchCallbacks constructed with a specified
// OverlaySupport no-ops when run with an unsupported response type.
TEST_F(OverlayDispatchCallbackTest, UnsupportedResponse) {
  OverlayDispatchCallback callback(
      base::BindRepeating(&OverlayDispatchCallbackTest::TestDispatchCallback,
                          base::Unretained(this)),
      FirstInfo::ResponseSupport());
  std::unique_ptr<OverlayResponse> unsupported_response =
      OverlayResponse::CreateWithInfo<SecondInfo>();
  callback.Run(unsupported_response.get());
  callback.Run(unsupported_response.get());
  EXPECT_EQ(0U, execution_count());
}
