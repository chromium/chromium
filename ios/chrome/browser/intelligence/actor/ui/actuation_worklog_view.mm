// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_tool_chip_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_accessory_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_item_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingSmall;
using intelligence::actor::kSpacingTiny;
using intelligence::actor::kTimelineGutterWidth;

// Animation constants.
const NSTimeInterval kAnimationDuration = 0.25;
const NSTimeInterval kSpringAnimationDuration = 0.4;
const CGFloat kSpringDamping = 1.0;

}  // namespace

@interface ActuationWorklogView () <ActuationWorklogItemViewDelegate>
@end

@implementation ActuationWorklogView {
  UIStackView* _mainStackView;
  UIStackView* _itemsStackView;
  ActorToolChipView* _toolChipView;
  UIStackView* _chipContainer;
  NSMutableArray<ActuationWorklogItemView*>* _itemViews;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _itemViews = [NSMutableArray array];

    _itemsStackView = [[UIStackView alloc] init];
    _itemsStackView.axis = UILayoutConstraintAxisVertical;
    _itemsStackView.translatesAutoresizingMaskIntoConstraints = NO;

    _toolChipView = [[ActorToolChipView alloc] init];
    _toolChipView.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* spacer = [[UIView alloc] init];
    _chipContainer = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _toolChipView, spacer ]];
    _chipContainer.alignment = UIStackViewAlignmentCenter;
    _chipContainer.layoutMarginsRelativeArrangement = YES;
    _chipContainer.layoutMargins = UIEdgeInsetsMake(
        kSpacingTiny, kTimelineGutterWidth, kSpacingSmall, 0.0);
    _chipContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _chipContainer.alpha = 0.0;
    _chipContainer.hidden = YES;

    _mainStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _itemsStackView, _chipContainer ]];
    _mainStackView.axis = UILayoutConstraintAxisVertical;
    _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_mainStackView];

    AddSameConstraints(_mainStackView, self);
  }
  return self;
}

#pragma mark - Public

- (void)setCollapsed:(BOOL)collapsed {
  [self setCollapsed:collapsed animated:NO];
}

- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated {
  if (_collapsed == collapsed) {
    return;
  }
  _collapsed = collapsed;

  if (animated) {
    [UIView transitionWithView:self
                      duration:kAnimationDuration
                       options:UIViewAnimationOptionTransitionCrossDissolve
                    animations:^{
                      [self updateVisibilityAndConnectors];
                    }
                    completion:nil];
  } else {
    [self updateVisibilityAndConnectors];
  }
}

- (void)setItems:(NSArray<ActuationWorklogItem*>*)items {
  CHECK(items);

  // Clear existing subviews.
  for (UIView* view in [_itemsStackView.arrangedSubviews copy]) {
    [view removeFromSuperview];
  }
  [_itemViews removeAllObjects];

  // Populate new views.
  NSInteger count = (NSInteger)items.count;
  for (NSInteger i = 0; i < count; ++i) {
    ActuationWorklogItemView* itemView =
        [[ActuationWorklogItemView alloc] init];
    itemView.delegate = self;
    [itemView configureWithItem:items[i]];
    [_itemViews addObject:itemView];
    [_itemsStackView addArrangedSubview:itemView];
  }

  [self updateVisibilityAndConnectors];
}

- (void)addItem:(ActuationWorklogItem*)item {
  CHECK(item);

  ActuationWorklogItemView* itemView = [[ActuationWorklogItemView alloc] init];
  itemView.delegate = self;
  [itemView configureWithItem:item];

  [_itemViews addObject:itemView];

  // Pre-hide and set alpha 0 before adding to stack view so UIKit does not
  // animate its frame from (0,0).
  itemView.hidden = YES;
  itemView.alpha = 0.0;
  [_itemsStackView addArrangedSubview:itemView];

  // Force layout synchronously outside any animation block to anchor the new
  // view's target frame at the bottom of the stack.
  [self updateVisibilityAndConnectors];

  [UIView transitionWithView:self
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    [self updateVisibilityAndConnectors];
                  }
                  completion:nil];
}

