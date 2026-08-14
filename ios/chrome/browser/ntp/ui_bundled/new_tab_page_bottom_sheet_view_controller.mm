// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import <cmath>

#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/scroll_delegate_proxy.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
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
constexpr CGFloat kMagicStackToFeedSpacing = 16.0;

// Minimum drag velocity required to trigger a state transition.
constexpr CGFloat kMinimumDragVelocityToChangeState = 250.0;

}  // namespace

@interface NewTabPageBottomSheetViewController () <UIGestureRecognizerDelegate,
                                                   UIScrollViewDelegate>
@property(nonatomic, strong) NSLayoutConstraint* bottomSheetTopConstraint;
@end

@implementation NewTabPageBottomSheetViewController {
  UIView* _dragHandle;
  UIView* _headerContainerView;
  UIView* _magicStackContainerView;
  NSLayoutConstraint* _magicStackTopConstraint;
  UIView* _mostVisitedContainerView;
  UIView* _mostVisitedView;
  UIView* _contentContainerView;
  BottomSheetSnappingState _sheetState;

  CGSize _lastSize;
  CGFloat _initialConstant;

  UIPanGestureRecognizer* _sheetPanGesture;
  __weak UIScrollView* _feedScrollView;

  // Proxy to intercept feed scroll events for VoiceOver auto-expand.
  ScrollDelegateProxy* _scrollProxy;

  // The original delegate of the feed scroll view.
  __weak id<UIScrollViewDelegate> _originalFeedDelegate;

  BOOL _isBottomOmnibox;
}

- (void)setOmniboxInBottomPosition:(BOOL)isBottomOmnibox {
  if (!IsNTPRedesignStaticFakeboxEnabled()) {
    return;
  }
  _isBottomOmnibox = isBottomOmnibox;
  [self updateFeedInsetsForBottomOmnibox];
}

- (void)updateFeedInsetsForBottomOmnibox {
  if (!IsNTPRedesignStaticFakeboxEnabled() || !_feedScrollView) {
    return;
  }
  CGFloat bottomInset = 0.0;
  if (_isBottomOmnibox && _sheetState == BottomSheetSnappingStateExpanded) {
    bottomInset = kToolbarHeight + self.view.safeAreaInsets.bottom;
  }
  UIEdgeInsets insets = _feedScrollView.contentInset;
  if (insets.bottom != bottomInset) {
    insets.bottom = bottomInset;
    _feedScrollView.contentInset = insets;
    _feedScrollView.scrollIndicatorInsets = insets;
  }
}

- (void)loadView {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  self.view = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(voiceOverStatusDidChange)
             name:UIAccessibilityVoiceOverStatusDidChangeNotification
           object:nil];

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

  // Add header container view that encapsulates MVT and Magic Stack.
  _headerContainerView = [[UIView alloc] init];
  _headerContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [visualEffectView.contentView addSubview:_headerContainerView];

  [NSLayoutConstraint activateConstraints:@[
    [_headerContainerView.leadingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.leadingAnchor],
    [_headerContainerView.trailingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.trailingAnchor],
    [_headerContainerView.topAnchor
        constraintEqualToAnchor:_dragHandle.bottomAnchor
                       constant:kContentContainerTopMargin],
  ]];

  // Add most visited tiles container view if feature is enabled.
  if (IsMVTInBottomSheetEnabled()) {
    _mostVisitedContainerView = [[UIView alloc] init];
    _mostVisitedContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    [_headerContainerView addSubview:_mostVisitedContainerView];

    [NSLayoutConstraint activateConstraints:@[
      [_mostVisitedContainerView.leadingAnchor
          constraintEqualToAnchor:_headerContainerView.leadingAnchor],
      [_mostVisitedContainerView.trailingAnchor
          constraintEqualToAnchor:_headerContainerView.trailingAnchor],
      [_mostVisitedContainerView.topAnchor
          constraintEqualToAnchor:_headerContainerView.topAnchor],
    ]];
  }

  // Add magic stack container view.
  _magicStackContainerView = [[UIView alloc] init];
  _magicStackContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [_headerContainerView addSubview:_magicStackContainerView];

  _magicStackTopConstraint = [_magicStackContainerView.topAnchor
      constraintEqualToAnchor:_headerContainerView.topAnchor
                     constant:0.0];
  _magicStackTopConstraint.active = YES;

  [NSLayoutConstraint activateConstraints:@[
    [_magicStackContainerView.leadingAnchor
        constraintEqualToAnchor:_headerContainerView.leadingAnchor],
    [_magicStackContainerView.trailingAnchor
        constraintEqualToAnchor:_headerContainerView.trailingAnchor],
    [_magicStackContainerView.heightAnchor
        constraintEqualToConstant:kMagicStackHeight],
    [_headerContainerView.bottomAnchor
        constraintEqualToAnchor:_magicStackContainerView.bottomAnchor],
  ]];

  // Add content container view.
  _contentContainerView = [[UIView alloc] init];
  _contentContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [visualEffectView.contentView addSubview:_contentContainerView];

  [NSLayoutConstraint activateConstraints:@[
    [_contentContainerView.leadingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.leadingAnchor],
    [_contentContainerView.trailingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.trailingAnchor],
    [_contentContainerView.topAnchor
        constraintEqualToAnchor:_headerContainerView.bottomAnchor
                       constant:kMagicStackToFeedSpacing],
    [_contentContainerView.bottomAnchor
        constraintEqualToAnchor:visualEffectView.contentView.bottomAnchor],
  ]];

  // Add pan gesture recognizer.
  _sheetPanGesture =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  _sheetPanGesture.delegate = self;
  [self.view addGestureRecognizer:_sheetPanGesture];

  [self updateContentContainerInsetForOffset:
            [self targetOffsetForState:_sheetState]];

  if (_mostVisitedView) {
    [self embedMostVisitedView:_mostVisitedView];
  }

  if (_magicStackViewController) {
    [self embedMagicStackViewController];
  }

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
  self.magicStackViewController = nil;
  _mostVisitedView = nil;
  _headerContainerView = nil;
}

