// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_favicon_grid.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kCornerRadius = 13;
const CGFloat kInnerSquircleCornerRadius = 10;
const CGFloat kSpacing = 4;
const CGFloat kImageSize = 16;
const CGFloat kImageContainerSize = 26;
const CGFloat kFaviconCornerRadius = 4;
const CGFloat kLabelFontSize = 11;

using LayoutSides::kBottom;
using LayoutSides::kLeading;
using LayoutSides::kTop;
using LayoutSides::kTrailing;

}  // namespace

@implementation TabGroupsPanelFaviconGrid {
  UIView* _innerSquircle;
  UIImageView* _imageView1;
  UIImageView* _imageView2;
  UIImageView* _imageView3;
  UIImageView* _imageView4;
  UILabel* _label;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;

    _innerSquircle = [[UIView alloc] init];
    _innerSquircle.backgroundColor = self.backgroundColor;
    _innerSquircle.layer.cornerRadius = kInnerSquircleCornerRadius;
    _innerSquircle.layer.masksToBounds = true;
    _innerSquircle.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_innerSquircle];

    _imageView1 = [self makeFaviconImageView];
    UIView* imageViewContainer1 = [self wrappedImageView:_imageView1];
    [_innerSquircle addSubview:imageViewContainer1];

    _imageView2 = [self makeFaviconImageView];
    UIView* imageViewContainer2 = [self wrappedImageView:_imageView2];
    [_innerSquircle addSubview:imageViewContainer2];

    _imageView3 = [self makeFaviconImageView];
    UIView* imageViewContainer3 = [self wrappedImageView:_imageView3];
    [_innerSquircle addSubview:imageViewContainer3];

    _imageView4 = [self makeFaviconImageView];
    UIView* imageViewContainer4 = [self wrappedImageView:_imageView4];
    [_innerSquircle addSubview:imageViewContainer4];

    _label = [[UILabel alloc] init];
    _label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _label.font = [UIFont systemFontOfSize:kLabelFontSize];
    _label.textAlignment = NSTextAlignmentCenter;
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    [_innerSquircle addSubview:_label];

    AddSameConstraintsWithInset(_innerSquircle, self, kSpacing);
    AddSameConstraintsToSides(imageViewContainer1, _innerSquircle,
                              kTop | kLeading);
    AddSameConstraintsToSides(imageViewContainer2, _innerSquircle,
                              kTop | kTrailing);
    AddSameConstraintsToSides(imageViewContainer3, _innerSquircle,
                              kBottom | kLeading);
    AddSameConstraintsToSides(imageViewContainer4, _innerSquircle,
                              kBottom | kTrailing);
    [NSLayoutConstraint activateConstraints:@[
      // Add the constraints between image view containers.
      [imageViewContainer2.leadingAnchor
          constraintEqualToAnchor:imageViewContainer1.trailingAnchor
                         constant:kSpacing],
      [imageViewContainer3.topAnchor
          constraintEqualToAnchor:imageViewContainer1.bottomAnchor
                         constant:kSpacing],
      [imageViewContainer4.leadingAnchor
          constraintEqualToAnchor:imageViewContainer3.trailingAnchor
                         constant:kSpacing],
    ]];
    AddSameConstraints(_label, imageViewContainer4);
  }
  return self;
}

- (void)setNumberOfTabs:(NSUInteger)numberOfTabs {
  _numberOfTabs = numberOfTabs;
  [self updateElements];
}

- (void)setFavicons:(NSArray<UIImage*>*)favicons {
  _favicons = [favicons copy];
  [self updateElements];
}

#pragma mark Private

// Makes a new rounded rectangle image view to display as part of the 2×2 grid.
- (UIImageView*)makeFaviconImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.tintColor = UIColor.whiteColor;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSquareConstraints(imageView, kImageSize);
  return imageView;
}

// Wraps the image view in a colored rounded rect.
- (UIView*)wrappedImageView:(UIImageView*)imageView {
  UIView* container = [[UIView alloc] init];
  container.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  container.layer.cornerRadius = kFaviconCornerRadius;
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:imageView];
  AddSquareConstraints(container, kImageContainerSize);
  AddSameCenterConstraints(imageView, container);
  return container;
}

// Reconfigures the 4 elements of the grid.
- (void)updateElements {
  // Reset subviews.
  _imageView1.image = nil;
  _imageView2.image = nil;
  _imageView3.image = nil;
  _imageView4.image = nil;
  _label.text = nil;
  _label.hidden = YES;

  const NSUInteger numberOfTabs = self.numberOfTabs;
  NSArray<UIImage*>* favicons = self.favicons;
  const NSUInteger numberOfFavicons = favicons.count;
  if (numberOfTabs > 0 && numberOfFavicons > 0) {
    _imageView1.image = favicons[0];
  }
  if (numberOfTabs > 1 && numberOfFavicons > 1) {
    _imageView2.image = favicons[1];
  }
  if (numberOfTabs > 2 && numberOfFavicons > 2) {
    _imageView3.image = favicons[2];
  }
  if (numberOfTabs == 4 && numberOfFavicons > 3) {
    _imageView4.image = favicons[3];
  } else if (numberOfTabs > 4) {
    _label.hidden = NO;
    const NSInteger overFlowCount = numberOfTabs - 3;
    _label.text = overFlowCount > 99
                      ? @"99+"
                      : [NSString stringWithFormat:@"+%@", @(overFlowCount)];
  }
}

@end
