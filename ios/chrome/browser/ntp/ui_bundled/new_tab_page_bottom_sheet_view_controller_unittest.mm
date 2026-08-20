// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/scroll_delegate_proxy.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

@interface NewTabPageBottomSheetViewController (Testing)
- (void)updateContentContainerInsetForOffset:(CGFloat)topOffset;
- (void)voiceOverStatusDidChange;
- (BOOL)isVoiceOverRunning;
- (void)setupSuperviewConstraints;
- (void)handleFeedPan:(UIPanGestureRecognizer*)gesture;
@end

class NewTabPageBottomSheetViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[NewTabPageBottomSheetViewController alloc] init];
  }

 protected:
  NewTabPageBottomSheetViewController* view_controller_;
};

// Tests that the view controller loads its view correctly.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestLoadView) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(nil, view_controller_.view);
}

// Tests that the feed view controller is correctly embedded as a child view
// controller.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestEmbedFeedViewController) {
  UIViewController* child_vc = [[UIViewController alloc] init];
  view_controller_.feedViewController = child_vc;

  [view_controller_ loadViewIfNeeded];

  EXPECT_EQ(child_vc.parentViewController, view_controller_);
  EXPECT_TRUE([child_vc.view isDescendantOfView:view_controller_.view]);
}

// Tests that the magic stack view controller is correctly embedded as a child
// view controller.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestEmbedMagicStackViewController) {
  UIViewController* child_vc = [[UIViewController alloc] init];
  view_controller_.magicStackViewController = child_vc;

  [view_controller_ loadViewIfNeeded];

  EXPECT_EQ(child_vc.parentViewController, view_controller_);
  UIView* container =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  EXPECT_NE(nil, container);
  EXPECT_TRUE([child_vc.view isDescendantOfView:container]);
}

// Tests that the magic stack container view alpha interpolates with progress in
// legacy mode.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestMagicStackContainerLegacyAlpha) {
  [view_controller_ loadViewIfNeeded];
  UIView* container =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  EXPECT_NE(nil, container);

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  CGFloat expanded = [view_controller_ expandedOffset];
  CGFloat resting = [view_controller_ restingOffset];

  // At resting offset, alpha is 1.0
  [view_controller_ updateContentContainerInsetForOffset:resting];
  EXPECT_FLOAT_EQ(1.0, container.alpha);

  // At expanded offset, alpha is 0.0
  [view_controller_ updateContentContainerInsetForOffset:expanded];
  EXPECT_FLOAT_EQ(0.0, container.alpha);

  // At halfway between expanded and resting, alpha is 0.5
  [view_controller_
      updateContentContainerInsetForOffset:(expanded + resting) / 2.0];
  EXPECT_FLOAT_EQ(0.5, container.alpha);
}

// Tests that the magic stack container view alpha remains 1.0 across top
// offsets in static-fakebox mode.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestMagicStackContainerStaticFakeboxRemainsVisible) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kNewTabPageRedesign, {{kNewTabPageRedesignStaticFakeboxParam, "true"}});

  [view_controller_ loadViewIfNeeded];
  UIView* container =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  EXPECT_NE(nil, container);

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  CGFloat expanded = [view_controller_ expandedOffset];
  CGFloat resting = [view_controller_ restingOffset];

  // At resting offset, alpha is 1.0
  [view_controller_ updateContentContainerInsetForOffset:resting];
  EXPECT_FLOAT_EQ(1.0, container.alpha);

  // At expanded offset, alpha remains 1.0
  [view_controller_ updateContentContainerInsetForOffset:expanded];
  EXPECT_FLOAT_EQ(1.0, container.alpha);

  // At halfway between expanded and resting, alpha remains 1.0
  [view_controller_
      updateContentContainerInsetForOffset:(expanded + resting) / 2.0];
  EXPECT_FLOAT_EQ(1.0, container.alpha);
}

