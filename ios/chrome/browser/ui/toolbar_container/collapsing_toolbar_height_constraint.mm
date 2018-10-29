// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/collapsing_toolbar_height_constraint.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#import "ios/chrome/browser/ui/toolbar_container/collapsing_toolbar_height_constraint_delegate.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_collapsing.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The progress range.
const CGFloat kMinProgress = 0.0;
const CGFloat kMaxProgress = 1.0;
}  // namespace

using toolbar_container::HeightRange;

@interface CollapsingToolbarHeightConstraint () {
  // Backing variable for property of same name.
  HeightRange _heightRange;
}
// The height values extracted from the constrained view.  If the view conforms
// to ToolbarCollapsing, these will be the values from that protocol, and will
// be updated using KVO if those values change.  Otherwise, they will both be
// equal to the intrinsic height of the view.
@property(nonatomic, readwrite) CGFloat collapsedToolbarHeight;
@property(nonatomic, readwrite) CGFloat expandedToolbarHeight;
// The collapsing toolbar whose height range is being observed.
@property(nonatomic, weak) UIView<ToolbarCollapsing>* collapsingToolbar;
@end

@implementation CollapsingToolbarHeightConstraint
@synthesize collapsedToolbarHeight = _collapsedToolbarHeight;
@synthesize expandedToolbarHeight = _expandedToolbarHeight;
@synthesize additionalHeight = _additionalHeight;
@synthesize collapsesAdditionalHeight = _collapsesAdditionalHeight;
@synthesize progress = _progress;
@synthesize delegate = _delegate;
@synthesize collapsingToolbar = _collapsingToolbar;

+ (instancetype)constraintWithView:(UIView*)view {
  DCHECK(view);
  CollapsingToolbarHeightConstraint* constraint =
      [[self class] constraintWithItem:view
                             attribute:NSLayoutAttributeHeight
                             relatedBy:NSLayoutRelationEqual
                                toItem:nil
                             attribute:NSLayoutAttributeNotAnAttribute
                            multiplier:0.0
                              constant:0.0];
  if ([view conformsToProtocol:@protocol(ToolbarCollapsing)]) {
    constraint.collapsingToolbar =
        static_cast<UIView<ToolbarCollapsing>*>(view);
  } else {
    CGFloat intrinsicHeight = view.intrinsicContentSize.height;
    constraint.collapsedToolbarHeight = intrinsicHeight;
    constraint.expandedToolbarHeight = intrinsicHeight;
    [constraint updateToolbarHeightRange];
  }
  constraint.progress = 1.0;

  return constraint;
}

#pragma mark - Accessors

- (void)setActive:(BOOL)active {
  [super setActive:active];
  if (self.active)
    [self startObservingCollapsingToolbar];
  else
    [self stopObservingCollapsingToolbar];
}

- (void)setAdditionalHeight:(CGFloat)additionalHeight {
  if (AreCGFloatsEqual(_additionalHeight, additionalHeight))
    return;
  _additionalHeight = additionalHeight;
  [self updateToolbarHeightRange];
}

- (void)setCollapsesAdditionalHeight:(BOOL)collapsesAdditionalHeight {
  if (_collapsesAdditionalHeight == collapsesAdditionalHeight)
    return;
  _collapsesAdditionalHeight = collapsesAdditionalHeight;
  [self updateToolbarHeightRange];
}

- (const HeightRange&)heightRange {
  // Custom getter is needed to support the C++ reference type.
  return _heightRange;
}

- (void)setProgress:(CGFloat)progress {
  progress = base::ClampToRange(progress, kMinProgress, kMaxProgress);
  if (AreCGFloatsEqual(_progress, progress))
    return;
  _progress = progress;
  [self updateHeightConstant];
}

- (void)setCollapsingToolbar:(UIView<ToolbarCollapsing>*)collapsingToolbar {
  if (_collapsingToolbar == collapsingToolbar)
    return;
  [self stopObservingCollapsingToolbar];
  _collapsingToolbar = collapsingToolbar;
  [self updateCollapsingToolbarHeights];
  if (self.active)
    [self startObservingCollapsingToolbar];
}

#pragma mark - Public

- (CGFloat)toolbarHeightForProgress:(CGFloat)progress {
  progress = base::ClampToRange(progress, kMinProgress, kMaxProgress);
  CGFloat base = self.collapsedToolbarHeight;
  CGFloat range = self.expandedToolbarHeight - self.collapsedToolbarHeight;
  if (self.collapsesAdditionalHeight) {
    range += self.additionalHeight;
  } else {
    base += self.additionalHeight;
  }
  return base + progress * range;
}

#pragma mark - KVO

- (void)observeValueForKeyPath:(NSString*)key
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  [self updateCollapsingToolbarHeights];
}

#pragma mark - KVO Helpers

- (NSArray<NSString*>* const)collapsingToolbarKeyPaths {
  static NSArray<NSString*>* const kKeyPaths =
      @[ @"expandedToolbarHeight", @"collapsedToolbarHeight" ];
  return kKeyPaths;
}

- (void)startObservingCollapsingToolbar {
  for (NSString* keyPath in [self collapsingToolbarKeyPaths]) {
    [self.collapsingToolbar addObserver:self
                             forKeyPath:keyPath
                                options:NSKeyValueObservingOptionNew
                                context:nullptr];
  }
}

- (void)stopObservingCollapsingToolbar {
  for (NSString* keyPath in [self collapsingToolbarKeyPaths]) {
    [self.collapsingToolbar removeObserver:self forKeyPath:keyPath];
  }
}

#pragma mark - Private

// Upates the collapsed and expanded heights from self.collapsingToolbar.
- (void)updateCollapsingToolbarHeights {
  self.collapsedToolbarHeight = self.collapsingToolbar.collapsedToolbarHeight;
  self.expandedToolbarHeight = self.collapsingToolbar.expandedToolbarHeight;
  [self updateToolbarHeightRange];
}

// Updates the height range using the current collapsing toolbar height values
// and additional height behavior.
- (void)updateToolbarHeightRange {
  HeightRange oldHeightRange = self.heightRange;
  CGFloat minHeight =
      self.collapsedToolbarHeight +
      (self.collapsesAdditionalHeight ? 0.0 : self.additionalHeight);
  CGFloat maxHeight = self.expandedToolbarHeight + self.additionalHeight;
  _heightRange = HeightRange(minHeight, maxHeight);
  if (_heightRange == oldHeightRange)
    return;
  [self updateHeightConstant];
  [self.delegate collapsingHeightConstraint:self
                   didUpdateFromHeightRange:oldHeightRange];
}

// Updates the constraint's constant
- (void)updateHeightConstant {
  self.constant = self.heightRange.GetInterpolatedHeight(self.progress);
}

@end
