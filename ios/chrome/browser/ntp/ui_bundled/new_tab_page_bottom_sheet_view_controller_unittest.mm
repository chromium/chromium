// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_collection_view.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_card_background_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/scroll_delegate_proxy.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

@interface NewTabPageBottomSheetViewController (Testing)
- (void)setupSuperviewConstraints;
- (void)handleFeedPan:(UIPanGestureRecognizer*)gesture;
- (void)applyBackgroundTheme;
- (CGFloat)headerHeight;
- (void)snapSheetWithVelocity:(CGPoint)velocity
              currentConstant:(CGFloat)currentConstant;
- (void)setSheetStateForTesting:(BottomSheetSnappingState)state;
- (BottomSheetSnappingState)sheetStateForTesting;
- (void)updateFeedSigninPromoVisibility;
@end

@interface LifecycleTrackingChildViewController : UIViewController
@property(nonatomic, assign) NSInteger didMoveToParentCount;
@property(nonatomic, assign) NSInteger willMoveToParentCount;
@end

@implementation LifecycleTrackingChildViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) {
    self.didMoveToParentCount++;
  }
}

- (void)willMoveToParentViewController:(UIViewController*)parent {
  [super willMoveToParentViewController:parent];
  if (!parent) {
    self.willMoveToParentCount++;
  }
}

@end

class NewTabPageBottomSheetViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    feature_list_.InitAndEnableFeature(kNewTabPageRedesign);
    view_controller_ = [[NewTabPageBottomSheetViewController alloc] init];
    view_controller_.traitOverrides.horizontalSizeClass =
        UIUserInterfaceSizeClassCompact;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  NewTabPageBottomSheetViewController* view_controller_;
};

// Tests that the view controller applies blur background when
// NewTabPageImageBackgroundTrait is true.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestBackgroundImageBackgroundTrait) {
  [view_controller_ loadViewIfNeeded];

  [[[CustomUITraitAccessor alloc]
      initWithMutableTraits:view_controller_.traitOverrides]
      setBoolForNewTabPageImageBackgroundTrait:YES];
  [view_controller_ applyBackgroundTheme];

  EXPECT_NSEQ(UIColor.clearColor, view_controller_.view.backgroundColor);
  UIVisualEffectView* blur_view =
      [view_controller_ valueForKey:@"_blurBackgroundView"];
  EXPECT_NE(nil, blur_view);
  EXPECT_FALSE(blur_view.hidden);
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
  MagicStackCollectionViewController* child_vc =
      [[MagicStackCollectionViewController alloc] init];
  view_controller_.magicStackViewController = child_vc;

  [view_controller_ loadViewIfNeeded];

  EXPECT_EQ(child_vc.parentViewController, view_controller_);
  UIView* container =
      [view_controller_ valueForKey:@"_magicStackContainerView"];
  EXPECT_NE(nil, container);
  EXPECT_TRUE([child_vc.view isDescendantOfView:container]);
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

// Tests that the header container is placed inside feedScrollView with top
// contentInset when feed is present.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestHeaderContainerEmbeddedInFeedScrollViewWithTopInset) {
  UIViewController* child_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [child_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = child_vc;
  [view_controller_ loadViewIfNeeded];

  UIView* headerContainer =
      [view_controller_ valueForKey:@"_headerContainerView"];
  EXPECT_NE(nil, headerContainer);
  EXPECT_TRUE([headerContainer isDescendantOfView:scroll_view]);

  CGFloat expectedHeaderHeight = [view_controller_ headerHeight];
  EXPECT_FLOAT_EQ(expectedHeaderHeight, scroll_view.contentInset.top);
  EXPECT_FLOAT_EQ(expectedHeaderHeight,
                  scroll_view.verticalScrollIndicatorInsets.top);
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

// Tests that setting setOmniboxInBottomPosition:YES applies bottom content
// insets.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestBottomOmniboxFeedInsets) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kNewTabPageRedesign);

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

  // Set state to expanded state.
  [view_controller_
      setSheetStateForTesting:BottomSheetSnappingState::kExpanded];
  [view_controller_ setOmniboxInBottomPosition:YES];

  UIScrollView* feedScrollView =
      [view_controller_ valueForKey:@"_feedScrollView"];
  EXPECT_GT(feedScrollView.contentInset.bottom, 0.0);

  // Switching back to NO should reset bottom contentInset to 0.
  [view_controller_ setOmniboxInBottomPosition:NO];
  EXPECT_EQ(feedScrollView.contentInset.bottom, 0.0);
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
  [view_controller_
      setSheetStateForTesting:BottomSheetSnappingState::kExpanded];
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
  [view_controller_
      setSheetStateForTesting:BottomSheetSnappingState::kExpanded];
  NSLayoutConstraint* topConstraint =
      [view_controller_ valueForKey:@"_bottomSheetTopConstraint"];
  topConstraint.constant = 100.0;
  scroll_view.contentOffset = CGPointMake(0, -scroll_view.contentInset.top);

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
  EXPECT_FLOAT_EQ(-scroll_view.contentInset.top, scroll_view.contentOffset.y);

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
  [view_controller_
      setSheetStateForTesting:BottomSheetSnappingState::kExpanded];
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

// Tests that magic stack parentage matches feedViewController when embedded
// inside feedScrollView, and falls back to NewTabPageBottomSheetViewController
// when feed is absent.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestMagicStackParentageWithAndWithoutFeed) {
  MagicStackCollectionViewController* magic_stack_vc =
      [[MagicStackCollectionViewController alloc] init];
  view_controller_.magicStackViewController = magic_stack_vc;

  // Without feed: magic stack is child of view_controller_.
  [view_controller_ loadViewIfNeeded];
  EXPECT_EQ(view_controller_, magic_stack_vc.parentViewController);

  // With feed containing scroll view: magic stack is child of feed_vc.
  UIViewController* feed_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [feed_vc.view addSubview:scroll_view];
  view_controller_.feedViewController = feed_vc;

  EXPECT_EQ(feed_vc, magic_stack_vc.parentViewController);

  // Detaching feed: magic stack falls back to view_controller_.
  view_controller_.feedViewController = nil;
  EXPECT_EQ(view_controller_, magic_stack_vc.parentViewController);
}