// Tests that the header container embeds magic stack container view and exists.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestHeaderContainerHierarchy) {
  [view_controller_ loadViewIfNeeded];
  UIView* headerContainer =
      [view_controller_ valueForKey:@"_headerContainerView"];
  EXPECT_NE(nil, headerContainer);
  EXPECT_TRUE([headerContainer isDescendantOfView:view_controller_.view]);

  UIView* magicStackContainer =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  EXPECT_NE(nil, magicStackContainer);
  EXPECT_TRUE([magicStackContainer isDescendantOfView:headerContainer]);

  UIView* contentContainer =
      [view_controller_ valueForKey:@"_contentContainerView"];
  EXPECT_NE(nil, contentContainer);
  EXPECT_TRUE([contentContainer isDescendantOfView:view_controller_.view]);
}

// Tests that the MVT container view alpha updates based on top offset when
// kMVTInBottomSheet is enabled.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestMVTContainerAlphaWhenEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMVTInBottomSheet);

  [view_controller_ loadViewIfNeeded];
  UIView* mvtContainer =
      [view_controller_ valueForKey:@"_mostVisitedContainerView"];
  EXPECT_NE(nil, mvtContainer);

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  CGFloat expanded = [view_controller_ expandedOffset];
  CGFloat resting = [view_controller_ restingOffset];

  // At resting offset, progress should be 1.0, meaning alpha is 1.0
  [view_controller_ updateContentContainerInsetForOffset:resting];
  EXPECT_FLOAT_EQ(1.0, mvtContainer.alpha);

  // At expanded offset, progress should be 0.0, meaning alpha is 0.0
  [view_controller_ updateContentContainerInsetForOffset:expanded];
  EXPECT_FLOAT_EQ(0.0, mvtContainer.alpha);

  // At halfway between expanded and resting, progress should be 0.5, alpha
  // should be 0.5
  [view_controller_
      updateContentContainerInsetForOffset:(expanded + resting) / 2.0];
  EXPECT_FLOAT_EQ(0.5, mvtContainer.alpha);
}

// Tests that the scroll delegate proxy is injected when VoiceOver is enabled.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestVoiceOverProxyInjection) {
  UIViewController* child_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [child_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = child_vc;
  [view_controller_ loadViewIfNeeded];

  __block BOOL isVoiceOver = YES;
  ScopedBlockSwizzler swizzler([NewTabPageBottomSheetViewController class],
                               @selector(isVoiceOverRunning),
                               ^BOOL(id self) { return isVoiceOver; });

  // Simulate VoiceOver ON.
  [view_controller_ voiceOverStatusDidChange];

  UIScrollView* feedScrollView = [view_controller_ valueForKey:@"_feedScrollView"];
  EXPECT_NE(nil, feedScrollView.delegate);

  // Simulate VoiceOver OFF.
  isVoiceOver = NO;
  [view_controller_ voiceOverStatusDidChange];

  EXPECT_EQ(nil, feedScrollView.delegate);
}

// Tests that expandedOffset queries the delegate.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestExpandedOffsetFromDelegate) {
  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(120.0);

  EXPECT_FLOAT_EQ(120.0, [view_controller_ expandedOffset]);
}

// Tests that setting setOmniboxInBottomPosition:YES in legacy mode
// (static-fakebox: false) does NOT apply bottom insets.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestBottomOmniboxFeedInsetsLegacy) {
  UIView* superview = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [superview addSubview:view_controller_.view];

  UIViewController* child_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [child_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = child_vc;
  [view_controller_ loadViewIfNeeded];

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(100.0);
  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  // Set state to expanded state (BottomSheetSnappingStateExpanded = 2)
  [view_controller_ setValue:@(2) forKey:@"_sheetState"];
  [view_controller_ setOmniboxInBottomPosition:YES];

  UIScrollView* feedScrollView =
      [view_controller_ valueForKey:@"_feedScrollView"];
  EXPECT_EQ(feedScrollView.contentInset.bottom, 0.0);
}

