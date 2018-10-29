// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_scroll_view_internal.h"

#import <UIKit/UIKit.h>

#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web_view/public/cwv_scroll_view_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVScrollViewTest : public PlatformTest {
 protected:
  CWVScrollViewTest() {
    scroll_view_ = [[CWVScrollView alloc] init];

    scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];
    scroll_view_.proxy = scroll_view_proxy_;

    ui_scroll_view_ = [[UIScrollView alloc] init];
    [scroll_view_proxy_ setScrollView:ui_scroll_view_];
  }

  CWVScrollView* scroll_view_;
  UIScrollView* ui_scroll_view_;
  CRWWebViewScrollViewProxy* scroll_view_proxy_;
};

// Tests CWVScrollView delegate callbacks.
TEST_F(CWVScrollViewTest, DelegateCallbacks) {
  id delegate = OCMProtocolMock(@protocol(CWVScrollViewDelegate));
  scroll_view_.delegate = delegate;

  [[delegate expect] scrollViewWillBeginDragging:scroll_view_];
  [scroll_view_proxy_ scrollViewWillBeginDragging:ui_scroll_view_];

  CGPoint targetContentOffset;
  [[delegate expect] scrollViewWillEndDragging:scroll_view_
                                  withVelocity:CGPointZero
                           targetContentOffset:&targetContentOffset];
  [scroll_view_proxy_ scrollViewWillEndDragging:ui_scroll_view_
                                   withVelocity:CGPointZero
                            targetContentOffset:&targetContentOffset];

  [[delegate expect] scrollViewDidScroll:scroll_view_];
  [scroll_view_proxy_ scrollViewDidScroll:ui_scroll_view_];

  [[delegate expect] scrollViewDidEndDecelerating:scroll_view_];
  [scroll_view_proxy_ scrollViewDidEndDecelerating:ui_scroll_view_];

  [[delegate expect] scrollViewShouldScrollToTop:scroll_view_];
  [scroll_view_proxy_ scrollViewShouldScrollToTop:ui_scroll_view_];

  [[delegate expect] scrollViewWillBeginZooming:scroll_view_];
  [scroll_view_proxy_ scrollViewWillBeginZooming:ui_scroll_view_ withView:nil];

  [delegate verify];
}

}  // namespace ios_web_view
