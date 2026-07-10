// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Snapping states for the bottom sheet.
typedef NS_ENUM(NSInteger, BottomSheetSnappingState) {
  BottomSheetSnappingStateCollapsed,
  BottomSheetSnappingStateResting,
  BottomSheetSnappingStateExpanded,
};

// Spacing/margin constants for content container.
constexpr CGFloat kContentContainerTopMargin = 16.0;

// Minimum velocity needed for a user drag to trigger bottom sheet state change.
constexpr CGFloat kMinimumDragVelocityToChangeState = 500;
}  // namespace

@interface NewTabPageBottomSheetViewController () <UIGestureRecognizerDelegate>
@property(nonatomic, strong) NSLayoutConstraint* bottomSheetTopConstraint;
@end

@implementation NewTabPageBottomSheetViewController {
  UIView* _dragHandle;
  UIView* _contentContainerView;
  NSLayoutConstraint* _contentContainerTopConstraint;
  BottomSheetSnappingState _sheetState;

  CGSize _lastSize;
  CGFloat _initialConstant;
}

- (void)loadView {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  self.view = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  _sheetState = BottomSheetSnappingStateResting;

  self.view.layer.cornerRadius = 24.0;
  self.view.layer.masksToBounds = YES;

  UIVisualEffectView* visualEffectView = (UIVisualEffectView*)self.view;

  // Add drag handle to bottom sheet.
  _dragHandle = [[UIView alloc] init];
  _dragHandle.translatesAutoresizingMaskIntoConstraints = NO;
  _dragHandle.backgroundColor = [UIColor colorWithWhite:0.5 alpha:0.3];
  _dragHandle.layer.cornerRadius = 2.5;
  [visualEffectView.contentView addSubview:_dragHandle];

  [NSLayoutConstraint activateConstraints:@[
    [_dragHandle.centerXAnchor
        constraintEqualToAnchor:visualEffectView.contentView.centerXAnchor],
    [_dragHandle.topAnchor
        constraintEqualToAnchor:visualEffectView.contentView.topAnchor
                       constant:8],
    [_dragHandle.widthAnchor constraintEqualToConstant:36],
    [_dragHandle.heightAnchor constraintEqualToConstant:5],
  ]];

  // Add content container view.
  _contentContainerView = [[UIView alloc] init];
  _contentContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [visualEffectView.contentView addSubview:_contentContainerView];

  _contentContainerTopConstraint = [_contentContainerView.topAnchor
      constraintEqualToAnchor:_dragHandle.bottomAnchor
                     constant:kContentContainerTopMargin];
  _contentContainerTopConstraint.active = YES;

  [NSLayoutConstraint activateConstraints:@[
    [_contentContainerView.leadingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.leadingAnchor],
    [_contentContainerView.trailingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.trailingAnchor],
    [_contentContainerView.bottomAnchor
        constraintEqualToAnchor:visualEffectView.contentView.bottomAnchor],
  ]];

  // Add pan gesture recognizer.
  UIPanGestureRecognizer* panGesture =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  panGesture.delegate = self;
  [self.view addGestureRecognizer:panGesture];

  if (_feedViewController) {
    [self embedFeedViewController];
  }
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) {
    [self setupSuperviewConstraints];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (self.view.superview &&
      !CGSizeEqualToSize(_lastSize, self.view.superview.bounds.size)) {
    _lastSize = self.view.superview.bounds.size;
    [self updateBottomSheetPositionAnimated:NO];
  }
}

- (void)invalidate {
  self.delegate = nil;
  self.feedViewController = nil;
}

#pragma mark - Action Targets

#pragma mark - Feed Integration

- (void)setFeedViewController:(UIViewController*)feedViewController {
  if (_feedViewController == feedViewController) {
    return;
  }
  if (_feedViewController) {
    [_feedViewController willMoveToParentViewController:nil];
    [_feedViewController.view removeFromSuperview];
    [_feedViewController removeFromParentViewController];
  }
  _feedViewController = feedViewController;
  if (self.isViewLoaded && _feedViewController) {
    [self embedFeedViewController];
  }
}

