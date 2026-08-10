// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_tool_chip_view.h"
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
    self.backgroundColor = [UIColor clearColor];
    _itemViews = [NSMutableArray array];

    _itemsStackView = [[UIStackView alloc] init];
    _itemsStackView.axis = UILayoutConstraintAxisVertical;
    _itemsStackView.alignment = UIStackViewAlignmentFill;
    _itemsStackView.distribution = UIStackViewDistributionFill;
    _itemsStackView.translatesAutoresizingMaskIntoConstraints = NO;

    _toolChipView = [[ActorToolChipView alloc] init];
    _toolChipView.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* spacer = [[UIView alloc] init];
    _chipContainer = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _toolChipView, spacer ]];
    _chipContainer.axis = UILayoutConstraintAxisHorizontal;
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
    _mainStackView.alignment = UIStackViewAlignmentFill;
    _mainStackView.distribution = UIStackViewDistributionFill;
    _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_mainStackView];

    AddSameConstraints(_mainStackView, self);
  }
  return self;
}

#pragma mark - Public

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
    itemView.connectorVisibility = [self connectorVisibilityAtIndex:i
                                                         totalCount:count];
    [itemView configureWithItem:items[i]];
    [_itemViews addObject:itemView];
    [_itemsStackView addArrangedSubview:itemView];
  }

  [self layoutIfNeeded];
}

- (void)addItem:(ActuationWorklogItem*)item {
  CHECK(item);

  // Update the previous last item to connect downward with the new item.
  NSInteger count = (NSInteger)_itemViews.count;
  if (count > 0) {
    ActuationWorklogItemView* lastView = _itemViews.lastObject;
    lastView.connectorVisibility = [self connectorVisibilityAtIndex:count - 1
                                                         totalCount:count + 1];
  }

  // Create and append the new view.
  ActuationWorklogItemView* itemView = [[ActuationWorklogItemView alloc] init];
  itemView.connectorVisibility = [self connectorVisibilityAtIndex:count
                                                       totalCount:count + 1];
  [itemView configureWithItem:item];
  [_itemViews addObject:itemView];
  [_itemsStackView addArrangedSubview:itemView];

  itemView.alpha = 0.0;
  itemView.hidden = YES;
  [self layoutIfNeeded];

  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     itemView.hidden = NO;
                     itemView.alpha = 1.0;
                     [self layoutIfNeeded];
                   }];
}

- (void)setLastItem:(ActuationWorklogItem*)item {
  // Item removal.
  if (!item) {
    [self removeLastItem];
    return;
  }

  NSInteger count = (NSInteger)_itemViews.count;
  // Presenting first item.
  if (count == 0) {
    [self addItem:item];
    return;
  }

  // Mutating the last item.
  ActuationWorklogItemView* lastView = _itemViews.lastObject;
  [UIView transitionWithView:lastView
                    duration:kAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    [lastView configureWithItem:item];
                  }
                  completion:nil];

  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     [self layoutIfNeeded];
                   }];
}

- (void)setChip:(ActuationWorklogChip*)chip {
  // Chip removal.
  if (!chip) {
    [self setChipVisible:NO];
    return;
  }

  BOOL chipAlreadyVisible = !_chipContainer.hidden;
  // Presenting new chip.
  if (!chipAlreadyVisible) {
    [_toolChipView updateText:chip.text icon:chip.icon];
    [self setChipVisible:YES];
    return;
  }

  // Mutating the current chip.
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

#pragma mark - Private

// Animates the removal of the last item and changes the active state of
// the preceding item.
- (void)removeLastItem {
  ActuationWorklogItemView* lastView = _itemViews.lastObject;
  if (!lastView) {
    return;
  }
  [_itemViews removeLastObject];

  [UIView animateWithDuration:kAnimationDuration
      animations:^{
        lastView.alpha = 0.0;
        lastView.hidden = YES;
        [self layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        [lastView removeFromSuperview];
      }];

  // Update connector visibility of the new last item.
  ActuationWorklogItemView* newLastView = _itemViews.lastObject;
  if (newLastView) {
    NSInteger count = (NSInteger)_itemViews.count;
    newLastView.connectorVisibility = [self connectorVisibilityAtIndex:count - 1
                                                            totalCount:count];
  }
}

// Animates the visibility of the chip container.
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

// Determines the connector lines visibility based on the view's index and
// total count in the log.
- (ActuationWorklogConnectorVisibility)
    connectorVisibilityAtIndex:(NSInteger)index
                    totalCount:(NSInteger)count {
  BOOL isLast = (index == count - 1);
  if (index == 0) {
    return isLast ? ActuationWorklogConnectorVisibility::kNone
                  : ActuationWorklogConnectorVisibility::kBottom;
  }
  return isLast ? ActuationWorklogConnectorVisibility::kTop
                : ActuationWorklogConnectorVisibility::kBoth;
}

@end
