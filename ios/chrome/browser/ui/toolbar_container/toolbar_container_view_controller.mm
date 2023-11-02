// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_view_controller.h"

#import <vector>

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/toolbar_container/collapsing_toolbar_height_constraint.h"
#import "ios/chrome/browser/ui/toolbar_container/collapsing_toolbar_height_constraint_delegate.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_view.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_height_range.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using toolbar_container::HeightRange;

@interface ToolbarContainerViewController ()<
    CollapsingToolbarHeightConstraintDelegate> {
  // Backing variables for properties of same name.
  HeightRange _heightRange;
  std::vector<CGFloat> _toolbarExpansionStartProgresses;
}

// The constraint managing the height of the container.
@property(nonatomic, strong, readonly) NSLayoutConstraint* heightConstraint;
// The height constraints for the toolbar views.
@property(nonatomic, strong, readonly)
    NSMutableArray<CollapsingToolbarHeightConstraint*>*
        toolbarHeightConstraints;
// The fullscreen progresses at which the toolbars begin expanding.  As the
// fullscreen progress goes from 0.0 to 1.0, the toolbars are expanded in the
// reverse of order of self.toolbars so that the toolbar closest to the page
// content is expanded first for scroll events.  Each toolbar is expanded for a
// portion of the [0.0, 1.0] progress range proportional to its height delta
// relative to the overall height delta of the toolbar stack.  This creates the
// effect of the overall stack height adjusting linearly while each individual
// toolbar's height is adjusted sequentially.
@property(nonatomic, assign, readonly)
    std::vector<CGFloat>& toolbarExpansionStartProgresses;
// Returns the height constraint for the first toolbar in self.toolbars.
@property(nonatomic, readonly)
    CollapsingToolbarHeightConstraint* firstToolbarHeightConstraint;
// Additional height to be added to the first toolbar in the stack.
@property(nonatomic, assign) CGFloat additionalStackHeight;
@end

@implementation ToolbarContainerViewController
@synthesize orientation = _orientation;
@synthesize collapsesSafeArea = _collapsesSafeArea;
@synthesize toolbars = _toolbars;
@synthesize heightConstraint = _heightConstraint;
@synthesize toolbarHeightConstraints = _toolbarHeightConstraints;
@synthesize additionalStackHeight = _additionalStackHeight;

#pragma mark - Accessors

- (const HeightRange&)heightRange {
  // Custom getter is needed to support the C++ reference type.
  return _heightRange;
}

- (std::vector<CGFloat>&)toolbarExpansionStartProgresses {
  // Custom getter is needed to support the C++ reference type.
  return _toolbarExpansionStartProgresses;
}

- (CollapsingToolbarHeightConstraint*)firstToolbarHeightConstraint {
  if (!self.viewLoaded || !self.toolbars.count)
    return nil;
  DCHECK_EQ(self.toolbarHeightConstraints.count, self.toolbars.count);
  DCHECK_EQ(self.toolbarHeightConstraints[0].firstItem, self.toolbars[0].view);
  return self.toolbarHeightConstraints[0];
}

- (void)setAdditionalStackHeight:(CGFloat)additionalStackHeight {
  DCHECK_GE(additionalStackHeight, 0.0);
  if (AreCGFloatsEqual(_additionalStackHeight, additionalStackHeight))
    return;
  _additionalStackHeight = additionalStackHeight;
  self.firstToolbarHeightConstraint.additionalHeight = _additionalStackHeight;
}

#pragma mark - CollapsingToolbarHeightConstraintDelegate

