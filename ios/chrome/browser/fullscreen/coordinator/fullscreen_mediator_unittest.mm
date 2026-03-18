// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"

#import "testing/platform_test.h"

// Test fixture for testing FullscreenMediator class.
class FullscreenMediatorTest : public PlatformTest {
 protected:
  FullscreenMediatorTest() { mediator_ = [[FullscreenMediator alloc] init]; }

  FullscreenMediator* mediator_;
};

// Tests that the mediator can be disconnected.
TEST_F(FullscreenMediatorTest, Disconnect) {
  [mediator_ disconnect];
}
