// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"

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

NSString* const kGridBackgroundColor = @"grid_background_color";

using LayoutSides::kBottom;
using LayoutSides::kLeading;
using LayoutSides::kTop;
using LayoutSides::kTrailing;

}  // namespace

// An opaque pointer to track an in-progress configuration.
// Each time a favicons grid is configured, such a token is created.
// In asynchronous callbacks, keep a weak reference to it to make sure
// the favicons grid is still waiting to be configured for the initial
// call. If the weak reference has been nilled out, then avoid configuring
// it. A new call to configure it has been made in the meantime (for
// example cell reuse).
@interface TabGroupFaviconsGridConfigurationToken : NSObject
@end

@implementation TabGroupFaviconsGridConfigurationToken
@end

@implementation TabGroupFaviconsGrid {
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

- (void)resetFavicons {
  self.favicon1 = nil;
  self.favicon2 = nil;
  self.favicon3 = nil;
  self.favicon4 = nil;
  _configurationToken = [[TabGroupFaviconsGridConfigurationToken alloc] init];
}

- (void)setNumberOfTabs:(NSUInteger)numberOfTabs {
  _numberOfTabs = numberOfTabs;

  if (numberOfTabs > 4) {
    _label.hidden = NO;
    const NSInteger overFlowCount = numberOfTabs - 3;
    _label.text = overFlowCount > 99
                      ? @"99+"
                      : [NSString stringWithFormat:@"+%@", @(overFlowCount)];
  } else {
    _label.hidden = YES;
    _label.text = nil;
  }
}

- (UIImage*)favicon1 {
  return _imageView1.image;
}

- (void)setFavicon1:(UIImage*)favicon1 {
  _imageView1.image = favicon1;
}

- (UIImage*)favicon2 {
  return _imageView2.image;
}

- (void)setFavicon2:(UIImage*)favicon2 {
  _imageView2.image = favicon2;
}

- (UIImage*)favicon3 {
  return _imageView3.image;
}

- (void)setFavicon3:(UIImage*)favicon3 {
  _imageView3.image = favicon3;
}

- (UIImage*)favicon4 {
  return _imageView4.image;
}

- (void)setFavicon4:(UIImage*)favicon4 {
  _imageView4.image = favicon4;
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

@end