// Tests that setting setOmniboxInBottomPosition:YES applies bottom content
// insets in static-fakebox mode.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestBottomOmniboxFeedInsetsStaticFakebox) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kNewTabPageRedesign, {{kNewTabPageRedesignStaticFakeboxParam, "true"}});

  UIView* superview = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [superview addSubview:view_controller_.view];

  UIViewController* child_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [child_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = child_vc;
  [view_controller_ loadViewIfNeeded];

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(100.0);
  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  // Set state to expanded state (BottomSheetSnappingStateExpanded = 2)
  [view_controller_ setValue:@(2) forKey:@"_sheetState"];
  [view_controller_ setOmniboxInBottomPosition:YES];

  UIScrollView* feedScrollView =
      [view_controller_ valueForKey:@"_feedScrollView"];
  EXPECT_GT(feedScrollView.contentInset.bottom, 0.0);

  // Switching back to NO should reset bottom contentInset to 0.
  [view_controller_ setOmniboxInBottomPosition:NO];
  EXPECT_EQ(feedScrollView.contentInset.bottom, 0.0);
}

// Tests that updateContentContainerInsetForOffset correctly handles resting <=
// expanded in legacy mode.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestRestingBelowExpandedLegacy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMVTInBottomSheet);

  [view_controller_ loadViewIfNeeded];
  UIView* mvtContainer =
      [view_controller_ valueForKey:@"_mostVisitedContainerView"];
  NSLayoutConstraint* magicStackTopConstraint =
      [view_controller_ valueForKey:@"_magicStackTopConstraint"];

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);

  [view_controller_ updateContentContainerInsetForOffset:400.0];
  EXPECT_FLOAT_EQ(0.0, mvtContainer.alpha);
  EXPECT_FLOAT_EQ(content_suggestions::FakeOmniboxHeight(),
                  magicStackTopConstraint.constant);
}

// Tests that updateContentContainerInsetForOffset correctly handles resting <=
// expanded in static-fakebox mode.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestRestingBelowExpandedStaticFakebox) {
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{kNewTabPageRedesign,
                             {{kNewTabPageRedesignStaticFakeboxParam, "true"}}},
                            {kMVTInBottomSheet, {}}},
      /*disabled_features=*/{});

  [view_controller_ loadViewIfNeeded];
  UIView* magicStackContainer =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  UIView* mvtContainer =
      [view_controller_ valueForKey:@"_mostVisitedContainerView"];
  NSLayoutConstraint* magicStackTopConstraint =
      [view_controller_ valueForKey:@"_magicStackTopConstraint"];

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);

  [view_controller_ updateContentContainerInsetForOffset:400.0];
  EXPECT_FLOAT_EQ(1.0, magicStackContainer.alpha);
  EXPECT_FLOAT_EQ(0.0, mvtContainer.alpha);
  EXPECT_FLOAT_EQ(0.0, magicStackTopConstraint.constant);
}

// Tests that slow upward scrolling in the feed does not alter the bottom
// sheet's top constraint or reset gesture translation.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestFeedSlowScrollDoesNotDragSheet) {
  UIView* superview = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [superview addSubview:view_controller_.view];

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(100.0);
  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  UIViewController* child_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  scroll_view.contentSize = CGSizeMake(400, 2000);
  [child_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = child_vc;
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setupSuperviewConstraints];

  // Set sheet to expanded state.
  [view_controller_ setValue:@(2) forKey:@"_sheetState"];
  NSLayoutConstraint* topConstraint =
      [view_controller_ valueForKey:@"_bottomSheetTopConstraint"];
  topConstraint.constant = 100.0;

  id mock_gesture = OCMClassMock([UIPanGestureRecognizer class]);
  OCMStub([mock_gesture translationInView:[OCMArg any]])
      .andReturn(CGPointMake(0, -5.0));
  OCMStub([mock_gesture velocityInView:[OCMArg any]])
      .andReturn(CGPointMake(0, -10.0));
  OCMStub([mock_gesture state]).andReturn(UIGestureRecognizerStateChanged);

  // Expect that setTranslation: is NEVER called on the feed gesture recognizer.
  [[mock_gesture reject] setTranslation:CGPointZero inView:[OCMArg any]];

  [view_controller_ handleFeedPan:mock_gesture];

  EXPECT_FLOAT_EQ(100.0, topConstraint.constant);
  EXPECT_TRUE(scroll_view.bounces);
  [mock_gesture verify];
}