- (void)collapsingHeightConstraint:
            (CollapsingToolbarHeightConstraint*)constraint
          didUpdateFromHeightRange:
              (const toolbar_container::HeightRange&)oldHeightRange {
  [self updateHeightRangeWithRange:self.heightRange + constraint.heightRange -
                                   oldHeightRange];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  // No changes are needed if there are no collapsing toolbars.
  CGFloat stackHeightDelta = self.heightRange.delta();
  if (!self.viewLoaded || AreCGFloatsEqual(stackHeightDelta, 0.0) ||
      !self.toolbars.count) {
    return;
  }

  for (NSUInteger i = 0; i < self.toolbars.count; ++i) {
    CollapsingToolbarHeightConstraint* constraint =
        self.toolbarHeightConstraints[i];
    // Calculate the progress range for the toolbar.  `startProgress` is pre-
    // calculated and stored in self.toolbarExpansionStartProgresses.  The end
    // progress is calculated by adding the proportion of the overall stack
    // height delta created by this toolbar.
    CGFloat startProgress = self.toolbarExpansionStartProgresses[i];
    CGFloat endProgress =
        startProgress + constraint.heightRange.delta() / stackHeightDelta;
    // CollapsingToolbarHeightConstraint clamps its progress value between 0.0
    // and 1.0, so `constraint`'s progress value will be set:
    // -  0.0 when `progress` <= `startProgress`,
    // -  1.0 when `progress` >= `endProgress`, and
    // -  scaled linearly from 0.0 to 1.0 for `progress` values within that
    //    range.
    constraint.progress =
        (progress - startProgress) / (endProgress - startProgress);
  }
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled)
    [self updateForFullscreenProgress:1.0];
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  __weak ToolbarContainerViewController* weakSelf = self;
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^{
    [weakSelf updateForFullscreenProgress:finalProgress];
    [[weakSelf view] setNeedsLayout];
    [[weakSelf view] layoutIfNeeded];
  }];
}

#pragma mark - ToolbarContainerConsumer

- (void)setOrientation:(ToolbarContainerOrientation)orientation {
  if (_orientation == orientation)
    return;
  _orientation = orientation;
  [self setUpToolbarStack];
}

- (void)setCollapsesSafeArea:(BOOL)collapsesSafeArea {
  if (_collapsesSafeArea == collapsesSafeArea)
    return;
  _collapsesSafeArea = collapsesSafeArea;
  self.firstToolbarHeightConstraint.collapsesAdditionalHeight =
      _collapsesSafeArea;
}

- (void)setToolbars:(NSArray<UIViewController*>*)toolbars {
  if ([_toolbars isEqualToArray:toolbars])
    return;
  [self removeToolbars];
  _toolbars = toolbars;
  self.toolbarExpansionStartProgresses.resize(_toolbars.count);
  [self setUpToolbarStack];
}

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[ToolbarContainerView alloc] init];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  _heightConstraint = [self.view.heightAnchor constraintEqualToConstant:0.0];
  _heightConstraint.active = YES;
  [self setUpToolbarStack];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if (!self.toolbarHeightConstraints.count)
    [self setUpToolbarStack];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self removeToolbars];
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self updateForSafeArea];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateForSafeArea];
}

#pragma mark - Layout Helpers

// Sets up the stack of toolbars.
- (void)setUpToolbarStack {
  if (!self.viewLoaded)
    return;
  [self removeToolbars];
  for (NSUInteger i = 0; i < self.toolbars.count; ++i) {
    [self addToolbarAtIndex:i];
  }
  [self createToolbarHeightConstraints];
  [self updateForSafeArea];
  [self calculateToolbarExpansionStartProgresses];
}

// Removes all the toolbars from the view.
- (void)removeToolbars {
  for (UIViewController* toolbar in self.toolbars) {
    [toolbar willMoveToParentViewController:nil];
    [toolbar.view removeFromSuperview];
    [toolbar removeFromParentViewController];
  }
  [self resetToolbarHeightConstraints];
}

// Adds the toolbar at `index` to the view.
- (void)addToolbarAtIndex:(NSUInteger)index {
  DCHECK_LT(index, self.toolbars.count);
  UIViewController* toolbar = self.toolbars[index];
  DCHECK(!toolbar.parentViewController);

  // Add the toolbar and its view controller.
  UIView* toolbarView = toolbar.view;
  [self addChildViewController:toolbar];
  [self.view addSubview:toolbar.view];
  toolbarView.translatesAutoresizingMaskIntoConstraints = NO;
  [toolbar didMoveToParentViewController:self];

  // The toolbars will always be the full width of the container.
  AddSameConstraintsToSides(self.view, toolbarView,
                            LayoutSides::kLeading | LayoutSides::kTrailing);

  // Calculate the positioning constraint.
  BOOL topToBottom =
      self.orientation == ToolbarContainerOrientation::kTopToBottom;
  NSLayoutAnchor* toolbarPositioningAnchor =
      topToBottom ? toolbarView.topAnchor : toolbarView.bottomAnchor;
  NSLayoutAnchor* positioningAnchor = nil;
  if (index > 0) {
    NSUInteger previousIndex = index - 1;
    UIViewController* previousToolbar = self.toolbars[previousIndex];
    DCHECK_EQ(previousToolbar.parentViewController, self);
    UIView* previousToolbarView = previousToolbar.view;
    positioningAnchor = topToBottom ? previousToolbarView.bottomAnchor
                                    : previousToolbarView.topAnchor;
  } else {
    positioningAnchor =
        topToBottom ? self.view.topAnchor : self.view.bottomAnchor;
  }
  [toolbarPositioningAnchor constraintEqualToAnchor:positioningAnchor].active =
      YES;
}

