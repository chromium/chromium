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

NSString* const kTabGroupFaviconsGridBackgroundColor =
    @"tab_group_favicons_grid_background_color";

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

  UIView* _imageContainerView1;
  UIView* _imageContainerView2;
  UIView* _imageContainerView3;
  UIView* _imageContainerView4;

  UIImageView* _imageView1;
  UIImageView* _imageView2;
  UIImageView* _imageView3;
  UIImageView* _imageView4;

  UILabel* _label;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor =
        [UIColor colorNamed:kTabGroupFaviconsGridBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;

    _innerSquircle = [[UIView alloc] init];
    _innerSquircle.backgroundColor = self.backgroundColor;
    _innerSquircle.layer.cornerRadius = kInnerSquircleCornerRadius;
    _innerSquircle.layer.masksToBounds = true;
    _innerSquircle.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_innerSquircle];

    _imageView1 = [self makeFaviconImageView];
    _imageContainerView1 = [self wrappedImageView:_imageView1];
    [_innerSquircle addSubview:_imageContainerView1];

    _imageView2 = [self makeFaviconImageView];
    _imageContainerView2 = [self wrappedImageView:_imageView2];
    [_innerSquircle addSubview:_imageContainerView2];

    _imageView3 = [self makeFaviconImageView];
    _imageContainerView3 = [self wrappedImageView:_imageView3];
    [_innerSquircle addSubview:_imageContainerView3];

    _imageView4 = [self makeFaviconImageView];
    _imageContainerView4 = [self wrappedImageView:_imageView4];
    [_innerSquircle addSubview:_imageContainerView4];

    _label = [[UILabel alloc] init];
    _label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _label.font = [UIFont systemFontOfSize:kLabelFontSize];
    _label.textAlignment = NSTextAlignmentCenter;
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    [_innerSquircle addSubview:_label];

    AddSameConstraintsWithInset(_innerSquircle, self, kSpacing);
    AddSameConstraintsToSides(_imageContainerView1, _innerSquircle,
                              kTop | kLeading);
    AddSameConstraintsToSides(_imageContainerView2, _innerSquircle,
                              kTop | kTrailing);
    AddSameConstraintsToSides(_imageContainerView3, _innerSquircle,
                              kBottom | kLeading);
    AddSameConstraintsToSides(_imageContainerView4, _innerSquircle,
                              kBottom | kTrailing);
    [NSLayoutConstraint activateConstraints:@[
      // Add the constraints between image view containers.
      [_imageContainerView2.leadingAnchor
          constraintEqualToAnchor:_imageContainerView1.trailingAnchor
                         constant:kSpacing],
      [_imageContainerView4.topAnchor
          constraintEqualToAnchor:_imageContainerView1.bottomAnchor
                         constant:kSpacing],
      [_imageContainerView4.leadingAnchor
          constraintEqualToAnchor:_imageContainerView3.trailingAnchor
                         constant:kSpacing],
    ]];
    AddSameConstraints(_label, _imageContainerView4);
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
  [self updateContainerViewBackgroundColor:_imageContainerView1
                                     empty:favicon1 == nil];
}

- (UIImage*)favicon2 {
  return _imageView2.image;
}

- (void)setFavicon2:(UIImage*)favicon2 {
  _imageView2.image = favicon2;
  [self updateContainerViewBackgroundColor:_imageContainerView2
                                     empty:favicon2 == nil];
}

- (UIImage*)favicon3 {
  return _imageView3.image;
}

- (void)setFavicon3:(UIImage*)favicon3 {
  _imageView3.image = favicon3;
  [self updateContainerViewBackgroundColor:_imageContainerView3
                                     empty:favicon3 == nil];
}

- (UIImage*)favicon4 {
  return _imageView4.image;
}

- (void)setFavicon4:(UIImage*)favicon4 {
  _imageView4.image = favicon4;

  // If there are more than 4 tabs, the `_imageContainerView4` will display the
  // total number of saved tabs.
  BOOL empty = (favicon4 == nil) && (_numberOfTabs < 5);
  [self updateContainerViewBackgroundColor:_imageContainerView4 empty:empty];
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
  container.backgroundColor =
      [UIColor colorNamed:kTabGroupFaviconsGridBackgroundColor];
  container.layer.cornerRadius = kFaviconCornerRadius;
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:imageView];
  AddSquareConstraints(container, kImageContainerSize);
  AddSameCenterConstraints(imageView, container);
  return container;
}

// Updates the `containerView` background color according to the `empty` state.
- (void)updateContainerViewBackgroundColor:(UIView*)containerView
                                     empty:(BOOL)empty {
  containerView.backgroundColor =
      empty ? [UIColor colorNamed:kTabGroupFaviconsGridBackgroundColor]
            : [UIColor colorNamed:kPrimaryBackgroundColor];
}

@end
