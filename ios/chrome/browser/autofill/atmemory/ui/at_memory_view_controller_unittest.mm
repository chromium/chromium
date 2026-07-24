// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class AtMemoryViewControllerTest : public PlatformTest {};

// Tests creating the view controller.
TEST_F(AtMemoryViewControllerTest, Init) {
  AtMemoryViewController* viewController =
      [[AtMemoryViewController alloc] init];
  EXPECT_NE(viewController, nil);
}
