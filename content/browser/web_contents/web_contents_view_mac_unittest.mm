// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_contents_view_mac.h"

#include "base/mac/scoped_nsobject.h"
#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/test/cocoa_helper.h"
#import "ui/base/test/cocoa_helper.h"

namespace content {

namespace {

class WebContentsNSViewTest : public ui::CocoaTest {};

}  // namespace

TEST_F(WebContentsNSViewTest, NonWebDragSourceTest) {
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

}  // namespace content
