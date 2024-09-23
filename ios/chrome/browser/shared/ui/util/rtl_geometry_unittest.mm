// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"

#import <UIKit/UIKit.h>

#import "testing/platform_test.h"

class RtlGeometryTest : public PlatformTest {};

// Tests that clearing the pasteboard does remove pasteboard items.
TEST_F(RtlGeometryTest, ScrollToSemanticLeadingTest) {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.contentSize = CGSizeMake(4, 4);

  // Switch to RTL.
  scrollView.semanticContentAttribute =
      UISemanticContentAttributeForceRightToLeft;
  ScrollToSemanticLeading(scrollView, NO);
  EXPECT_EQ(scrollView.contentOffset.x, scrollView.contentSize.width - 1);

  // Switch to LTR.
  scrollView.semanticContentAttribute =
      UISemanticContentAttributeForceLeftToRight;
  ScrollToSemanticLeading(scrollView, NO);
  EXPECT_EQ(scrollView.contentOffset.x, 0);
}
