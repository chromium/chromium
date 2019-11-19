// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "ios/web_view/test/observer.h"
#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Tests that the KVO compliant properties of CWVScrollView correctly report
// changes.
typedef ios_web_view::WebViewInttestBase ScrollViewKvoTest;

// Tests that CWVScrollView correctly reports |contentOffset| state.
TEST_F(ScrollViewKvoTest, contentOffset) {
  Observer* offset_observer = [[Observer alloc] init];
  [offset_observer setObservedObject:web_view_.scrollView
                             keyPath:@"contentOffset"];
  Observer* size_observer = [[Observer alloc] init];
  [size_observer setObservedObject:web_view_.scrollView keyPath:@"contentSize"];

  // A page must be loaded before changing |contentOffset|. Otherwise the change
  // is ignored because the underlying UIScrollView hasn't been created.
  [web_view_
      loadRequest:[NSURLRequest
                      requestWithURL:[NSURL URLWithString:@"about:blank"]]];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return size_observer.lastValue;
      }));

  web_view_.scrollView.contentOffset = CGPointMake(10, 20);
  EXPECT_NSEQ([NSValue valueWithCGPoint:CGPointMake(10, 20)],
              offset_observer.lastValue);

  [web_view_.scrollView setContentOffset:CGPointMake(30, 40) animated:YES];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return static_cast<bool>([offset_observer.lastValue
            isEqual:[NSValue valueWithCGPoint:CGPointMake(30, 40)]]);
      }));
}

}  // namespace ios_web_view
