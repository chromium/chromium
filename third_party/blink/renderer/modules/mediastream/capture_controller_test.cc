// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"

namespace blink {

class CaptureControllerTest : public testing::Test {
 public:
  ~CaptureControllerTest() override = default;
};

TEST_F(CaptureControllerTest, ReasonableMinimumAndMaximum) {
  V8TestingScope v8_scope;
  CaptureController* controller =
      MakeGarbageCollected<CaptureController>(v8_scope.GetExecutionContext());
  EXPECT_LT(controller->getMinZoomLevel(), controller->getMaxZoomLevel());
}

}  // namespace blink
