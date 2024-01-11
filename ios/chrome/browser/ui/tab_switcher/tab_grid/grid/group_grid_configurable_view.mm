// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_grid_configurable_view.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GroupGridConfigurableView {
  UIView* _topLeadingView;
  UIView* _topTrailingView;
  UIView* _bottomLeadingView;
  UIView* _bottomTrailingView;
}

- (instancetype)initWithSpacing:(CGFloat)spacing {
  self = [super initWithFrame:CGRectZero];

  if (self) {
    _topLeadingView = [[UIView alloc] init];
    _topTrailingView = [[UIView alloc] init];
    _bottomLeadingView = [[UIView alloc] init];
    _bottomTrailingView = [[UIView alloc] init];

    _topLeadingView.backgroundColor = [UIColor clearColor];
    _topTrailingView.backgroundColor = [UIColor clearColor];
    _bottomLeadingView.backgroundColor = [UIColor clearColor];
    _bottomTrailingView.backgroundColor = [UIColor clearColor];

    _topLeadingView.translatesAutoresizingMaskIntoConstraints = NO;
    _topTrailingView.translatesAutoresizingMaskIntoConstraints = NO;
    _bottomLeadingView.translatesAutoresizingMaskIntoConstraints = NO;
    _bottomTrailingView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_topLeadingView];
    [self addSubview:_topTrailingView];
    [self addSubview:_bottomLeadingView];
    [self addSubview:_bottomTrailingView];

    NSArray* constraints = @[
      [_topLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_topLeadingView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [_topTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_topTrailingView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [_bottomLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_bottomLeadingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],

      [_bottomTrailingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_bottomTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [_topTrailingView.leadingAnchor
          constraintEqualToAnchor:_topLeadingView.trailingAnchor
                         constant:spacing],
      [_topLeadingView.widthAnchor
          constraintEqualToAnchor:_topTrailingView.widthAnchor],

      [_bottomTrailingView.leadingAnchor
          constraintEqualToAnchor:_bottomLeadingView.trailingAnchor
                         constant:spacing],
      [_bottomLeadingView.widthAnchor
          constraintEqualToAnchor:_bottomTrailingView.widthAnchor],

      [_bottomLeadingView.topAnchor
          constraintEqualToAnchor:_topLeadingView.bottomAnchor
                         constant:spacing],
      [_bottomLeadingView.heightAnchor
          constraintEqualToAnchor:_topLeadingView.heightAnchor],

      [_bottomTrailingView.topAnchor
          constraintEqualToAnchor:_topTrailingView.bottomAnchor
                         constant:spacing],
      [_bottomTrailingView.heightAnchor
          constraintEqualToAnchor:_topTrailingView.heightAnchor],
    ];

    [NSLayoutConstraint activateConstraints:constraints];
  }

  return self;
}

- (void)updateTopLeadingWithView:(UIView*)view {
  [self updateView:_topLeadingView subview:view];
}

- (void)updateTopTrailingWithView:(UIView*)view {
  [self updateView:_topTrailingView subview:view];
}

- (void)updateBottomLeadingWithView:(UIView*)view {
  [self updateView:_bottomLeadingView subview:view];
}

- (void)updateBottomTrailingWithView:(UIView*)view {
  [self updateView:_bottomTrailingView subview:view];
}

- (void)setApplicableCornerRadius:(CGFloat)applicableCornerRadius {
  _applicableCornerRadius = applicableCornerRadius;
  _topLeadingView.layer.cornerRadius = _applicableCornerRadius;
  _topTrailingView.layer.cornerRadius = _applicableCornerRadius;
  _bottomLeadingView.layer.cornerRadius = _applicableCornerRadius;
  _bottomTrailingView.layer.cornerRadius = _applicableCornerRadius;
}

#pragma mark - Private

// Adds a given subview to a view and applying the same constraints to it.
- (void)updateView:(UIView*)view subview:(UIView*)subview {
  [view addSubview:subview];
  AddSameConstraints(view, subview);
}

@end
