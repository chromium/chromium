// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_contents_view_mac.h"

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/test/cocoa_helper.h"
#import "ui/base/test/cocoa_helper.h"

namespace content {

namespace {

class WebContentsViewCocoaTest : public ui::CocoaTest {
};

}  // namespace

TEST_F(WebContentsViewCocoaTest, NonWebDragSourceTest) {
  // The designated initializer is private but init should be fine in this case.
  base::scoped_nsobject<WebContentsViewCocoa> view(
      [[WebContentsViewCocoa alloc] init]);

  // Tests that |draggingSourceOperationMaskForLocal:| returns the expected mask
  // when dragging without a WebDragSource - i.e. when |startDragWithDropData:|
  // hasn't yet been called. Dragging a file from the downloads manager, for
  // example, requires this to work.
  EXPECT_EQ(NSDragOperationCopy,
      [view draggingSourceOperationMaskForLocal:YES]);
  EXPECT_EQ(NSDragOperationCopy,
      [view draggingSourceOperationMaskForLocal:NO]);
}

TEST_F(WebContentsViewCocoaTest, AccessibilityParentTest) {
  // The designated initializer is private but init should be fine in this case.
  base::scoped_nsobject<WebContentsViewCocoa> view(
      [[WebContentsViewCocoa alloc] init]);

  // NSBox so it participates in the a11y hierarchy.
  base::scoped_nsobject<NSView> parent_view([[NSBox alloc] init]);
  base::scoped_nsobject<NSView> accessibility_parent([[NSView alloc] init]);

  [parent_view addSubview:view];
  EXPECT_NSEQ([view accessibilityAttributeValue:NSAccessibilityParentAttribute],
              parent_view);

  [view setAccessibilityParentElement:accessibility_parent];
  EXPECT_NSEQ([view accessibilityAttributeValue:NSAccessibilityParentAttribute],
              accessibility_parent);

  [view setAccessibilityParentElement:nil];
  EXPECT_NSEQ([view accessibilityAttributeValue:NSAccessibilityParentAttribute],
              parent_view);
}

namespace {

class WebContentsViewMacTest : public RenderViewHostTestHarness {
 protected:
  WebContentsViewMacTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    window_.reset([[CocoaTestHelperWindow alloc] init]);
    [[window_ contentView]
        addSubview:web_contents()->GetNativeView().GetNativeNSView()];
  }

  base::scoped_nsobject<CocoaTestHelperWindow> window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsViewMacTest);
};

}  // namespace

TEST_F(WebContentsViewMacTest, ShowHideParent) {
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
  [[window_ contentView] setHidden:YES];
  EXPECT_EQ(Visibility::HIDDEN, web_contents()->GetVisibility());
  [[window_ contentView] setHidden:NO];
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
}

TEST_F(WebContentsViewMacTest, OccludeView) {
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
  [window_ setPretendIsOccluded:YES];
  EXPECT_EQ(Visibility::OCCLUDED, web_contents()->GetVisibility());
  [window_ setPretendIsOccluded:NO];
  EXPECT_EQ(Visibility::VISIBLE, web_contents()->GetVisibility());
}

}  // namespace content