- (void)setLastItem:(ActuationWorklogItem*)item {
  if (!item) {
    [self removeLastItem];
    return;
  }

  NSInteger count = (NSInteger)_itemViews.count;
  if (count == 0) {
    [self addItem:item];
    return;
  }

  ActuationWorklogItemView* lastView = _itemViews.lastObject;
  [UIView transitionWithView:self
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    [lastView configureWithItem:item];
                    [self updateVisibilityAndConnectors];
                  }
                  completion:nil];
}

- (void)setChip:(ActuationWorklogChip*)chip {
  if (!chip) {
    [self setChipVisible:NO];
    [self updateVisibilityAndConnectors];
    return;
  }

  BOOL chipAlreadyVisible = !_chipContainer.hidden;
  if (!chipAlreadyVisible) {
    [_toolChipView updateText:chip.text icon:chip.icon];
    [self setChipVisible:YES];
    [self updateVisibilityAndConnectors];
    return;
  }

  ActorToolChipView* chipView = _toolChipView;
  [UIView transitionWithView:_toolChipView
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    [chipView updateText:chip.text icon:chip.icon];
                  }
                  completion:nil];

  [UIView animateWithDuration:kSpringAnimationDuration
                        delay:0.0
       usingSpringWithDamping:kSpringDamping
        initialSpringVelocity:0.0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     [self layoutIfNeeded];
                   }
                   completion:nil];
}

- (void)reset {
  for (UIView* view in [_itemsStackView.arrangedSubviews copy]) {
    [view removeFromSuperview];
  }
  [_itemViews removeAllObjects];
  _collapsed = NO;
  _chipContainer.hidden = YES;
  _chipContainer.alpha = 0.0;
}

#pragma mark - ActuationWorklogItemViewDelegate

- (void)worklogItemViewDidTapItem:(ActuationWorklogItemView*)itemView {
  if (_itemViews.count > 0 && itemView == _itemViews[0]) {
    [self setCollapsed:!_collapsed animated:YES];
    [self.delegate worklogView:self didChangeCollapsed:_collapsed];
  }
}

- (void)worklogItemView:(ActuationWorklogItemView*)itemView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem {
  [self.delegate worklogView:self didTapAccessoryItem:accessoryItem];
}

#pragma mark - Private

// Synchronizes subview visibility and timeline connector styles with the
// current `_collapsed` state. In collapsed mode, only the header item and
// any currently active in-progress item remain visible.
- (void)updateVisibilityAndConnectors {
  NSInteger count = (NSInteger)_itemViews.count;
  if (count == 0) {
    return;
  }

  ActuationWorklogItemView* lastView = _itemViews.lastObject;
  // Collapsing is only meaningful when multiple items exist in the worklog.
  BOOL shouldCollapse = _collapsed && (count > 1);
  // Keep the trailing item visible during collapse if it is actively running.
  BOOL showLast = shouldCollapse && lastView.item.active;
  for (NSInteger i = 0; i < count; ++i) {
    ActuationWorklogItemView* view = _itemViews[i];
    BOOL isFirst = (i == 0);
    BOOL isLast = (i == count - 1);
    // In collapsed mode, only show the header and any active in-progress items.
    BOOL shouldBeVisible = !shouldCollapse || isFirst || (showLast && isLast);

    // Unhide expanding items at alpha 0 so they fade in smoothly.
    if (shouldBeVisible && view.hidden) {
      view.hidden = NO;
      view.alpha = 0.0;
    }

    // The first item acts as the collapse toggle when multiple items exist.
    if (isFirst) {
      view.collapsible = (count > 1);
      [view setCollapsed:_collapsed animated:NO];
    }

    // Update timeline connector lines to match the current collapse state.
    view.connectorVisibility =
        shouldCollapse ? [self collapsedConnectorsAtIndex:i showLast:showLast]
                       : [self connectorsAtIndex:i];
    view.alpha = shouldBeVisible ? 1.0 : 0.0;
    view.hidden = !shouldBeVisible;
  }

  [self layoutIfNeeded];
}

