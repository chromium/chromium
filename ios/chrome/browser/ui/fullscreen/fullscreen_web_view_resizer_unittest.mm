// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_resizer.h"

#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"
#include "ios/web/common/features.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTopToolbarExpandedHeight = 50;
const CGFloat kTopToolbarCollapsedHeight = 20;
const CGFloat kBottomToolbarHeight = 40;
const CGFloat kViewWidth = 300;
const CGFloat kViewHeight = 700;
}  // namespace

class FullscreenWebViewResizerTest : public PlatformTest {
 public:
  FullscreenWebViewResizerTest() {
    // FullscreenModel setup.
    _model.SetExpandedToolbarHeight(kTopToolbarExpandedHeight);
    _model.SetCollapsedToolbarHeight(kTopToolbarCollapsedHeight);
    _model.SetBottomToolbarHeight(kBottomToolbarHeight);
    _model.SetContentHeight(1000);
    _model.SetScrollViewHeight(700);
    _model.SetScrollViewIsScrolling(true);
    _model.SetYContentOffset(10);
    _model.ResetForNavigation();

    // WebState view setup.
    CGRect superviewFrame = CGRectMake(0, 0, kViewWidth, kViewHeight);
    _webStateSuperview = [[UIView alloc] initWithFrame:superviewFrame];
    _webStateView = [[UIView alloc] initWithFrame:CGRectZero];
    [_webStateSuperview addSubview:_webStateView];

    // WebState setup.
    _webState.SetView(_webStateView);
  }

 protected:
  base::test::ScopedFeatureList _features;
  FullscreenModel _model;
  UIView* _webStateSuperview;
  UIView* _webStateView;
  web::TestWebState _webState;
};

// Tests that updating the resizer works as expected.
TEST_F(FullscreenWebViewResizerTest, UpdateWebState) {
  ASSERT_EQ(1, _model.progress());
  FullscreenWebViewResizer* resizer =
      [[FullscreenWebViewResizer alloc] initWithModel:&_model];
  resizer.webState = &_webState;

  // The frame should be updated when setting the WebState.
  CGRect fullInsetFrame = CGRectMake(
      0, kTopToolbarExpandedHeight, kViewWidth,
      kViewHeight - kTopToolbarExpandedHeight - kBottomToolbarHeight);
  EXPECT_TRUE(CGRectEqualToRect(fullInsetFrame, _webStateView.frame));

  // Scroll the view then update the resizer.
  SimulateFullscreenUserScrollForProgress(&_model, 0.0);
  ASSERT_EQ(0, _model.progress());
  [resizer updateForCurrentState];
  CGRect smallInsetFrame = CGRectMake(0, kTopToolbarCollapsedHeight, kViewWidth,
                                      kViewHeight - kTopToolbarCollapsedHeight);
  EXPECT_TRUE(CGRectEqualToRect(smallInsetFrame, _webStateView.frame));
}

// Tests that it is possible to force the update the of the Resizer to a value
// differente from the model's one.
TEST_F(FullscreenWebViewResizerTest, ForceUpdateWebState) {
  FullscreenWebViewResizer* resizer =
      [[FullscreenWebViewResizer alloc] initWithModel:&_model];
  resizer.webState = &_webState;

  ASSERT_EQ(1, _model.progress());

  // The frame should be updated when setting the WebState.
  CGRect smallInsetFrame = CGRectMake(0, kTopToolbarCollapsedHeight, kViewWidth,
                                      kViewHeight - kTopToolbarCollapsedHeight);
  [resizer forceToUpdateToProgress:0];
  EXPECT_TRUE(CGRectEqualToRect(smallInsetFrame, _webStateView.frame));
}

// Tests that nothing happen if there is no superview for the web state view.
TEST_F(FullscreenWebViewResizerTest, WebStateNoSuperview) {
  // WebState view setup.
  CGRect webViewFrame = CGRectMake(0, 0, kViewWidth, kViewHeight);
  UIView* webStateView = [[UIView alloc] initWithFrame:webViewFrame];

  // WebState setup.
  web::TestWebState webState;
  webState.SetView(webStateView);

  FullscreenWebViewResizer* resizer =
      [[FullscreenWebViewResizer alloc] initWithModel:&_model];
  resizer.webState = &webState;

  EXPECT_TRUE(CGRectEqualToRect(webViewFrame, webStateView.frame));

  [resizer updateForCurrentState];
  EXPECT_TRUE(CGRectEqualToRect(webViewFrame, webStateView.frame));
}

// Tests that nothing happen (no crash) if there is no web state.
TEST_F(FullscreenWebViewResizerTest, NoWebState) {
  FullscreenWebViewResizer* resizer =
      [[FullscreenWebViewResizer alloc] initWithModel:&_model];

  [resizer updateForCurrentState];
}