- (void)embedFeedViewController {
  if (!_feedViewController || !_contentContainerView) {
    return;
  }
  if (_feedViewController.parentViewController == self) {
    return;
  }
  if (_feedViewController.parentViewController) {
    [_feedViewController willMoveToParentViewController:nil];
    [_feedViewController.view removeFromSuperview];
    [_feedViewController removeFromParentViewController];
  }
  [self addChildViewController:_feedViewController];
  _feedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [_contentContainerView addSubview:_feedViewController.view];
  AddSameConstraints(_feedViewController.view, _contentContainerView);
  [_feedViewController didMoveToParentViewController:self];

  _feedViewController.view.hidden = NO;
}

#pragma mark - Snapping Offsets

- (CGFloat)collapsedOffset {
  UIView* superview = self.view.superview;
  return superview ? superview.bounds.size.height * 0.75 : 0;
}

- (CGFloat)restingOffset {
  UIView* superview = self.view.superview;
  return superview ? superview.bounds.size.height * 0.60 : 0;
}

- (CGFloat)expandedOffset {
  UIView* superview = self.view.superview;
  return superview ? 20.0 + superview.safeAreaInsets.top : 0;
}

- (CGFloat)targetOffsetForState:(BottomSheetSnappingState)state {
  switch (state) {
    case BottomSheetSnappingStateCollapsed:
      return [self collapsedOffset];
    case BottomSheetSnappingStateResting:
      return [self restingOffset];
    case BottomSheetSnappingStateExpanded:
      return [self expandedOffset];
  }
}

#pragma mark - Bottom Sheet Snapping and Panning

- (void)setupSuperviewConstraints {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor constraintEqualToAnchor:superview.leadingAnchor],
    [self.view.trailingAnchor constraintEqualToAnchor:superview.trailingAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:superview.bottomAnchor],
  ]];

  if (!_bottomSheetTopConstraint) {
    _bottomSheetTopConstraint =
        [self.view.topAnchor constraintEqualToAnchor:superview.topAnchor
                                            constant:[self restingOffset]];
    _bottomSheetTopConstraint.active = YES;
  }
}

- (void)updateBottomSheetPositionAnimated:(BOOL)animated {
  if (!_bottomSheetTopConstraint) {
    return;
  }
  CGFloat targetConstant = [self targetOffsetForState:_sheetState];

  if (!animated) {
    _bottomSheetTopConstraint.constant = targetConstant;
    [self updateContentContainerInsetForOffset:targetConstant];
    [self.delegate bottomSheetViewController:self
                          didUpdateTopOffset:targetConstant];
  } else {
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.3
                          delay:0
         usingSpringWithDamping:0.85
          initialSpringVelocity:0.5
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       NewTabPageBottomSheetViewController* strongSelf =
                           weakSelf;
                       if (!strongSelf) {
                         return;
                       }
                       strongSelf.bottomSheetTopConstraint.constant =
                           targetConstant;
                       [strongSelf
                           updateContentContainerInsetForOffset:targetConstant];
                       [strongSelf.delegate
                           bottomSheetViewController:strongSelf
                                  didUpdateTopOffset:targetConstant];
                       [strongSelf.view.superview layoutIfNeeded];
                     }
                     completion:nil];
  }
}