// Removes the trailing timeline item from the log with an animated
// transition, synchronizing layout and connectors after removal.
- (void)removeLastItem {
  ActuationWorklogItemView* lastView = _itemViews.lastObject;
  if (!lastView) {
    return;
  }
  [_itemViews removeLastObject];

  if (_collapsed) {
    // Anchor starting visibility and layout frames synchronously before UIKit
    // takes the bitmap layer snapshot for the transition.
    [self updateVisibilityAndConnectors];
    [UIView transitionWithView:self
        duration:kAnimationDuration
        options:UIViewAnimationOptionTransitionCrossDissolve
        animations:^{
          [self updateVisibilityAndConnectors];
        }
        completion:^(BOOL finished) {
          [lastView removeFromSuperview];
        }];
  } else {
    [UIView animateWithDuration:kAnimationDuration
        animations:^{
          lastView.alpha = 0.0;
          lastView.hidden = YES;
          [self layoutIfNeeded];
        }
        completion:^(BOOL finished) {
          [lastView removeFromSuperview];
          [self updateVisibilityAndConnectors];
        }];
  }
}

// Sets the visibility of the tool chip container at the bottom of the worklog.
- (void)setChipVisible:(BOOL)visible {
  BOOL visibilityChanged = _chipContainer.hidden != !visible;
  if (!visibilityChanged) {
    return;
  }

  if (visible) {
    _chipContainer.hidden = NO;
    [self layoutIfNeeded];
  }

  UIView* chipContainer = _chipContainer;
  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     chipContainer.alpha = visible ? 1.0 : 0.0;
                     chipContainer.hidden = !visible;
                     [self layoutIfNeeded];
                   }];
}

// Returns the connector line segments (top, bottom, both, or none) for an
// item at `index` when the worklog is in the expanded state.
- (ActuationWorklogConnectorVisibility)connectorsAtIndex:(NSInteger)index {
  NSInteger count = (NSInteger)_itemViews.count;
  BOOL hasChip = !_chipContainer.hidden;
  BOOL isFirst = (index == 0);
  BOOL isLast = (index == count - 1);
  if (isFirst && isLast) {
    return hasChip ? ActuationWorklogConnectorVisibility::kBottom
                   : ActuationWorklogConnectorVisibility::kNone;
  }
  if (isFirst) {
    return ActuationWorklogConnectorVisibility::kBottom;
  }
  if (isLast) {
    return hasChip ? ActuationWorklogConnectorVisibility::kBoth
                   : ActuationWorklogConnectorVisibility::kTop;
  }
  return ActuationWorklogConnectorVisibility::kBoth;
}

// Returns the connector line segments for an item at `index` when the worklog
// is in the collapsed state, accounting for whether the last active item or
// tool chip is shown.
- (ActuationWorklogConnectorVisibility)
    collapsedConnectorsAtIndex:(NSInteger)index
                      showLast:(BOOL)showLast {
  NSInteger count = (NSInteger)_itemViews.count;
  BOOL hasChip = !_chipContainer.hidden;
  BOOL isFirst = (index == 0);
  BOOL isLast = (index == count - 1);
  if (isFirst) {
    return (showLast || hasChip) ? ActuationWorklogConnectorVisibility::kBottom
                                 : ActuationWorklogConnectorVisibility::kNone;
  }
  if (showLast && isLast) {
    return hasChip ? ActuationWorklogConnectorVisibility::kBoth
                   : ActuationWorklogConnectorVisibility::kTop;
  }
  return ActuationWorklogConnectorVisibility::kNone;
}

@end
