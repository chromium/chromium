// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_compact_view.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_tool_chip_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;
using intelligence::actor::kTimelineGutterWidth;
using intelligence::actor::kToolChipHeight;

// Animation transition duration in seconds.
const NSTimeInterval kAnimationDuration = 0.5;

// Spring damping ratio (1.0 = critically damped, no overshoot).
const CGFloat kSpringDamping = 1.0;

// The spacing buffer reserved at the bottom of compact timeline cells.
const CGFloat kCompactBottomBufferHeight = kToolChipHeight + kSpacingMedium;

}  // namespace

// Private helper container representing a pending layout transition queue item.
@interface ActuationWorklogPendingTransition : NSObject
@property(nonatomic, strong) ActuationWorklogItem* item;
@property(nonatomic, strong) ActuationWorklogChip* chip;
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

  UIView* _stepsView;
  ActorToolChipView* _toolChipView;
  UIView* _chipContainer;
  NSLayoutConstraint* _stepsViewHeightConstraint;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.clipsToBounds = YES;
    _pendingTransitions = [NSMutableArray array];

    _stepsView = [[UIView alloc] init];
    _stepsView.clipsToBounds = YES;
    _stepsView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_stepsView];
    [NSLayoutConstraint activateConstraints:@[
      [_stepsView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_stepsView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_stepsView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    ]];

    _chipContainer = [[UIView alloc] init];
    _chipContainer.alpha = 0.0;
    _chipContainer.hidden = YES;
    _chipContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_chipContainer];

    _toolChipView = [[ActorToolChipView alloc] init];
    _toolChipView.translatesAutoresizingMaskIntoConstraints = NO;
    [_chipContainer addSubview:_toolChipView];
    AddSameConstraints(_toolChipView, _chipContainer);

    [NSLayoutConstraint activateConstraints:@[
      [_chipContainer.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                                  constant:-kSpacingSmall],
      [_chipContainer.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kTimelineGutterWidth],
    ]];

    _stepsViewHeightConstraint =
        [_stepsView.heightAnchor constraintEqualToConstant:0.0];
    _stepsViewHeightConstraint.active = YES;
  }
  return self;
}

- (void)transitionToItem:(ActuationWorklogItem*)item
                    chip:(ActuationWorklogChip*)chip
                animated:(BOOL)animated {
  CHECK(item);
  if (!_currentView) {
    [self loadFirstItem:item chip:chip];
    return;
  }

  ActuationWorklogPendingTransition* transition =
      [[ActuationWorklogPendingTransition alloc] init];
  transition.item = item;
  transition.chip = chip;
  transition.animated = animated;

  [_pendingTransitions addObject:transition];
  [self processNextPendingTransitionIfNeeded];
}

- (void)reset {
  [_pendingTransitions removeAllObjects];
  _isTransitioning = NO;
  [_currentView removeFromSuperview];
  [_nextView removeFromSuperview];
  _currentView = nil;
  _nextView = nil;
  _currentTopConstraint = nil;
  _stepsViewHeightConstraint.constant = 0.0;
  _lastReportedHeight = 0.0;
  [self setChipVisible:NO];
}

#pragma mark - UIView

