// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_handler.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class GeminiTabPickerHandlerTest : public PlatformTest {
 protected:
  GeminiTabPickerHandlerTest() {
    handler_ = [[GeminiTabPickerHandler alloc] init];
  }

  GeminiTabPickerHandler* handler_;
};

// Tests that the handler conforms to the GeminiTabPickerDelegate protocol.
TEST_F(GeminiTabPickerHandlerTest, TestConformsToProtocol) {
  EXPECT_TRUE([handler_ conformsToProtocol:@protocol(GeminiTabPickerDelegate)]);
}