- (void)snapSheetWithVelocity:(CGPoint)velocity
              currentConstant:(CGFloat)currentConstant {
  CGFloat collapsed = [self collapsedOffset];
  CGFloat resting = [self restingOffset];
  CGFloat expanded = [self expandedOffset];

  BottomSheetSnappingState targetState = _sheetState;

  if (velocity.y > kMinimumDragVelocityToChangeState) {
    // Dragged down quickly
    if (_sheetState == BottomSheetSnappingStateExpanded) {
      targetState = BottomSheetSnappingStateResting;
    } else if (_sheetState == BottomSheetSnappingStateResting) {
      targetState = BottomSheetSnappingStateCollapsed;
    }
  } else if (velocity.y < -kMinimumDragVelocityToChangeState) {
    // Dragged up quickly
    if (_sheetState == BottomSheetSnappingStateCollapsed) {
      targetState = BottomSheetSnappingStateResting;
    } else if (_sheetState == BottomSheetSnappingStateResting) {
      targetState = BottomSheetSnappingStateExpanded;
    }
  } else {
    // Slow drag - snap to closest based on distance midpoints
    CGFloat mid1 = (expanded + resting) / 2.0;
    CGFloat mid2 = (resting + collapsed) / 2.0;

    if (currentConstant < mid1) {
      targetState = BottomSheetSnappingStateExpanded;
    } else if (currentConstant >= mid1 && currentConstant < mid2) {
      targetState = BottomSheetSnappingStateResting;
    } else {
      targetState = BottomSheetSnappingStateCollapsed;
    }
  }

  _sheetState = targetState;
  [self updateBottomSheetPositionAnimated:YES];
}

- (void)updateContentContainerInsetForOffset:(CGFloat)topOffset {
  CGFloat expanded = [self expandedOffset];
  CGFloat resting = [self restingOffset];
  if (resting <= expanded) {
    _contentContainerTopConstraint.constant = kContentContainerTopMargin;
    return;
  }
  CGFloat progress = (topOffset - expanded) / (resting - expanded);
  progress = MIN(1.0, MAX(0.0, progress));

  // When expanded (progress = 0.0), inset is 88.0.
  // When resting/collapsed (progress = 1.0), inset is 16.0.
  CGFloat extraPadding = 72.0;  // omniboxHeight (56) + spacing (16)
  _contentContainerTopConstraint.constant =
      kContentContainerTopMargin + (1.0 - progress) * extraPadding;
}

- (void)handlePan:(UIPanGestureRecognizer*)gesture {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }
  CGPoint translation = [gesture translationInView:superview];
  CGPoint velocity = [gesture velocityInView:superview];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    _initialConstant = _bottomSheetTopConstraint.constant;
  }

  CGFloat newConstant = _initialConstant + translation.y;
  CGFloat minOffset = [self expandedOffset];
  CGFloat maxOffset = [self collapsedOffset];

  if (newConstant < minOffset) {
    newConstant = minOffset;
  } else if (newConstant > maxOffset) {
    newConstant = maxOffset;
  }

  _bottomSheetTopConstraint.constant = newConstant;
  [self updateContentContainerInsetForOffset:newConstant];
  [self.delegate bottomSheetViewController:self didUpdateTopOffset:newConstant];

  if (gesture.state == UIGestureRecognizerStateEnded) {
    [self snapSheetWithVelocity:velocity currentConstant:newConstant];
  }
}

- (void)feedScrollViewDidScroll:(UIScrollView*)scrollView {
  if (_sheetState == BottomSheetSnappingStateExpanded &&
      scrollView.contentOffset.y < 0 && scrollView.dragging) {
    CGFloat delta = -scrollView.contentOffset.y;
    // Lock contentOffset to top.
    scrollView.contentOffset = CGPointZero;

    CGFloat newConstant = _bottomSheetTopConstraint.constant + delta;
    CGFloat maxOffset = [self collapsedOffset];
    if (newConstant > maxOffset) {
      newConstant = maxOffset;
    }
    _bottomSheetTopConstraint.constant = newConstant;
    [self.delegate bottomSheetViewController:self
                          didUpdateTopOffset:newConstant];
  }
}

- (void)feedScrollViewDidEndDragging:(UIScrollView*)scrollView
                      willDecelerate:(BOOL)decelerate {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }
  CGFloat currentConstant = _bottomSheetTopConstraint.constant;
  if (currentConstant > [self expandedOffset]) {
    CGPoint velocity =
        [scrollView.panGestureRecognizer velocityInView:superview];
    [self snapSheetWithVelocity:velocity currentConstant:currentConstant];
  }
}

@end
