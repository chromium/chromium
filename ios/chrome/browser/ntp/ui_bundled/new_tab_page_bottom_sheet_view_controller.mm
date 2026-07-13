// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import <cmath>

#import "ios/chrome/browser/ntp/ui_bundled/scroll_delegate_proxy.h"
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

// Minimum drag velocity required to trigger a state transition.
constexpr CGFloat kMinimumDragVelocityToChangeState = 250.0;

}  // namespace

@interface NewTabPageBottomSheetViewController () <UIGestureRecognizerDelegate,
                                                   UIScrollViewDelegate>
@property(nonatomic, strong) NSLayoutConstraint* bottomSheetTopConstraint;
@end

@implementation NewTabPageBottomSheetViewController {
  UIView* _dragHandle;
  UIView* _contentContainerView;
  NSLayoutConstraint* _contentContainerTopConstraint;
  BottomSheetSnappingState _sheetState;

  CGSize _lastSize;
  CGFloat _initialConstant;

  UIPanGestureRecognizer* _sheetPanGesture;
  __weak UIScrollView* _feedScrollView;
  ScrollDelegateProxy* _scrollProxy;
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
  _sheetPanGesture =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  _sheetPanGesture.delegate = self;
  [self.view addGestureRecognizer:_sheetPanGesture];

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
  if (_feedScrollView && _scrollProxy) {
    _feedScrollView.delegate = _scrollProxy.originalTarget;
  }
  _scrollProxy = nil;
  self.delegate = nil;
  self.feedViewController = nil;
}

#pragma mark - Action Targets

#pragma mark - Feed Integration

- (UIScrollView*)findScrollViewInView:(UIView*)view {
  if ([view isKindOfClass:[UIScrollView class]]) {
    return (UIScrollView*)view;
  }
  for (UIView* subview in view.subviews) {
    UIScrollView* scrollView = [self findScrollViewInView:subview];
    if (scrollView) {
      return scrollView;
    }
  }
  return nil;
}

- (void)updateFeedScrollViewReference {
  if (_feedScrollView && _scrollProxy) {
    _feedScrollView.delegate = _scrollProxy.originalTarget;
  }
  _scrollProxy = nil;

  if (_feedViewController) {
    _feedScrollView = [self findScrollViewInView:_feedViewController.view];
    if (_feedScrollView) {
      _feedScrollView.scrollEnabled =
          (_sheetState == BottomSheetSnappingStateExpanded);
      id originalDelegate = _feedScrollView.delegate;
      if (originalDelegate != self &&
          ![originalDelegate isKindOfClass:[ScrollDelegateProxy class]]) {
        _scrollProxy = [[ScrollDelegateProxy alloc]
            initWithInterceptingTarget:self
                        originalTarget:originalDelegate];
        _feedScrollView.delegate = _scrollProxy;
      }
    }
  } else {
    _feedScrollView = nil;
  }
}

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
  [self updateFeedScrollViewReference];
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
  [self updateFeedScrollViewReference];
}

#pragma mark - Snapping Offsets

- (CGFloat)collapsedOffset {
  return [self.delegate collapsedOffsetForBottomSheetViewController:self];
}

- (CGFloat)restingOffset {
  return [self.delegate restingOffsetForBottomSheetViewController:self];
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

  if (_feedScrollView) {
    _feedScrollView.scrollEnabled =
        (_sheetState == BottomSheetSnappingStateExpanded);
  }

  if (_sheetState != BottomSheetSnappingStateExpanded && _feedScrollView) {
    [_feedScrollView setContentOffset:CGPointZero animated:animated];
  }

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

  if (std::abs(velocity.y) > kMinimumDragVelocityToChangeState) {
    if (velocity.y > 0) {
      // Swiping down: transition to the next lower state.
      if (_sheetState == BottomSheetSnappingStateExpanded) {
        targetState = BottomSheetSnappingStateResting;
      } else if (_sheetState == BottomSheetSnappingStateResting) {
        targetState = BottomSheetSnappingStateCollapsed;
      }
    } else {
      // Swiping up: transition to the next higher state.
      if (_sheetState == BottomSheetSnappingStateCollapsed) {
        targetState = BottomSheetSnappingStateResting;
      } else if (_sheetState == BottomSheetSnappingStateResting) {
        targetState = BottomSheetSnappingStateExpanded;
      }
    }
  } else {
    // Slow drag: snap to the closest state based on distance from
    // currentConstant.
    CGFloat distExpanded = std::abs(currentConstant - expanded);
    CGFloat distResting = std::abs(currentConstant - resting);
    CGFloat distCollapsed = std::abs(currentConstant - collapsed);

    CGFloat minDist = MIN(distExpanded, MIN(distResting, distCollapsed));
    if (minDist == distExpanded) {
      targetState = BottomSheetSnappingStateExpanded;
    } else if (minDist == distResting) {
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

  if (_sheetState == BottomSheetSnappingStateExpanded && _feedScrollView) {
    if (_feedScrollView.contentOffset.y > 0) {
      _initialConstant = [self expandedOffset];
      [gesture setTranslation:CGPointZero inView:superview];
      return;
    }
    if (translation.y < 0) {
      _initialConstant = [self expandedOffset];
      [gesture setTranslation:CGPointZero inView:superview];
      return;
    }
  }

  CGFloat targetConstant = _initialConstant + translation.y;
  CGFloat minOffset = [self expandedOffset];
  CGFloat maxOffset = [self collapsedOffset];

  if (targetConstant < minOffset) {
    targetConstant = minOffset;
  } else if (targetConstant > maxOffset) {
    targetConstant = maxOffset;
  }

  _bottomSheetTopConstraint.constant = targetConstant;
  [self updateContentContainerInsetForOffset:targetConstant];
  [self.delegate bottomSheetViewController:self
                        didUpdateTopOffset:targetConstant];

  if (gesture.state == UIGestureRecognizerStateEnded) {
    [self snapSheetWithVelocity:velocity currentConstant:targetConstant];
  } else if (gesture.state == UIGestureRecognizerStateCancelled) {
    [self updateBottomSheetPositionAnimated:YES];
  }
}
#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  if (gestureRecognizer == _sheetPanGesture &&
      otherGestureRecognizer == _feedScrollView.panGestureRecognizer) {
    return _sheetState == BottomSheetSnappingStateExpanded;
  }
  return NO;
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  return YES;
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if (_sheetState != BottomSheetSnappingStateExpanded) {
    return;
  }

  CGFloat currentConstant = _bottomSheetTopConstraint.constant;
  CGFloat expanded = [self expandedOffset];

  if (currentConstant > expanded) {
    scrollView.contentOffset = CGPointZero;
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  CGFloat currentConstant = _bottomSheetTopConstraint.constant;
  if (_sheetState == BottomSheetSnappingStateExpanded &&
      currentConstant > [self expandedOffset]) {
    *targetContentOffset = CGPointZero;
  }
}

@end
