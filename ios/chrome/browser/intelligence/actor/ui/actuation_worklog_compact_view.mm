// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_compact_view.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"

namespace {

// Animation transition duration in seconds.
const NSTimeInterval kAnimationDuration = 0.5;

// Spring damping ratio (1.0 = critically damped, no overshoot).
const CGFloat kSpringDamping = 1.0;

}  // namespace

// Private helper container representing a pending layout transition queue item.
@interface ActuationWorklogPendingTransition : NSObject
@property(nonatomic, strong) ActuationWorklogItem* item;
@property(nonatomic, assign) BOOL animated;
@end

@implementation ActuationWorklogPendingTransition
@end

@implementation ActuationWorklogCompactView {
  ActuationWorklogItemView* _currentView;
  ActuationWorklogItemView* _nextView;

  NSLayoutConstraint* _currentTopConstraint;

  NSMutableArray<ActuationWorklogPendingTransition*>* _pendingTransitions;
  BOOL _isTransitioning;
  CGFloat _lastReportedHeight;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.clipsToBounds = YES;
    _pendingTransitions = [NSMutableArray array];
    _isTransitioning = NO;
    _lastReportedHeight = 0.0;
  }
  return self;
}

- (void)transitionToItem:(ActuationWorklogItem*)item animated:(BOOL)animated {
  CHECK(item);
  if (!_currentView) {
    [self loadFirstItem:item];
    return;
  }

  ActuationWorklogPendingTransition* transition =
      [[ActuationWorklogPendingTransition alloc] init];
  transition.item = item;
  transition.animated = animated;

  [_pendingTransitions addObject:transition];
  [self processNextPendingTransitionIfNeeded];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  // Only update height when resting (not transitioning).
  if (_currentView && !_isTransitioning) {
    [self notifyHeight:_currentView.bounds.size.height];
  }
}

#pragma mark - Private

// Dequeues and executes the next available transition if we are not currently
// transitioning.
- (void)processNextPendingTransitionIfNeeded {
  if (_isTransitioning || _pendingTransitions.count == 0) {
    return;
  }

  _isTransitioning = YES;
  ActuationWorklogPendingTransition* transition =
      _pendingTransitions.firstObject;
  [_pendingTransitions removeObjectAtIndex:0];

  [self executeTransition:transition];
}

// Prepares and executes a queued transition.
- (void)executeTransition:(ActuationWorklogPendingTransition*)transition {
  [self prepareTransitionToItem:transition.item];

  CGFloat targetHeight = _nextView.bounds.size.height;
  _currentTopConstraint.constant = -_currentView.bounds.size.height;

  [self transitionToHeight:targetHeight animated:transition.animated];
}

// Instantiates the incoming step view and anchors it directly below the active
// view in preparation for the slide transition.
- (void)prepareTransitionToItem:(ActuationWorklogItem*)item {
  _nextView = [self addViewWithItem:item
                          connector:ActuationWorklogConnectorVisibility::kBoth];

  [NSLayoutConstraint activateConstraints:@[
    [_nextView.topAnchor constraintEqualToAnchor:_currentView.bottomAnchor],
  ]];

  // Force placement of `_nextView` directly below
  [self layoutIfNeeded];
}

// Executes the height layout change transition.
- (void)transitionToHeight:(CGFloat)targetHeight animated:(BOOL)animated {
  if (!animated) {
    [self notifyHeight:targetHeight];
    [self transitionDidComplete];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
      delay:0.0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:0.0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [weakSelf notifyHeight:targetHeight];
        [weakSelf layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        [weakSelf transitionDidComplete];
      }];
}

// Finalizes the layout state and view promotion once a transition completes.
- (void)transitionDidComplete {
  if (_nextView) {
    [_currentView removeFromSuperview];
    _currentView = _nextView;
    _currentTopConstraint =
        [_currentView.topAnchor constraintEqualToAnchor:self.topAnchor];
    _currentTopConstraint.active = YES;
    _nextView = nil;
  }

  _isTransitioning = NO;
  [self processNextPendingTransitionIfNeeded];
}

// Renders the first step immediately without transition animations.
- (void)loadFirstItem:(ActuationWorklogItem*)item {
  _currentView =
      [self addViewWithItem:item
                  connector:ActuationWorklogConnectorVisibility::kBottom];

  _currentTopConstraint =
      [_currentView.topAnchor constraintEqualToAnchor:self.topAnchor];
  _currentTopConstraint.active = YES;
  [self layoutIfNeeded];
  [self notifyHeight:_currentView.bounds.size.height];
}

// Helper to instantiate, add, and horizontally anchor a new step view.
- (ActuationWorklogItemView*)
    addViewWithItem:(ActuationWorklogItem*)item
          connector:(ActuationWorklogConnectorVisibility)connector {
  ActuationWorklogItemView* view = [[ActuationWorklogItemView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:view];

  [NSLayoutConstraint activateConstraints:@[
    [view.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [view.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ]];

  view.connectorVisibility = connector;
  [view configureWithItem:item];
  return view;
}

// Notifies `delegate` of view height changes, guarding against redundant calls.
- (void)notifyHeight:(CGFloat)height {
  if (height == _lastReportedHeight) {
    return;
  }
  _lastReportedHeight = height;
  [self.delegate worklogCompactView:self didChangeHeight:height];
}

@end
