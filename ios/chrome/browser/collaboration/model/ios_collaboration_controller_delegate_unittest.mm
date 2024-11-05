// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/ios_collaboration_controller_delegate.h"

#import "base/check.h"
#import "testing/platform_test.h"

namespace collaboration {

// Test fixture for the iOS collaboration controller delegate.
class IOSCollaborationControllerDelegateTest : public PlatformTest {
 protected:
  IOSCollaborationControllerDelegateTest() {
    delegate_ = std::make_unique<IOSCollaborationControllerDelegate>();
  }

  std::unique_ptr<IOSCollaborationControllerDelegate> delegate_;
};

// Tests that the delegate exists.
TEST_F(IOSCollaborationControllerDelegateTest, DelegateExists) {
  CHECK(delegate_);
}

}  // namespace collaboration