- (void)layoutSubviews {
  if (_currentView && !_isTransitioning) {
    _stepsViewHeightConstraint.constant =
        [_currentView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
            .height;
  }
  [super layoutSubviews];

  // Only update height when resting (not transitioning).
  if (_currentView && !_isTransitioning) {
    [self notifyHeight];
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
  [self prepareForTransition:transition];

  CGFloat targetHeight = _nextView.bounds.size.height;
  _currentTopConstraint.constant = -_currentView.bounds.size.height;
  _stepsViewHeightConstraint.constant = targetHeight;

  [self applyChipTransition:transition];
  [self applyLayoutTransition:transition];
}

- (void)prepareForTransition:(ActuationWorklogPendingTransition*)transition {
  _nextView = [self addViewWithItem:transition.item
                          connector:ActuationWorklogConnectorVisibility::kBoth];
  CGFloat chipHeight = transition.chip ? kCompactBottomBufferHeight : 0.0;
  _nextView.bottomBufferHeight = chipHeight;

  [NSLayoutConstraint activateConstraints:@[
    [_nextView.topAnchor constraintEqualToAnchor:_currentView.bottomAnchor],
  ]];

  // Force placement of `_nextView` directly below.
  [self layoutIfNeeded];
}

- (void)applyChipTransition:(ActuationWorklogPendingTransition*)transition {
  ActuationWorklogChip* chip = transition.chip;
  if (!chip) {
    return;
  }

  if (!transition.animated || _chipContainer.hidden) {
    [_toolChipView updateText:chip.text icon:chip.icon];
    return;
  }

  // Cross dissolve only when there is already a chip present.
  ActorToolChipView* chipView = _toolChipView;
  [UIView transitionWithView:_toolChipView
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    [chipView updateText:chip.text icon:chip.icon];
                  }
                  completion:nil];
}

// Performs the slide and height layout transition animation.
- (void)applyLayoutTransition:(ActuationWorklogPendingTransition*)transition {
  BOOL show = (transition.chip != nil);
  BOOL animated = transition.animated;
  BOOL visibilityChanged = (_chipContainer.hidden != !show);
  if (!animated) {
    [self setChipVisible:show];
    [self notifyHeight];
    [self transitionDidComplete];
    return;
  }

  // Unhide the container if showing to allow the fade-in animation.
  if (show) {
    _chipContainer.hidden = NO;
  }

  UIView* chipContainer = _chipContainer;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
      delay:0.0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:0.0
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        [weakSelf layoutIfNeeded];
        if (visibilityChanged) {
          chipContainer.alpha = show ? 1.0 : 0.0;
        }
        [weakSelf notifyHeight];
      }
      completion:^(BOOL finished) {
        [weakSelf setChipVisible:show];
        [weakSelf transitionDidComplete];
      }];
}

// Finalizes the layout state and view promotion once a transition completes.
- (void)transitionDidComplete {
  if (_nextView) {
    [_currentView removeFromSuperview];
    _currentView = _nextView;
    _currentTopConstraint =
        [_currentView.topAnchor constraintEqualToAnchor:_stepsView.topAnchor];
    _currentTopConstraint.active = YES;
    _nextView = nil;
  }

  _isTransitioning = NO;
  [self processNextPendingTransitionIfNeeded];
}

// Renders the first step immediately without transition animations.
- (void)loadFirstItem:(ActuationWorklogItem*)item
                 chip:(ActuationWorklogChip*)chip {
  _currentView =
      [self addViewWithItem:item
                  connector:ActuationWorklogConnectorVisibility::kBottom];

  if (chip) {
    [_toolChipView updateText:chip.text icon:chip.icon];
  }
  [self setChipVisible:(chip != nil)];

  CGFloat chipHeight = chip ? kCompactBottomBufferHeight : 0.0;
  _currentView.bottomBufferHeight = chipHeight;

  _currentTopConstraint =
      [_currentView.topAnchor constraintEqualToAnchor:_stepsView.topAnchor];
  _currentTopConstraint.active = YES;

  _stepsViewHeightConstraint.constant =
      [_currentView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;

  [self layoutIfNeeded];
  [self notifyHeight];
}

// Helper to instantiate, add, and horizontally anchor a new step view.
- (ActuationWorklogItemView*)
    addViewWithItem:(ActuationWorklogItem*)item
          connector:(ActuationWorklogConnectorVisibility)connector {
  ActuationWorklogItemView* view = [[ActuationWorklogItemView alloc] init];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [_stepsView addSubview:view];

  [NSLayoutConstraint activateConstraints:@[
    [view.leadingAnchor constraintEqualToAnchor:_stepsView.leadingAnchor],
    [view.trailingAnchor constraintEqualToAnchor:_stepsView.trailingAnchor],
  ]];

  view.connectorVisibility = connector;
  [view configureWithItem:item];
  return view;
}

// Updates the chip container visibility and opacity states in sync.
- (void)setChipVisible:(BOOL)visible {
  _chipContainer.hidden = !visible;
  _chipContainer.alpha = visible ? 1.0 : 0.0;
}

// Notifies `delegate` of view height changes, querying layout height
// dynamically.
- (void)notifyHeight {
  CGFloat height = _stepsViewHeightConstraint.constant;
  if (height == _lastReportedHeight) {
    return;
  }
  _lastReportedHeight = height;
  [self.delegate worklogCompactView:self didChangeHeight:height];
}

@end
