// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/ui_view_controller_with_display_tracing.h"

#import <QuartzCore/QuartzCore.h>

#import <tuple>

#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/test/tracing/trace_test_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface UIViewControllerWithDisplayTracing (Testing)
- (double)currentMediaTime;
- (void)handleCATransactionCommitEndWithLink:(UIUpdateLink*)link
                                        info:(UIUpdateInfo*)info
                                isLowLatency:(BOOL)isLowLatency;
- (void)handleTapGesture:(UITapGestureRecognizer*)sender;
- (void)handlePanGesture:(UIPanGestureRecognizer*)sender;
- (void)handleUpdatePhase:(int)phaseId;
- (const char*)currentGestureForTesting;
@end

@interface TestUIViewControllerWithDisplayTracing
    : UIViewControllerWithDisplayTracing
@property(nonatomic, assign) double mockedMediaTime;
@end

@implementation TestUIViewControllerWithDisplayTracing
- (double)currentMediaTime {
  return self.mockedMediaTime != 0.0 ? self.mockedMediaTime
                                     : [super currentMediaTime];
}
@end

@interface TestUITapGestureRecognizer : UITapGestureRecognizer
@property(nonatomic, assign) UIGestureRecognizerState mockedState;
@end

@implementation TestUITapGestureRecognizer
- (UIGestureRecognizerState)state {
  return self.mockedState;
}
@end

@interface TestUIPanGestureRecognizer : UIPanGestureRecognizer
@property(nonatomic, assign) UIGestureRecognizerState mockedState;
@end

@implementation TestUIPanGestureRecognizer
- (UIGestureRecognizerState)state {
  return self.mockedState;
}
@end

@interface TestUISwipeGestureRecognizer : UISwipeGestureRecognizer
@property(nonatomic, assign) UIGestureRecognizerState mockedState;
@end

@implementation TestUISwipeGestureRecognizer
- (UIGestureRecognizerState)state {
  return self.mockedState;
}
@end

namespace {

constexpr double kInitialMediaTime = 1000.0;
constexpr double kFramePeriod10FPS = 1.0 / 10.0;
constexpr double kFramePeriod60FPS = 1.0 / 60.0;
constexpr double kFramePeriod120FPS = 1.0 / 120.0;
constexpr double kToleranceEpsilon = 1e-5;

// The test fixture's parameters are:
// [0] BOOL isLowLatency
// [1] double framePeriod (seconds)
// [2] double framePhase: The fraction of a vsync cycle that has elapsed
//      between the last vsync and the current frame's commit time
class UIViewControllerWithDisplayTracingTest
    : public PlatformTest,
      public ::testing::WithParamInterface<std::tuple<BOOL, double, double>> {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[TestUIViewControllerWithDisplayTracing alloc]
        initWithDisplayTracingOptions:
            UIViewControllerDisplayTracingOptionAllTraces];
    // Enable variable refresh rate by default at max 120FPS.
    [view_controller_ setValue:@YES forKey:@"isVariableRefreshRate"];
    [view_controller_ setValue:@(kFramePeriod120FPS)
                        forKey:@"minSupportedFramePeriod"];
  }

  void TearDown() override {
    view_controller_ = nil;
    PlatformTest::TearDown();
  }

  int SimulateFrame(double targetTime, BOOL isLowLatency = NO) {
    id infoMock = OCMClassMock([UIUpdateInfo class]);
    OCMStub([infoMock estimatedPresentationTime]).andReturn(targetTime);
    [view_controller_ handleCATransactionCommitEndWithLink:nil
                                                      info:infoMock
                                              isLowLatency:isLowLatency];
    return [[view_controller_ valueForKey:@"lastDroppedFrames"] intValue];
  }

  base::test::TracingEnvironment tracing_environment_;
  base::test::TaskEnvironment task_environment_;
  TestUIViewControllerWithDisplayTracing* view_controller_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    UIViewControllerWithDisplayTracingTest,
    ::testing::Values(std::make_tuple(NO, kFramePeriod10FPS, 0.01),
                      std::make_tuple(NO, kFramePeriod10FPS, 0.99),
                      std::make_tuple(NO, kFramePeriod60FPS, 0.5),
                      std::make_tuple(NO, kFramePeriod120FPS, 0.01),
                      std::make_tuple(NO, kFramePeriod120FPS, 0.99),
                      std::make_tuple(YES, kFramePeriod120FPS, 0.01),
                      std::make_tuple(YES, kFramePeriod120FPS, 0.99),
                      std::make_tuple(YES, kFramePeriod60FPS, 0.5)));

