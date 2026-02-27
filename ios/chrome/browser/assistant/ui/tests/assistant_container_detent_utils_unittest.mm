// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_detent_utils.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using AssistantContainerDetentUtilsTest = PlatformTest;

// Tests that the medium detent is correctly configured.
TEST_F(AssistantContainerDetentUtilsTest, MediumDetent) {
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 200)];

  // Attach to a window so safeAreaLayoutGuide is computed.
  UIWindow* window = [[UIWindow alloc] initWithFrame:view.bounds];
  [window addSubview:view];
  [window layoutIfNeeded];

  AssistantContainerDetent* detent = AssistantContainerMediumDetent(view);

  EXPECT_TRUE([detent.identifier
      isEqualToString:kAssistantContainerMediumDetentIdentifier]);
  EXPECT_EQ(detent.value, 100);
}

// Tests that the large detent is correctly configured.
TEST_F(AssistantContainerDetentUtilsTest, LargeDetent) {
  UIView* view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 200)];

  // Attach to a window so safeAreaLayoutGuide is computed.
  UIWindow* window = [[UIWindow alloc] initWithFrame:view.bounds];
  [window addSubview:view];
  [window layoutIfNeeded];

  AssistantContainerDetent* detent = AssistantContainerLargeDetent(view);

  EXPECT_TRUE([detent.identifier
      isEqualToString:kAssistantContainerLargeDetentIdentifier]);
  EXPECT_EQ(detent.value, 200);
}

// Tests that the fixed detent is created correctly.
TEST_F(AssistantContainerDetentUtilsTest, FixedDetent) {
  AssistantContainerDetent* detent = AssistantContainerFixedDetent(
      100, kAssistantContainerMinimizedDetentIdentifier);
  EXPECT_EQ(detent.value, 100);
  EXPECT_NSEQ(detent.identifier, kAssistantContainerMinimizedDetentIdentifier);
}

}  // namespace
