// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_group_stroke_view.h"

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"

@implementation TabStripGroupStrokeView {
  CAShapeLayer* _groupStrokeLeadingLayer;
  CAShapeLayer* _groupStrokeTrailingLayer;
}

#pragma mark - Initialization

- (instancetype)init {
  self = [super init];
  if (self) {
    // Setup leading path view.
    _groupStrokeLeadingLayer = [CAShapeLayer layer];
    _groupStrokeLeadingLayer.fillColor = UIColor.clearColor.CGColor;
    _groupStrokeLeadingLayer.lineWidth =
        TabStripCollectionViewConstants.groupStrokeLineWidth;
    UIView* groupStrokeLeadingView = [[UIView alloc] init];
    groupStrokeLeadingView.translatesAutoresizingMaskIntoConstraints = NO;
    [groupStrokeLeadingView.layer addSublayer:_groupStrokeLeadingLayer];
    [NSLayoutConstraint activateConstraints:@[
      [groupStrokeLeadingView.widthAnchor constraintEqualToConstant:0],
      [groupStrokeLeadingView.heightAnchor constraintEqualToConstant:0],
    ]];

    // Setup trailing path view.
    _groupStrokeTrailingLayer = [CAShapeLayer layer];
    _groupStrokeTrailingLayer.fillColor = UIColor.clearColor.CGColor;
    _groupStrokeTrailingLayer.lineWidth =
        TabStripCollectionViewConstants.groupStrokeLineWidth;
    UIView* groupStrokeTrailingView = [[UIView alloc] init];
    groupStrokeTrailingView.translatesAutoresizingMaskIntoConstraints = NO;
    [groupStrokeTrailingView.layer addSublayer:_groupStrokeTrailingLayer];
    [NSLayoutConstraint activateConstraints:@[
      [groupStrokeTrailingView.widthAnchor constraintEqualToConstant:0],
      [groupStrokeTrailingView.heightAnchor constraintEqualToConstant:0],
    ]];

    // Set up self.
    self.translatesAutoresizingMaskIntoConstraints = NO;
    [self.heightAnchor constraintEqualToConstant:TabStripCollectionViewConstants
                                                     .groupStrokeLineWidth]
        .active = YES;
    [self addSubview:groupStrokeLeadingView];
    [self addSubview:groupStrokeTrailingView];

    [NSLayoutConstraint activateConstraints:@[
      [groupStrokeLeadingView.centerXAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [groupStrokeLeadingView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [groupStrokeTrailingView.centerXAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [groupStrokeTrailingView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - UIView

- (void)setBackgroundColor:(UIColor*)color {
  [super setBackgroundColor:color];
  _groupStrokeLeadingLayer.strokeColor = color.CGColor;
  _groupStrokeTrailingLayer.strokeColor = color.CGColor;
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  _groupStrokeLeadingLayer.strokeColor = self.backgroundColor.CGColor;
  _groupStrokeTrailingLayer.strokeColor = self.backgroundColor.CGColor;
}

#pragma mark - Public

- (void)setLeadingPath:(CGPathRef)path {
  if (UseRTLLayout()) {
    const auto flipHorizontally = CGAffineTransformMakeScale(-1, 1);
    path = CGPathCreateCopyByTransformingPath(path, &flipHorizontally);
  }
  _groupStrokeLeadingLayer.path = path;
}

- (void)setTrailingPath:(CGPathRef)path {
  if (UseRTLLayout()) {
    const auto flipHorizontally = CGAffineTransformMakeScale(-1, 1);
    path = CGPathCreateCopyByTransformingPath(path, &flipHorizontally);
  }
  _groupStrokeTrailingLayer.path = path;
}

@end