// Verifies that transitioning from 60FPS to the target frame rate updates
// the currentFramePeriodEstimate correctly.
TEST_P(UIViewControllerWithDisplayTracingTest, TransitionUpdatesEstimate) {
  BOOL isLowLatency = std::get<0>(GetParam());
  double framePeriod = std::get<1>(GetParam());
  double framePhase = std::get<2>(GetParam());
  double downstreamPipelineLatency =
      ((isLowLatency ? 0 : 1) + (1.0 - framePhase)) * framePeriod;

  // Assume the display is initially operating at 60 FPS.
  [view_controller_ setValue:@(kFramePeriod60FPS)
                      forKey:@"currentFramePeriodEstimate"];

  view_controller_.mockedMediaTime = kInitialMediaTime;
  // Target time is far enough into the future to not trigger a dropped frame.
  double target = kInitialMediaTime + downstreamPipelineLatency;
  SimulateFrame(target, isLowLatency);

  // Verify lastTargetPresentationTime is set.
  EXPECT_DOUBLE_EQ([[view_controller_ valueForKey:@"lastTargetPresentationTime"]
                       doubleValue],
                   target);

  // Next frame arrives exactly framePeriod seconds later.
  view_controller_.mockedMediaTime += framePeriod;
  target += framePeriod;
  int dropped = SimulateFrame(target, isLowLatency);

  EXPECT_NEAR([[view_controller_ valueForKey:@"currentFramePeriodEstimate"]
                  doubleValue],
              framePeriod, kToleranceEpsilon);
  EXPECT_EQ(dropped, 0);
}

// Verifies that dropped frames are calculated correctly based on missed
// deadlines.
TEST_P(UIViewControllerWithDisplayTracingTest,
       DroppedFramesCalculatedCorrectly) {
  BOOL isLowLatency = std::get<0>(GetParam());
  double framePeriod = std::get<1>(GetParam());
  double framePhase = std::get<2>(GetParam());

  // Minimum latency threshold is 1 period if pipeline is double buffered.
  double pipelineMinLatency = isLowLatency ? 0.0 : framePeriod;

  // Assume a steady frame period estimate.
  [view_controller_ setValue:@(framePeriod)
                      forKey:@"currentFramePeriodEstimate"];

  // Establish lastTargetPresentationTime to check that it gets ignored
  // because it is stale.
  [view_controller_ setValue:@(123.4) forKey:@"lastTargetPresentationTime"];

  // Set a fixed, deterministic media time.
  view_controller_.mockedMediaTime = kInitialMediaTime;
  double earliestPresentation1 = kInitialMediaTime + pipelineMinLatency;
  // Set up target time so that we miss the deadline by `framePhase` of a frame
  // period.
  double target1 = earliestPresentation1 - framePhase * framePeriod;
  int dropped1 = SimulateFrame(target1, isLowLatency);

  EXPECT_EQ(dropped1, 1);

  // Verify that currentFramePeriodEstimate is not updated.
  EXPECT_DOUBLE_EQ([[view_controller_ valueForKey:@"currentFramePeriodEstimate"]
                       doubleValue],
                   framePeriod);

  // Simulate another commit with 2 dropped frames.
  view_controller_.mockedMediaTime = 2 * kInitialMediaTime;
  double earliestPresentation2 =
      view_controller_.mockedMediaTime + pipelineMinLatency;
  // Set up target time so that we miss the deadline by `1 + framePhase` frame
  // periods. This causes the calculation to round up to 2 dropped frames.
  double target2 = earliestPresentation2 - (1 + framePhase) * framePeriod;
  int dropped2 = SimulateFrame(target2, isLowLatency);

  EXPECT_EQ(dropped2, 2);
  // Verify that currentFramePeriodEstimate is not updated.
  EXPECT_DOUBLE_EQ([[view_controller_ valueForKey:@"currentFramePeriodEstimate"]
                       doubleValue],
                   framePeriod);
}

class UIViewControllerWithDisplayTracingGestureTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[TestUIViewControllerWithDisplayTracing alloc]
        initWithDisplayTracingOptions:
            UIViewControllerDisplayTracingOptionGesture];
  }

  void TearDown() override {
    view_controller_ = nil;
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  TestUIViewControllerWithDisplayTracing* view_controller_;
};

// Verifies that a tap gesture on a UIControl is not disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       TapGestureOnControlNotDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIControl* button =
      [[UIControl alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:button];

  id senderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  EXPECT_EQ([view_controller_ currentGestureForTesting], nullptr);

  [view_controller_ handleTapGesture:senderMock];

  EXPECT_STREQ([view_controller_ currentGestureForTesting], "Tap");
}

// Verifies that a tap gesture on a plain UIView with no gesture recognizer is
// disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       TapGestureOnPlainViewDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* plainView = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:plainView];

  id senderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  [view_controller_ handleTapGesture:senderMock];

  EXPECT_EQ([view_controller_ currentGestureForTesting], nullptr);
}