// Tests that downward panning when the feed is at contentOffset.y == 0 drags
// the bottom sheet down by the incremental delta.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestFeedPullDownAtTopDragsSheetIncrementally) {
  UIView* superview = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [superview addSubview:view_controller_.view];

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(100.0);
  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  UIViewController* child_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  scroll_view.contentSize = CGSizeMake(400, 2000);
  [child_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = child_vc;
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setupSuperviewConstraints];

  // Set sheet to expanded state.
  [view_controller_ setValue:@(2) forKey:@"_sheetState"];
  NSLayoutConstraint* topConstraint =
      [view_controller_ valueForKey:@"_bottomSheetTopConstraint"];
  topConstraint.constant = 100.0;
  scroll_view.contentOffset = CGPointZero;

  __block UIGestureRecognizerState gestureState = UIGestureRecognizerStateBegan;
  __block CGPoint gestureTranslation = CGPointZero;

  id mock_gesture = OCMClassMock([UIPanGestureRecognizer class]);
  [[mock_gesture reject] setTranslation:CGPointZero inView:[OCMArg any]];
  OCMStub([mock_gesture state]).andDo(^(NSInvocation* invocation) {
    [invocation setReturnValue:&gestureState];
  });
  OCMStub([mock_gesture translationInView:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        [invocation setReturnValue:&gestureTranslation];
      });
  OCMStub([mock_gesture velocityInView:[OCMArg any]]).andReturn(CGPointZero);

  // Step 1: Gesture begins.
  gestureState = UIGestureRecognizerStateBegan;
  gestureTranslation = CGPointMake(0, 0.0);
  [view_controller_ handleFeedPan:mock_gesture];
  EXPECT_FLOAT_EQ(100.0, topConstraint.constant);

  // Step 2: First downward delta (+15 pt).
  gestureState = UIGestureRecognizerStateChanged;
  gestureTranslation = CGPointMake(0, 15.0);
  [view_controller_ handleFeedPan:mock_gesture];
  EXPECT_FLOAT_EQ(115.0, topConstraint.constant);
  EXPECT_FALSE(scroll_view.bounces);
  EXPECT_FLOAT_EQ(0.0, scroll_view.contentOffset.y);

  // Step 3: Second downward delta (+10 pt, cumulative +25 pt).
  gestureTranslation = CGPointMake(0, 25.0);
  [view_controller_ handleFeedPan:mock_gesture];
  EXPECT_FLOAT_EQ(125.0, topConstraint.constant);

  [mock_gesture verify];
}

// Tests that downward panning when the feed is scrolled down
// (contentOffset.y > 0) does not drag the bottom sheet.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestFeedPullDownWhileScrolledDownDoesNotDragSheet) {
  UIView* superview = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [superview addSubview:view_controller_.view];

  id mock_delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = mock_delegate;

  OCMStub([mock_delegate
              expandedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(100.0);
  OCMStub([mock_delegate
              restingOffsetForBottomSheetViewController:view_controller_])
      .andReturn(400.0);
  OCMStub([mock_delegate
              collapsedOffsetForBottomSheetViewController:view_controller_])
      .andReturn(600.0);

  UIViewController* child_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  scroll_view.contentSize = CGSizeMake(400, 2000);
  [child_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = child_vc;
  [view_controller_ loadViewIfNeeded];
  [view_controller_ setupSuperviewConstraints];

  // Set sheet to expanded state.
  [view_controller_ setValue:@(2) forKey:@"_sheetState"];
  NSLayoutConstraint* topConstraint =
      [view_controller_ valueForKey:@"_bottomSheetTopConstraint"];
  topConstraint.constant = 100.0;

  // Scrolled 50 pt into feed content.
  scroll_view.contentOffset = CGPointMake(0, 50.0);

  id mock_gesture = OCMClassMock([UIPanGestureRecognizer class]);
  [[mock_gesture reject] setTranslation:CGPointZero inView:[OCMArg any]];

  // Pulling down with translation +10.
  OCMStub([mock_gesture translationInView:[OCMArg any]])
      .andReturn(CGPointMake(0, 10.0));
  OCMStub([mock_gesture state]).andReturn(UIGestureRecognizerStateChanged);

  [view_controller_ handleFeedPan:mock_gesture];

  EXPECT_FLOAT_EQ(100.0, topConstraint.constant);
  EXPECT_TRUE(scroll_view.bounces);
  [mock_gesture verify];
}
