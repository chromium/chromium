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
const CGFloat kImageSize = 26;
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
    [_innerSquircle addSubview:_imageView1];

    _imageView2 = [self makeFaviconImageView];
    [_innerSquircle addSubview:_imageView2];

    _imageView3 = [self makeFaviconImageView];
    [_innerSquircle addSubview:_imageView3];

    _imageView4 = [self makeFaviconImageView];
    [_innerSquircle addSubview:_imageView4];

    _label = [[UILabel alloc] init];
    _label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _label.font = [UIFont systemFontOfSize:kLabelFontSize];
    _label.textAlignment = NSTextAlignmentCenter;
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    [_innerSquircle addSubview:_label];

    AddSameConstraintsWithInset(_innerSquircle, self, kSpacing);
    AddSquareConstraints(_imageView1, kImageSize);
    AddSquareConstraints(_imageView2, kImageSize);
    AddSquareConstraints(_imageView3, kImageSize);
    AddSquareConstraints(_imageView4, kImageSize);
    AddSameConstraintsToSides(_imageView1, _innerSquircle, kTop | kLeading);
    AddSameConstraintsToSides(_imageView2, _innerSquircle, kTop | kTrailing);
    AddSameConstraintsToSides(_imageView3, _innerSquircle, kBottom | kLeading);
    AddSameConstraintsToSides(_imageView4, _innerSquircle, kBottom | kTrailing);
    [NSLayoutConstraint activateConstraints:@[
      // Add the constraints between image views.
      [_imageView2.leadingAnchor
          constraintEqualToAnchor:_imageView1.trailingAnchor
                         constant:kSpacing],
      [_imageView3.topAnchor constraintEqualToAnchor:_imageView1.bottomAnchor
                                            constant:kSpacing],
      [_imageView4.leadingAnchor
          constraintEqualToAnchor:_imageView3.trailingAnchor
                         constant:kSpacing],
    ]];
    AddSameConstraints(_label, _imageView4);
  }
  return self;
}

- (void)setFavicons:(NSArray<UIImage*>*)favicons {
  _favicons = [favicons copy];

  // Reset subviews.
  _imageView1.image = nil;
  _imageView2.image = nil;
  _imageView3.image = nil;
  _imageView4.image = nil;
  _label.text = nil;

  const NSInteger count = favicons.count;
  if (count > 0) {
    _imageView1.image = favicons[0];
    if (count > 1) {
      _imageView2.image = favicons[1];
      if (count > 2) {
        _imageView3.image = favicons[2];
        if (count == 4) {
          _imageView4.image = favicons[3];
          _label.hidden = YES;
        } else if (count > 4) {
          _label.hidden = NO;
          const NSInteger overFlowCount = count - 3;
          _label.text =
              overFlowCount > 99
                  ? @"99+"
                  : [NSString stringWithFormat:@"+%@", @(overFlowCount)];
        }
      }
    }
  }
}

#pragma mark Private

// Makes a new rounded rectangle image view to display as part of the 2×2 grid.
- (UIImageView*)makeFaviconImageView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  imageView.layer.cornerRadius = kFaviconCornerRadius;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

@end