// Verifies that a tap gesture on a subview with a recognized tap gesture
// recognizer is not disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       TapGestureOnViewWithRecognizedTapNotDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* subview = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:subview];

  TestUITapGestureRecognizer* subviewRecognizer =
      [[TestUITapGestureRecognizer alloc] initWithTarget:nil action:nil];
  subviewRecognizer.mockedState = UIGestureRecognizerStateRecognized;
  [subview addGestureRecognizer:subviewRecognizer];

  id senderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  [view_controller_ handleTapGesture:senderMock];

  EXPECT_STREQ([view_controller_ currentGestureForTesting], "Tap");
}

// Verifies that when a child view controller (also using
// UIViewControllerWithDisplayTracing) is nested within the parent, a tap
// gesture on a subview correctly records the gesture on both parent and child
// view controllers.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       TapGestureOnNestedViewControllerBothRecordGesture) {
  // Set up parent view hierarchy.
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  view_controller_.view = rootView;

  // Set up nested child view controller and view.
  TestUIViewControllerWithDisplayTracing* childViewController =
      [[TestUIViewControllerWithDisplayTracing alloc]
          initWithDisplayTracingOptions:
              UIViewControllerDisplayTracingOptionGesture];
  [view_controller_ addChildViewController:childViewController];
  UIView* childView = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 80, 80)];
  childViewController.view = childView;
  [rootView addSubview:childView];
  [childViewController didMoveToParentViewController:view_controller_];

  // Set up subview inside the child view controller's view.
  UIView* subview = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [childView addSubview:subview];

  // Add a recognized tap gesture recognizer to the leaf subview.
  TestUITapGestureRecognizer* subviewRecognizer =
      [[TestUITapGestureRecognizer alloc] initWithTarget:nil action:nil];
  subviewRecognizer.mockedState = UIGestureRecognizerStateRecognized;
  [subview addGestureRecognizer:subviewRecognizer];

  // Mock child's gesture recognizer sender.
  id childSenderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([childSenderMock view]).andReturn(childView);
  // Location (15, 15) in childView lands inside subview (at 10, 10, size 50,
  // 50).
  OCMStub([childSenderMock locationInView:childView])
      .andReturn(CGPointMake(15, 15));

  // Mock parent's gesture recognizer sender.
  id parentSenderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([parentSenderMock view]).andReturn(rootView);
  // Location (25, 25) in rootView lands inside childView (at 10, 10) and
  // subview (at 20, 20 in rootView).
  OCMStub([parentSenderMock locationInView:rootView])
      .andReturn(CGPointMake(25, 25));

  // Pre-conditions: both controllers start with no gesture recorded.
  EXPECT_EQ([view_controller_ currentGestureForTesting], nullptr);
  EXPECT_EQ([childViewController currentGestureForTesting], nullptr);

  // Act: Handle tap gesture on both controllers.
  [childViewController handleTapGesture:childSenderMock];
  [view_controller_ handleTapGesture:parentSenderMock];

  // Verify: Both view controllers successfully record "Tap".
  EXPECT_STREQ([childViewController currentGestureForTesting], "Tap");
  EXPECT_STREQ([view_controller_ currentGestureForTesting], "Tap");
}

// Verifies that when a child view controller is nested, and only the parent
// receives the EventDispatch update phase (reproducing the real product
// behavior), both parent and child view controllers correctly record gesture
// metrics.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       TapGestureOnNestedViewControllerBothRecordMetrics) {
  base::HistogramTester histogram_tester;

  // Set up parent view hierarchy.
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  view_controller_.view = rootView;

  // Set up nested child view controller and view.
  TestUIViewControllerWithDisplayTracing* childViewController =
      [[TestUIViewControllerWithDisplayTracing alloc]
          initWithDisplayTracingOptions:
              UIViewControllerDisplayTracingOptionGesture];
  [view_controller_ addChildViewController:childViewController];
  UIView* childView = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 80, 80)];
  childViewController.view = childView;
  [rootView addSubview:childView];
  [childViewController didMoveToParentViewController:view_controller_];

  // Set up subview inside the child view controller's view.
  UIView* subview = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [childView addSubview:subview];

  // Add a recognized tap gesture recognizer to the leaf subview.
  TestUITapGestureRecognizer* subviewRecognizer =
      [[TestUITapGestureRecognizer alloc] initWithTarget:nil action:nil];
  subviewRecognizer.mockedState = UIGestureRecognizerStateRecognized;
  [subview addGestureRecognizer:subviewRecognizer];

  // Simulate EventDispatch phase ONLY on the parent (as it happens in the
  // product).
  [view_controller_ handleUpdatePhase:8];

  // Mock child's gesture recognizer sender.
  id childSenderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([childSenderMock view]).andReturn(childView);
  OCMStub([childSenderMock locationInView:childView])
      .andReturn(CGPointMake(15, 15));

  // Mock parent's gesture recognizer sender.
  id parentSenderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([parentSenderMock view]).andReturn(rootView);
  OCMStub([parentSenderMock locationInView:rootView])
      .andReturn(CGPointMake(25, 25));

  // Handle tap gestures.
  [childViewController handleTapGesture:childSenderMock];
  [view_controller_ handleTapGesture:parentSenderMock];

  // Simulate CATransactionCommitEnd to trigger UMA recording on both.
  id infoMock = OCMClassMock([UIUpdateInfo class]);
  OCMStub([infoMock estimatedPresentationTime])
      .andReturn([view_controller_ currentMediaTime] + 0.016);

  [childViewController handleCATransactionCommitEndWithLink:nil
                                                       info:infoMock
                                               isLowLatency:NO];
  [view_controller_ handleCATransactionCommitEndWithLink:nil
                                                    info:infoMock
                                            isLowLatency:NO];

  // Verify that both recorded the metrics in the histogram.
  // Since both controllers share the same class name
  // "TestUIViewControllerWithDisplayTracing", they both report to the same
  // histogram name, which should collect 2 samples.
  std::string histogram_name =
      "IOS.InputLatency.TestUIViewControllerWithDisplayTracing.Tap."
      "InputToCommit";
  histogram_tester.ExpectTotalCount(histogram_name, 2);
}