- (BOOL)accessibilityPerformEscape {
  if (_sheetState == BottomSheetSnappingStateExpanded) {
    _sheetState = BottomSheetSnappingStateResting;
    [self updateBottomSheetPositionAnimated:YES];
    if ([self.delegate
            respondsToSelector:@selector(
                                   bottomSheetViewControllerDidEscape:)]) {
      [self.delegate bottomSheetViewControllerDidEscape:self];
    }
    return YES;
  }
  return NO;
}

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
  UIScrollView* newFeedScrollView = nil;
  if (_feedViewController) {
    newFeedScrollView = [self findScrollViewInView:_feedViewController.view];
  }

  if (_feedScrollView == newFeedScrollView) {
    return;
  }

  if (_feedScrollView) {
    [_feedScrollView.panGestureRecognizer
        removeTarget:self
              action:@selector(handleFeedPan:)];
    if (_scrollProxy) {
      _feedScrollView.delegate = _originalFeedDelegate;
      _scrollProxy = nil;
    }
  }

  _feedScrollView = newFeedScrollView;

  if (_feedScrollView) {
    _originalFeedDelegate = _feedScrollView.delegate;
    if ([self isVoiceOverRunning]) {
      _scrollProxy = [[ScrollDelegateProxy alloc]
          initWithInterceptingTarget:self
                      originalTarget:_originalFeedDelegate];
      _feedScrollView.delegate = _scrollProxy;
    }

    _feedScrollView.scrollEnabled =
        (_sheetState == BottomSheetSnappingStateExpanded) ||
        [self isVoiceOverRunning];
    [_feedScrollView.panGestureRecognizer addTarget:self
                                             action:@selector(handleFeedPan:)];
    if (IsNTPRedesignStaticFakeboxEnabled()) {
      [self updateFeedInsetsForBottomOmnibox];
    }
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

- (void)setMagicStackViewController:
    (UIViewController*)magicStackViewController {
  if (_magicStackViewController == magicStackViewController) {
    return;
  }
  if (_magicStackViewController) {
    [_magicStackViewController willMoveToParentViewController:nil];
    [_magicStackViewController.view removeFromSuperview];
    [_magicStackViewController removeFromParentViewController];
  }
  _magicStackViewController = magicStackViewController;
  if (self.isViewLoaded && _magicStackViewController) {
    [self embedMagicStackViewController];
  }
}

- (void)embedMagicStackViewController {
  if (!_magicStackViewController || !_magicStackContainerView) {
    return;
  }
  if (_magicStackViewController.parentViewController == self) {
    return;
  }
  if (_magicStackViewController.parentViewController) {
    [_magicStackViewController willMoveToParentViewController:nil];
    [_magicStackViewController.view removeFromSuperview];
    [_magicStackViewController removeFromParentViewController];
  }
  [self addChildViewController:_magicStackViewController];
  _magicStackViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [_magicStackContainerView addSubview:_magicStackViewController.view];
  AddSameConstraints(_magicStackViewController.view, _magicStackContainerView);
  [_magicStackViewController didMoveToParentViewController:self];
}

- (void)embedMostVisitedView:(UIView*)mostVisitedView {
  if (_mostVisitedView != mostVisitedView) {
    if (_mostVisitedView) {
      [_mostVisitedView removeFromSuperview];
    }
    _mostVisitedView = mostVisitedView;
  }
  if (self.isViewLoaded && _mostVisitedView && _mostVisitedContainerView) {
    if (_mostVisitedView.superview == _mostVisitedContainerView) {
      return;
    }
    _mostVisitedView.translatesAutoresizingMaskIntoConstraints = NO;
    [_mostVisitedContainerView addSubview:_mostVisitedView];
    AddSameConstraints(_mostVisitedView, _mostVisitedContainerView);
    [self.view setNeedsLayout];
    [self.view layoutIfNeeded];
    CGFloat offset = _bottomSheetTopConstraint
                         ? _bottomSheetTopConstraint.constant
                         : [self restingOffset];
    [self updateContentContainerInsetForOffset:offset];
  }
}

#pragma mark - Snapping Offsets

- (CGFloat)collapsedOffset {
  return [self.delegate collapsedOffsetForBottomSheetViewController:self];
}

- (CGFloat)restingOffset {
  return [self.delegate restingOffsetForBottomSheetViewController:self];
}

- (CGFloat)expandedOffset {
  return [self.delegate expandedOffsetForBottomSheetViewController:self];
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
        (_sheetState == BottomSheetSnappingStateExpanded) ||
        UIAccessibilityIsVoiceOverRunning();
    _feedScrollView.bounces = (_sheetState == BottomSheetSnappingStateExpanded);
    if (IsNTPRedesignStaticFakeboxEnabled()) {
      [self updateFeedInsetsForBottomOmnibox];
    }
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

- (CGFloat)mostVisitedTilesHeight {
  CGFloat mvtHeight = CGRectGetHeight(_mostVisitedContainerView.bounds);
  if (mvtHeight <= 0 && _mostVisitedView) {
    mvtHeight = [_mostVisitedView
                    systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
                    .height;
  }
  return mvtHeight;
}

- (void)updateStaticFakeboxContentContainerInsetForOffset:(CGFloat)topOffset {
  _magicStackContainerView.alpha = 1.0;

  CGFloat expanded = [self expandedOffset];
  CGFloat resting = [self restingOffset];
  if (resting <= expanded) {
    if (IsMVTInBottomSheetEnabled()) {
      _mostVisitedContainerView.alpha = 0.0;
    }
    _magicStackTopConstraint.constant = 0.0;
    return;
  }

  CGFloat progress = (topOffset - expanded) / (resting - expanded);
  progress = MIN(1.0, MAX(0.0, progress));

  if (IsMVTInBottomSheetEnabled()) {
    _mostVisitedContainerView.alpha = progress;
    CGFloat mvtHeight = [self mostVisitedTilesHeight];
    CGFloat restingMagicStackTop =
        (mvtHeight > 0)
            ? (mvtHeight +
               content_suggestions::ReducedModuleSpacing(self.traitCollection))
            : 0.0;
    _magicStackTopConstraint.constant = progress * restingMagicStackTop;
  } else {
    _magicStackTopConstraint.constant = 0.0;
  }
}

- (void)updateLegacyContentContainerInsetForOffset:(CGFloat)topOffset {
  CGFloat expanded = [self expandedOffset];
  CGFloat resting = [self restingOffset];
  if (resting <= expanded) {
    if (IsMVTInBottomSheetEnabled()) {
      _mostVisitedContainerView.alpha = 0.0;
      _magicStackTopConstraint.constant =
          content_suggestions::FakeOmniboxHeight();
    } else {
      _magicStackContainerView.alpha = 1.0;
      _magicStackTopConstraint.constant = 0.0;
    }
    return;
  }

  CGFloat progress = (topOffset - expanded) / (resting - expanded);
  progress = MIN(1.0, MAX(0.0, progress));

  if (IsMVTInBottomSheetEnabled()) {
    _mostVisitedContainerView.alpha = progress;
    CGFloat mvtHeight = [self mostVisitedTilesHeight];
    CGFloat expandedMagicStackTop = content_suggestions::FakeOmniboxHeight();
    CGFloat restingMagicStackTop =
        (mvtHeight > 0)
            ? (mvtHeight +
               content_suggestions::ReducedModuleSpacing(self.traitCollection))
            : 0.0;
    _magicStackTopConstraint.constant =
        progress * restingMagicStackTop +
        (1.0 - progress) * expandedMagicStackTop;
  } else {
    _magicStackContainerView.alpha = progress;
    CGFloat expandedMagicStackTop =
        content_suggestions::FakeOmniboxHeight() - kMagicStackHeight;
    CGFloat restingMagicStackTop = 0.0;
    _magicStackTopConstraint.constant =
        progress * restingMagicStackTop +
        (1.0 - progress) * expandedMagicStackTop;
  }
}

- (void)updateContentContainerInsetForOffset:(CGFloat)topOffset {
  if (IsNTPRedesignStaticFakeboxEnabled()) {
    [self updateStaticFakeboxContentContainerInsetForOffset:topOffset];
  } else {
    [self updateLegacyContentContainerInsetForOffset:topOffset];
  }
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

  if (targetConstant > minOffset && _feedScrollView &&
      _feedScrollView.contentOffset.y > 0) {
    [_feedScrollView setContentOffset:CGPointZero animated:NO];
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
       shouldReceiveTouch:(UITouch*)touch {
  if (gestureRecognizer == _sheetPanGesture) {
    CGPoint point = [touch locationInView:_magicStackContainerView];
    if ([_magicStackContainerView pointInside:point withEvent:nil] &&
        _magicStackContainerView.alpha > 0.0) {
      return NO;
    }
    if (_feedScrollView && _sheetState == BottomSheetSnappingStateExpanded) {
      CGPoint feedPoint = [touch locationInView:_feedScrollView];
      if ([_feedScrollView pointInside:feedPoint withEvent:nil]) {
        return NO;
      }
    }
  }
  return YES;
}

- (void)handleFeedPan:(UIPanGestureRecognizer*)gesture {
  if (_sheetState != BottomSheetSnappingStateExpanded || !_feedScrollView) {
    return;
  }

  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }

  CGPoint translation = [gesture translationInView:superview];
  CGPoint velocity = [gesture velocityInView:superview];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    _initialConstant = _bottomSheetTopConstraint.constant;
  }

  CGFloat expandedOffset = [self expandedOffset];

  if (_bottomSheetTopConstraint.constant <= expandedOffset &&
      translation.y < 0) {
    _initialConstant = expandedOffset;
    [gesture setTranslation:CGPointZero inView:superview];
    _feedScrollView.bounces = YES;
    return;
  }

  if (_bottomSheetTopConstraint.constant <= expandedOffset &&
      _feedScrollView.contentOffset.y > 0) {
    _initialConstant = expandedOffset;
    [gesture setTranslation:CGPointZero inView:superview];
    _feedScrollView.bounces = YES;
    return;
  }

  _feedScrollView.contentOffset = CGPointZero;
  _feedScrollView.bounces = NO;

  CGFloat targetConstant = _initialConstant + translation.y;
  CGFloat maxOffset = [self collapsedOffset];

  if (targetConstant < expandedOffset) {
    targetConstant = expandedOffset;
    _initialConstant = expandedOffset;
    [gesture setTranslation:CGPointZero inView:superview];
  } else if (targetConstant > maxOffset) {
    targetConstant = maxOffset;
  }

  _bottomSheetTopConstraint.constant = targetConstant;
  [self updateContentContainerInsetForOffset:targetConstant];
  [self.delegate bottomSheetViewController:self
                        didUpdateTopOffset:targetConstant];

  if (gesture.state == UIGestureRecognizerStateEnded) {
    if (_bottomSheetTopConstraint.constant > expandedOffset) {
      [self snapSheetWithVelocity:velocity
                  currentConstant:_bottomSheetTopConstraint.constant];
    }
  } else if (gesture.state == UIGestureRecognizerStateCancelled) {
    [self updateBottomSheetPositionAnimated:YES];
  }
}

- (void)voiceOverStatusDidChange {
  if (_feedScrollView) {
    _feedScrollView.scrollEnabled =
        (_sheetState == BottomSheetSnappingStateExpanded) ||
        [self isVoiceOverRunning];

    if ([self isVoiceOverRunning]) {
      if (!_scrollProxy) {
        _scrollProxy = [[ScrollDelegateProxy alloc]
            initWithInterceptingTarget:self
                        originalTarget:_originalFeedDelegate];
        _feedScrollView.delegate = _scrollProxy;
      }
    } else {
      if (_scrollProxy) {
        _feedScrollView.delegate = _originalFeedDelegate;
        _scrollProxy = nil;
      }
    }
  }
}

- (BOOL)isVoiceOverRunning {
  return UIAccessibilityIsVoiceOverRunning();
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  if ([self isVoiceOverRunning]) {
    if (_sheetState != BottomSheetSnappingStateExpanded &&
        scrollView.contentOffset.y > 0) {
      _sheetState = BottomSheetSnappingStateExpanded;
      [self updateBottomSheetPositionAnimated:YES];
    } else if (_sheetState == BottomSheetSnappingStateExpanded &&
               scrollView.contentOffset.y < 0) {
      _sheetState = BottomSheetSnappingStateResting;
      [self updateBottomSheetPositionAnimated:YES];
    }
  }
}

@end