// Tests that detaching feedViewController resets feedScrollView contentInset to
// UIEdgeInsetsZero.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestFeedInsetsResetOnDetachment) {
  UIViewController* feed_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [feed_vc.view addSubview:scroll_view];

  view_controller_.feedViewController = feed_vc;
  [view_controller_ loadViewIfNeeded];

  CGFloat expectedHeaderHeight = [view_controller_ headerHeight];
  EXPECT_FLOAT_EQ(expectedHeaderHeight, scroll_view.contentInset.top);

  // Detach feed view controller.
  view_controller_.feedViewController = nil;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(UIEdgeInsetsZero,
                                            scroll_view.contentInset));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      UIEdgeInsetsZero, scroll_view.verticalScrollIndicatorInsets));
}

// Tests that children receive exactly one didMoveToParentViewController call
// upon initial load and attachment.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestZeroRedundantLifecycleCalls) {
  LifecycleTrackingChildViewController* magic_stack_vc =
      [[LifecycleTrackingChildViewController alloc] init];
  LifecycleTrackingChildViewController* feed_vc =
      [[LifecycleTrackingChildViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  [feed_vc.view addSubview:scroll_view];

  view_controller_.magicStackViewController =
      (MagicStackCollectionViewController*)magic_stack_vc;
  view_controller_.feedViewController = feed_vc;

  [view_controller_ loadViewIfNeeded];

  EXPECT_EQ(1, magic_stack_vc.didMoveToParentCount);
  EXPECT_EQ(1, feed_vc.didMoveToParentCount);
}

// Tests that setting feedViewController = nil or magicStackViewController = nil
// cleanly resets parentViewController to nil and removes the view.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestCleanDetachmentOnNilAssignment) {
  UIViewController* feed_vc = [[UIViewController alloc] init];
  MagicStackCollectionViewController* magic_stack_vc =
      [[MagicStackCollectionViewController alloc] init];

  view_controller_.feedViewController = feed_vc;
  view_controller_.magicStackViewController = magic_stack_vc;
  [view_controller_ loadViewIfNeeded];

  EXPECT_NE(nil, feed_vc.parentViewController);
  EXPECT_NE(nil, feed_vc.view.superview);
  EXPECT_NE(nil, magic_stack_vc.parentViewController);
  EXPECT_NE(nil, magic_stack_vc.view.superview);

  view_controller_.feedViewController = nil;
  EXPECT_EQ(nil, feed_vc.parentViewController);
  EXPECT_EQ(nil, feed_vc.view.superview);

  view_controller_.magicStackViewController = nil;
  EXPECT_EQ(nil, magic_stack_vc.parentViewController);
  EXPECT_EQ(nil, magic_stack_vc.view.superview);
}