// Verifies that a tap gesture on a subview with an unrecognized tap gesture
// recognizer is disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       TapGestureOnViewWithUnrecognizedTapDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* subview = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:subview];

  TestUITapGestureRecognizer* subviewRecognizer =
      [[TestUITapGestureRecognizer alloc] initWithTarget:nil action:nil];
  subviewRecognizer.mockedState = UIGestureRecognizerStatePossible;
  [subview addGestureRecognizer:subviewRecognizer];

  id senderMock = OCMClassMock([UITapGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  [view_controller_ handleTapGesture:senderMock];

  EXPECT_EQ([view_controller_ currentGestureForTesting], nullptr);
}

// Verifies that a pan gesture on a plain UIView is disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       PanGestureOnPlainViewDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* plainView = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:plainView];

  id senderMock = OCMClassMock([UIPanGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  [view_controller_ handlePanGesture:senderMock];

  EXPECT_EQ([view_controller_ currentGestureForTesting], nullptr);
}

// Verifies that a pan gesture on a subview with a began pan gesture recognizer
// is not disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       PanGestureOnViewWithBeganPanNotDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* subview = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:subview];

  TestUIPanGestureRecognizer* subviewRecognizer =
      [[TestUIPanGestureRecognizer alloc] initWithTarget:nil action:nil];
  subviewRecognizer.mockedState = UIGestureRecognizerStateBegan;
  [subview addGestureRecognizer:subviewRecognizer];

  id senderMock = OCMClassMock([UIPanGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  [view_controller_ handlePanGesture:senderMock];

  EXPECT_STREQ([view_controller_ currentGestureForTesting], "Pan");
}

// Verifies that a pan gesture on a subview with a recognized swipe gesture
// recognizer is not disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       PanGestureOnViewWithRecognizedSwipeNotDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* subview = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:subview];

  TestUISwipeGestureRecognizer* subviewRecognizer =
      [[TestUISwipeGestureRecognizer alloc] initWithTarget:nil action:nil];
  subviewRecognizer.mockedState = UIGestureRecognizerStateRecognized;
  [subview addGestureRecognizer:subviewRecognizer];

  id senderMock = OCMClassMock([UIPanGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  [view_controller_ handlePanGesture:senderMock];

  EXPECT_STREQ([view_controller_ currentGestureForTesting], "Pan");
}

// Verifies that a pan gesture on a subview with an unrecognized pan gesture
// recognizer is disregarded.
TEST_F(UIViewControllerWithDisplayTracingGestureTest,
       PanGestureOnViewWithUnrecognizedPanDisregarded) {
  UIView* rootView = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];
  UIView* subview = [[UIView alloc] initWithFrame:CGRectMake(10, 10, 50, 50)];
  [rootView addSubview:subview];

  TestUIPanGestureRecognizer* subviewRecognizer =
      [[TestUIPanGestureRecognizer alloc] initWithTarget:nil action:nil];
  subviewRecognizer.mockedState = UIGestureRecognizerStatePossible;
  [subview addGestureRecognizer:subviewRecognizer];

  id senderMock = OCMClassMock([UIPanGestureRecognizer class]);
  OCMStub([senderMock view]).andReturn(rootView);
  OCMStub([senderMock locationInView:rootView]).andReturn(CGPointMake(20, 20));

  view_controller_.view = rootView;

  [view_controller_ handlePanGesture:senderMock];

  EXPECT_EQ([view_controller_ currentGestureForTesting], nullptr);
}

}  // namespace