// Deactivates the toolbar height constraints and resets the property.
- (void)resetToolbarHeightConstraints {
  if (_toolbarHeightConstraints.count) {
    [NSLayoutConstraint deactivateConstraints:_toolbarHeightConstraints];
    for (CollapsingToolbarHeightConstraint* constraint in
             _toolbarHeightConstraints) {
      constraint.delegate = nil;
    }
  }
  _toolbarHeightConstraints = nil;
  _heightRange = HeightRange();
}

// Creates and activates height constriants for the toolbars and adds them to
// self.toolbarHeightConstraints at the same index of their corresponding
// toolbar view controller.
- (void)createToolbarHeightConstraints {
  [self resetToolbarHeightConstraints];
  _toolbarHeightConstraints = [NSMutableArray array];
  HeightRange heightRange;
  for (NSUInteger i = 0; i < self.toolbars.count; ++i) {
    UIView* toolbarView = self.toolbars[i].view;
    CollapsingToolbarHeightConstraint* heightConstraint =
        [CollapsingToolbarHeightConstraint constraintWithView:toolbarView];
    heightConstraint.active = YES;
    // Set up the additional height for the first toolbar.
    if (!self.toolbarHeightConstraints.count) {
      heightConstraint.additionalHeight = self.additionalStackHeight;
      heightConstraint.collapsesAdditionalHeight = self.collapsesSafeArea;
    }
    // Set as delegate to receive notifications of height range updates.
    heightConstraint.delegate = self;
    // Add the height range values.
    heightRange += heightConstraint.heightRange;
    [_toolbarHeightConstraints addObject:heightConstraint];
  }
  [self updateHeightRangeWithRange:heightRange];
}

// Updates the height range of the stack with `range`.
- (void)updateHeightRangeWithRange:(const HeightRange&)range {
  if (_heightRange == range)
    return;
  BOOL maxHeightUpdated =
      !AreCGFloatsEqual(_heightRange.max_height(), range.max_height());
  BOOL deltaUpdated = !AreCGFloatsEqual(_heightRange.delta(), range.delta());
  _heightRange = range;
  if (maxHeightUpdated)
    self.heightConstraint.constant = _heightRange.max_height();
  if (deltaUpdated)
    [self calculateToolbarExpansionStartProgresses];
}

// Calculates the fullscreen progress values at which the toolbars should start
// expanding.  See comments for self.toolbarExpansionStartProgresses for more
// details.
- (void)calculateToolbarExpansionStartProgresses {
  DCHECK_EQ(self.toolbarExpansionStartProgresses.size(), self.toolbars.count);
  if (!self.toolbars.count)
    return;
  CGFloat startProgress = 0.0;
  for (NSUInteger i = self.toolbars.count - 1; i > 0; --i) {
    self.toolbarExpansionStartProgresses[i] = startProgress;
    CGFloat delta = self.heightRange.delta();
    if (delta > 0.0) {
      startProgress +=
          self.toolbarHeightConstraints[i].heightRange.delta() / delta;
    }
  }
  self.toolbarExpansionStartProgresses[0] = startProgress;
}

// Adds additional height to the first toolbar to account for the safe area.
- (void)updateForSafeArea {
  if (self.orientation == ToolbarContainerOrientation::kTopToBottom) {
    self.additionalStackHeight = self.view.safeAreaInsets.top;
  } else {
    self.additionalStackHeight = self.view.safeAreaInsets.bottom;
  }
}

@end