// Tests that calling invalidate cleanly unparents all children and clears
// references.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestInvalidateCleansUpChildren) {
  UIViewController* feed_vc = [[UIViewController alloc] init];
  MagicStackCollectionViewController* magic_stack_vc =
      [[MagicStackCollectionViewController alloc] init];

  view_controller_.feedViewController = feed_vc;
  view_controller_.magicStackViewController = magic_stack_vc;
  [view_controller_ loadViewIfNeeded];

  [view_controller_ invalidate];

  EXPECT_EQ(nil, feed_vc.parentViewController);
  EXPECT_EQ(nil, feed_vc.view.superview);
  EXPECT_EQ(nil, magic_stack_vc.parentViewController);
  EXPECT_EQ(nil, magic_stack_vc.view.superview);
}

// Tests that headerHeight returns 0.0 when in iPad regular layout mode.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestIPadRegularHeaderHeight) {
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;

  UIViewController* feed_vc = [[UIViewController alloc] init];
  MagicStackCollectionViewController* magic_stack_vc =
      [[MagicStackCollectionViewController alloc] init];
  view_controller_.feedViewController = feed_vc;
  view_controller_.magicStackViewController = magic_stack_vc;
  [view_controller_ loadViewIfNeeded];

  EXPECT_FLOAT_EQ(0.0, [view_controller_ headerHeight]);
}

// Tests that dragging downward on iPad regular clamps to Resting state and
// never enters Collapsed state.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestIPadRegularSnappingDoesNotCollapse) {
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;

  [view_controller_ loadViewIfNeeded];

  // Provide a large downward velocity and currentConstant beyond resting.
  CGFloat resting_offset = [view_controller_ restingOffset];
  [view_controller_ snapSheetWithVelocity:CGPointMake(0, 1500)
                          currentConstant:resting_offset + 200.0];

  EXPECT_EQ(BottomSheetSnappingState::kResting,
            [view_controller_ sheetStateForTesting]);
}

// Tests that collapseToRestingAnimated sets the sheet state to Resting.
TEST_F(NewTabPageBottomSheetViewControllerTest, TestCollapseToRestingAnimated) {
  [view_controller_ loadViewIfNeeded];
  [view_controller_
      setSheetStateForTesting:BottomSheetSnappingState::kExpanded];

  [view_controller_ collapseToRestingAnimated:NO];

  EXPECT_EQ(BottomSheetSnappingState::kResting,
            [view_controller_ sheetStateForTesting]);
}

// Tests that the bottom sheet view uses maskedCorners with 24.0 cornerRadius on
// top corners only on iPad regular layout, and all 4 corners on compact layout.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestIPadRegularLayoutMaskedCorners) {
  UIView* superview =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)];
  [superview addSubview:view_controller_.view];

  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;

  [view_controller_ loadViewIfNeeded];
  [view_controller_ updateLayoutModeForCurrentTraitCollection];
  EXPECT_FLOAT_EQ(24.0, view_controller_.view.layer.cornerRadius);
  CACornerMask expectedRegularCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  EXPECT_EQ(expectedRegularCorners, view_controller_.view.layer.maskedCorners);

  // When switching to compact layout, all 4 corners should be rounded.
  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  [view_controller_ updateLayoutModeForCurrentTraitCollection];
  CACornerMask expectedCompactCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner | kCALayerMinXMaxYCorner |
      kCALayerMaxXMaxYCorner;
  EXPECT_EQ(expectedCompactCorners, view_controller_.view.layer.maskedCorners);
}

// Tests that updateLayoutModeForCurrentTraitCollection configures iPad regular
// layout correctly with bottom anchoring, top-aligned content container, and
// card background styling.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestIPadRegularLayoutConfiguration) {
  UIView* superview =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 1024, 768)];
  [superview addSubview:view_controller_.view];

  view_controller_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassRegular;

  [view_controller_ loadViewIfNeeded];
  [view_controller_ setupSuperviewConstraints];

  // Background color should match NTPCardBackgroundColor.
  EXPECT_NSEQ(NTPCardBackgroundColor(
                  [view_controller_.traitCollection objectForNewTabPageTrait]),
              view_controller_.view.backgroundColor);

  [superview layoutIfNeeded];
  // On iPad regular layout, the sheet is centered horizontally and pinned to
  // the bottom.
  EXPECT_FLOAT_EQ(CGRectGetMaxY(superview.bounds),
                  CGRectGetMaxY(view_controller_.view.frame));
  EXPECT_FLOAT_EQ(CGRectGetMidX(superview.bounds),
                  CGRectGetMidX(view_controller_.view.frame));
  EXPECT_FLOAT_EQ(
      content_suggestions::SearchFieldWidth(superview.bounds.size.width,
                                            view_controller_.traitCollection),
      view_controller_.view.frame.size.width);
}

