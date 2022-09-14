// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_view_controller.h"

#import <algorithm>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_collapsing.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_height_range.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using toolbar_container::HeightRange;

namespace {
// The container view width.
const CGFloat kContainerViewWidth = 300.0;
// The number of toolbars to add.
const size_t kToolbarCount = 2;
// The collapsed and expanded heights of the toolbars.
const CGFloat kCollapsedToolbarHeight = 50.0;
const CGFloat kExpandedToolbarHeight = 150.0;
// The non-collapsing toolbar height.
const CGFloat kNonCollapsingToolbarHeight = 75.0;
// The inset into the stack for the safe area.
const CGFloat kSafeAreaStackInset = 100.0;
// The progress values to check.
const CGFloat kStackProgressValues[] = {0.0, 0.25, 0.5, 0.75, 1.0};
// Parameters used for the test fixtures.
typedef NS_ENUM(NSUInteger, ToolbarContainerTestConfig) {
  kEmptyConfig = 0,
  kTopToBottom = 1 << 0,
  kCollapsingToolbars = 1 << 1,
  kCollapsingSafeInset = 1 << 2,
  kToolbarContainerConfigMax = 1 << 3,
};
// Returns a string version of `frame` to use for error printing.
std::string GetFrameString(CGRect frame) {
  return base::SysNSStringToUTF8(NSStringFromCGRect(frame));
}
}  // namespace

// Test toolbar view.
@interface TestToolbarView : UIView<ToolbarCollapsing> {
  HeightRange _heightRange;
}
- (instancetype)initWithHeightRange:(const HeightRange&)heightRange;
@end

@implementation TestToolbarView
- (instancetype)initWithHeightRange:(const HeightRange&)heightRange {
  if (self = [super init])
    _heightRange = heightRange;
  return self;
}
- (CGFloat)expandedToolbarHeight {
  return _heightRange.max_height();
}
- (CGFloat)collapsedToolbarHeight {
  return _heightRange.min_height();
}
@end

// Test toolbar view controller.
@interface TestToolbarViewController : UIViewController {
  HeightRange _heightRange;
}
@property(nonatomic, strong, readonly) TestToolbarView* toolbarView;
- (instancetype)initWithHeightRange:(const HeightRange&)heightRange;
@end

@implementation TestToolbarViewController
- (instancetype)initWithHeightRange:(const HeightRange&)heightRange {
  if (self = [super init])
    _heightRange = heightRange;
  return self;
}
- (void)loadView {
  self.view = [[TestToolbarView alloc] initWithHeightRange:_heightRange];
}
- (TestToolbarView*)toolbarView {
  return static_cast<TestToolbarView*>(self.view);
}
@end

