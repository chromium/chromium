// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_group_stroke_view.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"

@implementation TabStripGroupStrokeView {
  CAShapeLayer* _groupStrokeLeftLayer;
  CAShapeLayer* _groupStrokeRightLayer;
}

#pragma mark - Initialization

- (instancetype)init {
  self = [super init];
  if (self) {
    // Setup left path view.
    _groupStrokeLeftLayer = [CAShapeLayer layer];
    _groupStrokeLeftLayer.fillColor = UIColor.clearColor.CGColor;
    _groupStrokeLeftLayer.lineWidth =
        TabStripCollectionViewConstants.groupStrokeLineWidth;
    UIView* groupStrokeLeftView = [[UIView alloc] init];
    groupStrokeLeftView.translatesAutoresizingMaskIntoConstraints = NO;
    [groupStrokeLeftView.layer addSublayer:_groupStrokeLeftLayer];
    [NSLayoutConstraint activateConstraints:@[
      [groupStrokeLeftView.widthAnchor constraintEqualToConstant:0],
      [groupStrokeLeftView.heightAnchor constraintEqualToConstant:0],
    ]];

    // Setup right path view.
    _groupStrokeRightLayer = [CAShapeLayer layer];
    _groupStrokeRightLayer.fillColor = UIColor.clearColor.CGColor;
    _groupStrokeRightLayer.lineWidth =
        TabStripCollectionViewConstants.groupStrokeLineWidth;
    UIView* groupStrokeRightView = [[UIView alloc] init];
    groupStrokeRightView.translatesAutoresizingMaskIntoConstraints = NO;
    [groupStrokeRightView.layer addSublayer:_groupStrokeRightLayer];
    [NSLayoutConstraint activateConstraints:@[
      [groupStrokeRightView.widthAnchor constraintEqualToConstant:0],
      [groupStrokeRightView.heightAnchor constraintEqualToConstant:0],
    ]];

    // Set up self.
    self.translatesAutoresizingMaskIntoConstraints = NO;
    [self.heightAnchor constraintEqualToConstant:TabStripCollectionViewConstants
                                                     .groupStrokeLineWidth]
        .active = YES;
    [self addSubview:groupStrokeLeftView];
    [self addSubview:groupStrokeRightView];

    [NSLayoutConstraint activateConstraints:@[
      [groupStrokeLeftView.centerXAnchor
          constraintEqualToAnchor:self.leftAnchor],
      [groupStrokeLeftView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
      [groupStrokeRightView.centerXAnchor
          constraintEqualToAnchor:self.rightAnchor],
      [groupStrokeRightView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

#pragma mark - UIView

- (void)setBackgroundColor:(UIColor*)color {
  [super setBackgroundColor:color];
  _groupStrokeLeftLayer.strokeColor = color.CGColor;
  _groupStrokeRightLayer.strokeColor = color.CGColor;
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  _groupStrokeLeftLayer.strokeColor = self.backgroundColor.CGColor;
  _groupStrokeRightLayer.strokeColor = self.backgroundColor.CGColor;
}

#pragma mark - Public

- (void)setLeftPath:(CGPathRef)path {
  _groupStrokeLeftLayer.path = path;
}

- (void)setRightPath:(CGPathRef)path {
  _groupStrokeRightLayer.path = path;
}

@end