// Tests that feed insets correctly expand to include feedTopSectionHeight.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestFeedInsetsWithFeedTopSection) {
  UIViewController* promo_vc = [[UIViewController alloc] init];
  promo_vc.view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 390, 120)];
  [promo_vc.view.heightAnchor constraintEqualToConstant:120.0].active = YES;
  view_controller_.feedTopSectionViewController = promo_vc;

  UIViewController* feed_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 390, 800)];
  [feed_vc.view addSubview:scroll_view];
  view_controller_.feedViewController = feed_vc;

  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  CGFloat promo_height = [view_controller_ feedTopSectionHeight];
  EXPECT_GT(promo_height, 0.0);

  // The top inset expands to accommodate the promo plus the 16pt inter-module
  // spacing.
  constexpr CGFloat kExpectedPromoSpacing = 16.0;
  CGFloat expected_header_height = [view_controller_ headerHeight];
  CGFloat expected_top_inset =
      expected_header_height + promo_height + kExpectedPromoSpacing;
  EXPECT_FLOAT_EQ(expected_top_inset, scroll_view.contentInset.top);

  // When the promo view is hidden, feedTopSectionHeight is 0.
  promo_vc.view.hidden = YES;
  EXPECT_FLOAT_EQ(0.0, [view_controller_ feedTopSectionHeight]);

  [view_controller_ updateFeedLayout];
  EXPECT_FLOAT_EQ(expected_header_height, scroll_view.contentInset.top);
}

// Tests that handleFeedTopSectionClosed animates hidden and preserves child VC
// hierarchy.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestHandleFeedTopSectionClosed) {
  UIViewController* promo_vc = [[UIViewController alloc] init];
  promo_vc.view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 390, 100)];
  [promo_vc.view.heightAnchor constraintEqualToConstant:100.0].active = YES;
  view_controller_.feedTopSectionViewController = promo_vc;

  UIViewController* feed_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 390, 800)];
  [feed_vc.view addSubview:scroll_view];
  view_controller_.feedViewController = feed_vc;

  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  EXPECT_FALSE(promo_vc.view.hidden);

  [view_controller_ handleFeedTopSectionClosed];

  EXPECT_TRUE(promo_vc.view.hidden);
  // Child VC lifecycle must remain intact (not detached by dismiss animation).
  EXPECT_EQ(promo_vc.parentViewController, feed_vc);
}

// Tests that promo visibility is not reported as YES when sheet has no window.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestFeedSigninPromoVisibilityReportingWithoutWindow) {
  UIViewController* promo_vc = [[UIViewController alloc] init];
  promo_vc.view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 390, 100)];
  [promo_vc.view.heightAnchor constraintEqualToConstant:100.0].active = YES;
  view_controller_.feedTopSectionViewController = promo_vc;

  UIViewController* feed_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 390, 800)];
  [feed_vc.view addSubview:scroll_view];
  view_controller_.feedViewController = feed_vc;

  id delegate =
      OCMProtocolMock(@protocol(NewTabPageBottomSheetViewControllerDelegate));
  view_controller_.delegate = delegate;

  [view_controller_ loadViewIfNeeded];

  // Without a window, visibility should not report YES.
  [[delegate reject] bottomSheetViewController:view_controller_
                didChangeSigninPromoVisibility:YES];
  [view_controller_ updateFeedSigninPromoVisibility];
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that embedMostVisitedView embeds the view, and passing nil removes it
// and updates insets.
TEST_F(NewTabPageBottomSheetViewControllerTest,
       TestEmbedMostVisitedViewAndDetach) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kMVTInBottomSheet);

  UIViewController* feed_vc = [[UIViewController alloc] init];
  UIScrollView* scroll_view =
      [[UIScrollView alloc] initWithFrame:CGRectMake(0, 0, 390, 800)];
  [feed_vc.view addSubview:scroll_view];
  view_controller_.feedViewController = feed_vc;

  [view_controller_ loadViewIfNeeded];
  [view_controller_.view layoutIfNeeded];

  CGFloat insets_without_mvt = scroll_view.contentInset.top;

  UIView* mvt_view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 300, 120)];
  [mvt_view.heightAnchor constraintEqualToConstant:120.0].active = YES;

  [view_controller_ embedMostVisitedView:mvt_view];
  [view_controller_.view layoutIfNeeded];

  UIView* container =
      [view_controller_ valueForKey:@"_mostVisitedContainerView"];
  ASSERT_TRUE(container != nil);
  EXPECT_EQ(mvt_view.superview, container);
  EXPECT_GT(scroll_view.contentInset.top, insets_without_mvt);

  // Detach by passing nil.
  [view_controller_ embedMostVisitedView:nil];
  [view_controller_.view layoutIfNeeded];

  EXPECT_EQ(mvt_view.superview, nil);
  EXPECT_EQ(scroll_view.contentInset.top, insets_without_mvt);
}