// Test fixture for ToolbarContainerViewController.
class ToolbarContainerViewControllerTest
    : public ::testing::TestWithParam<ToolbarContainerTestConfig> {
 public:
  ToolbarContainerViewControllerTest()
      : window_([[UIWindow alloc] init]),
        view_controller_([[ToolbarContainerViewController alloc] init]) {
    // Resize the window and add the container view such that it hugs the
    // leading/trailing edges.
    window_.frame = CGRectMake(0.0, 0.0, kContainerViewWidth, 1000);
    [window_ addSubview:container_view()];
    AddSameConstraintsToSides(window_, container_view(),
                              LayoutSides::kLeading | LayoutSides::kTrailing);
    UpdateForOrientationConfig();
    UpdateForToolbarCollapsingConfig();
    UpdateForSafeInsetCollapsingConfig();
    ForceLayout();
  }

  ~ToolbarContainerViewControllerTest() override {
    view_controller_.toolbars = nil;
  }

  // Convenience getters for each of the config option flags.
  bool IsTopToBottom() { return (GetParam() & kTopToBottom) == kTopToBottom; }
  bool HasCollapsingToolbars() {
    return (GetParam() & kCollapsingToolbars) == kCollapsingToolbars;
  }
  bool HasCollapsingSafeInset() {
    return (GetParam() & kCollapsingSafeInset) == kCollapsingSafeInset;
  }

  // Sets the orientation of the container and constraints its view to the top
  // or bottom of the window.
  void UpdateForOrientationConfig() {
    view_controller_.orientation =
        IsTopToBottom() ? ToolbarContainerOrientation::kTopToBottom
                        : ToolbarContainerOrientation::kBottomToTop;
    UIEdgeInsets safe_insets = container_view().safeAreaInsets;
    if (IsTopToBottom())
      safe_insets.top = kSafeAreaStackInset - safe_insets.top;
    else
      safe_insets.bottom = kSafeAreaStackInset - safe_insets.bottom;
    view_controller_.additionalSafeAreaInsets = safe_insets;
  }

  // Adds collapsible or non-collapsible toolbars to the container, depending on
  // the config flag.
  void UpdateForToolbarCollapsingConfig() {
    // Calculate the height range for the toolbars.
    HeightRange toolbar_height_range;
    if (HasCollapsingToolbars()) {
      toolbar_height_range =
          HeightRange(kCollapsedToolbarHeight, kExpandedToolbarHeight);
    } else {
      toolbar_height_range =
          HeightRange(kNonCollapsingToolbarHeight, kNonCollapsingToolbarHeight);
    }
    // Add kToolbarCount toolbars with `toolbar_height_range`.
    height_ranges_ =
        std::vector<HeightRange>(kToolbarCount, toolbar_height_range);
    NSMutableArray* toolbars = [NSMutableArray array];
    for (const HeightRange& height_range : height_ranges_) {
      TestToolbarViewController* toolbar =
          [[TestToolbarViewController alloc] initWithHeightRange:height_range];
      [toolbars addObject:toolbar];
    }
    view_controller_.toolbars = toolbars;
  }

  // Updates the view controller safe inset collapsing behavior based on the
  // config.
  void UpdateForSafeInsetCollapsingConfig() {
    view_controller_.collapsesSafeArea = HasCollapsingSafeInset();
  }

  // Returns the total height range for the toolbar view at `index`, accounting
  // for both the toolbar expansion and the collapsing safe area inset.
  HeightRange GetTotalToolbarHeightRange(NSUInteger index) {
    HeightRange height_range = height_ranges_[index];
    if (index == 0) {
      bool collapses_safe_area = view_controller_.collapsesSafeArea;
      HeightRange safe_area_height_range = HeightRange(
          collapses_safe_area ? 0.0 : kSafeAreaStackInset, kSafeAreaStackInset);
      height_range += safe_area_height_range;
    }
    return height_range;
  }

  // Returns the expected height of the toolbar stack.
  CGFloat GetExpectedStackHeight() {
    CGFloat expected_stack_height = 0.0;
    for (NSUInteger index = 0; index < kToolbarCount; ++index) {
      expected_stack_height += GetTotalToolbarHeightRange(index).max_height();
    }
    return expected_stack_height;
  }

  // Set the stack progress.
  void SetStackProgress(CGFloat progress) {
    stack_progress_ = progress;
    [view_controller_ updateForFullscreenProgress:progress];
    ForceLayout();
  }

  // Forces a layout of the hierarchy.
  void ForceLayout() {
    [window_ setNeedsLayout];
    [window_ layoutIfNeeded];
    [container_view() setNeedsLayout];
    [container_view() layoutIfNeeded];
    stack_height_delta_ = 0.0;
    for (NSUInteger index = 0; index < kToolbarCount; ++index) {
      stack_height_delta_ += GetTotalToolbarHeightRange(index).delta();
    }
  }

  // Returns the toolbar view at `index`.
  TestToolbarView* GetToolbarView(NSUInteger index) {
    return static_cast<TestToolbarViewController*>(
               view_controller_.toolbars[index])
        .toolbarView;
  }

  // Returns the progress value for the toolbar at `index` for the current stack
  // progress.
  CGFloat GetToolbarProgress(NSUInteger index) {
    // Calculate the start progress.
    CGFloat start_progress = 0.0;
    for (NSUInteger i = kToolbarCount - 1; i > index; --i) {
      if (stack_height_delta_ > 0.0) {
        start_progress +=
            GetTotalToolbarHeightRange(i).delta() / stack_height_delta_;
      }
    }
    // Get the individual toolbar progress.
    HeightRange height_range = GetTotalToolbarHeightRange(index);
    CGFloat end_progress =
        start_progress + height_range.delta() / stack_height_delta_;
    CGFloat progress =
        (stack_progress_ - start_progress) / (end_progress - start_progress);
    progress = std::min(static_cast<CGFloat>(1.0), progress);
    progress = std::max(static_cast<CGFloat>(0.0), progress);
    return progress;
  }

  // Returns the expected frame for the toolbar at `index` at the current stack
  // progress.
  CGRect GetExpectedToolbarFrame(NSUInteger index) {
    const HeightRange& height_range = GetTotalToolbarHeightRange(index);
    CGSize size = CGSizeMake(
        kContainerViewWidth,
        height_range.GetInterpolatedHeight(GetToolbarProgress(index)));
    CGFloat origin_y = 0.0;
    bool is_first_toolbar = index == 0;
    if (IsTopToBottom()) {
      origin_y = is_first_toolbar
                     ? 0.0
                     : CGRectGetMaxY(GetExpectedToolbarFrame(index - 1));
    } else {
      CGFloat bottom_edge =
          is_first_toolbar ? CGRectGetMaxY(container_view().bounds)
                           : CGRectGetMinY(GetExpectedToolbarFrame(index - 1));
      origin_y = bottom_edge - size.height;
    }
    return CGRectMake(0.0, origin_y, size.width, size.height);
  }

  // Checks that the frames of the toolbar views are expected for the current
  // stack progress.
  void CheckToolbarFrames() {
    for (NSUInteger index = 0; index < kToolbarCount; ++index) {
      CGRect toolbar_frame = GetToolbarView(index).frame;
      CGRect expected_toolbar_frame = GetExpectedToolbarFrame(index);
      EXPECT_TRUE(CGRectEqualToRect(toolbar_frame, expected_toolbar_frame)) <<
          "IsTopToBottom          : " << IsTopToBottom() << "\n"
          "HasCollapsingToolbars  : " << HasCollapsingToolbars() << "\n"
          "HasCollapsingSafeInset : " << HasCollapsingSafeInset() << "\n"
          "Stack Progress         : " << stack_progress_ << "\n"
          "Toolbar Index          : " << index << "\n"
          "toolbar_frame          : " << GetFrameString(toolbar_frame) << "\n"
          "expected_toolbar_frame : " << GetFrameString(expected_toolbar_frame);
    }
  }

  // The view.
  UIView* container_view() { return view_controller_.view; }

 private:
  __strong UIWindow* window_ = nil;
  __strong ToolbarContainerViewController* view_controller_ = nil;
  std::vector<HeightRange> height_ranges_;
  CGFloat stack_progress_ = 1.0;
  CGFloat stack_height_delta_ = 0.0;
};

// Tests the layout of the toolbar stack configured using the
// ToolbarContainerTestConfig test fixture parameter.
TEST_P(ToolbarContainerViewControllerTest, VerifyStackLayoutForProgresses) {
  // Check that the container height is as expected.
  EXPECT_EQ(CGRectGetHeight(container_view().bounds), GetExpectedStackHeight());
  // Set the stack progress to the progress values in kStackProgressValues and
  // verify the toolbar frames for each of these stack progress values.
  for (size_t index = 0; index < std::size(kStackProgressValues); ++index) {
    SetStackProgress(kStackProgressValues[index]);
    CheckToolbarFrames();
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ToolbarContainerViewControllerTest,
                         ::testing::Range(kEmptyConfig,
                                          kToolbarContainerConfigMax));
