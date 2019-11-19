// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/cells/page_info_item.h"

#import "ios/chrome/browser/ui/payments/cells/accessibility_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPageInfoFaviconImageViewID = @"kPageInfoFaviconImageViewID";
NSString* const kPageInfoLockIndicatorImageViewID =
    @"kPageInfoLockIndicatorImageViewID";

namespace {
// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 10;

// Vertical spacing between the labels.
const CGFloat kLabelsVerticalSpacing = 2;

// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Horizontal spacing between the favicon and the labels.
const CGFloat kFaviconAndLabelsHorizontalSpacing = 12;

// Dimension for the favicon in points.
const CGFloat kFaviconDimension = 16;
}

@implementation PageInfoItem

@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;
@synthesize connectionSecure = _connectionSecure;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PageInfoCell class];
  }
  return self;
}

- (void)configureCell:(PageInfoCell*)cell {
  [super configureCell:cell];
  cell.pageFaviconView.image = self.pageFavicon;
  cell.pageTitleLabel.text = self.pageTitle;
  cell.pageHostLabel.text = self.pageHost;
  cell.pageLockIndicatorView.image = nil;

  if (self.connectionSecure) {
    NSMutableAttributedString* text = [[NSMutableAttributedString alloc]
        initWithString:cell.pageHostLabel.text];
    [text addAttribute:NSForegroundColorAttributeName
                 value:[UIColor colorNamed:kGreenColor]
                 range:NSMakeRange(0, strlen(url::kHttpsScheme))];
    // We need to set the font to the attributed portion, or the field doesn't
    // asjust with dynamic types.
    [text addAttribute:NSFontAttributeName
                 value:[[UIFontMetrics defaultMetrics]
                           scaledFontForFont:[[MDCTypography fontLoader]
                                                 regularFontOfSize:12]]
                 range:NSMakeRange(0, strlen(url::kHttpsScheme))];
    [cell.pageHostLabel setAttributedText:text];

    // Set lock image. UIImageRenderingModeAlwaysTemplate is used so that
    // the color of the lock indicator image can be changed to green.
    cell.pageLockIndicatorView.image = [NativeImage(IDR_IOS_OMNIBOX_HTTPS_VALID)
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }

  // Invalidate the constraints so that layout can account for whether or not a
  // favicon is present.
  [cell updateConstraintsIfNeeded];
}

@end

@implementation PageInfoCell {
  NSLayoutConstraint* _pageTitleLabelLeadingConstraint;
  NSLayoutConstraint* _pageHostLabelLeadingConstraint;
}

@synthesize pageTitleLabel = _pageTitleLabel;
@synthesize pageHostLabel = _pageHostLabel;
@synthesize pageFaviconView = _pageFaviconView;
@synthesize pageLockIndicatorView = _pageLockIndicatorView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;
    self.contentView.clipsToBounds = YES;

    // Favicon
    _pageFaviconView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _pageFaviconView.translatesAutoresizingMaskIntoConstraints = NO;
    _pageFaviconView.accessibilityIdentifier = kPageInfoFaviconImageViewID;
    [self.contentView addSubview:_pageFaviconView];

    // Page title
    _pageTitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    SetUILabelScaledFont(_pageTitleLabel,
                         [[MDCTypography fontLoader] mediumFontOfSize:12]);
    _pageTitleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _pageTitleLabel.backgroundColor = [UIColor clearColor];
    _pageTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_pageTitleLabel];

    // Page host
    _pageHostLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    SetUILabelScaledFont(_pageHostLabel,
                         [[MDCTypography fontLoader] regularFontOfSize:12]);
    _pageHostLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    // Truncate host name from the leading side if it is too long. This is
    // according to Eliding Origin Names and Hostnames guideline found here:
    // https://www.chromium.org/Home/chromium-security/enamel#TOC-Presenting-Origins
    _pageHostLabel.lineBreakMode = NSLineBreakByTruncatingHead;
    _pageHostLabel.backgroundColor = [UIColor clearColor];
    _pageHostLabel.translatesAutoresizingMaskIntoConstraints = NO;
    // Prevents host label from bleeding into lock indicator view when host text
    // is very long.
    [_pageHostLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [self.contentView addSubview:_pageHostLabel];

    // Lock indicator
    _pageLockIndicatorView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _pageLockIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
    _pageLockIndicatorView.accessibilityIdentifier =
        kPageInfoLockIndicatorImageViewID;
    _pageLockIndicatorView.contentMode = UIViewContentModeScaleAspectFit;
    [_pageLockIndicatorView setTintColor:[UIColor colorNamed:kGreenColor]];
    [self.contentView addSubview:_pageLockIndicatorView];

    // Layout
    [NSLayoutConstraint activateConstraints:@[
      [_pageFaviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_pageFaviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_pageFaviconView.heightAnchor
          constraintEqualToConstant:kFaviconDimension],
      [_pageFaviconView.widthAnchor
          constraintEqualToAnchor:_pageFaviconView.heightAnchor],

      // The constraint on the leading anchor of the lock indicator is
      // activated in updateConstraints rather than here so that it can
      // depend on whether a favicon is present or not.
      [_pageLockIndicatorView.leadingAnchor
          constraintEqualToAnchor:_pageTitleLabel.leadingAnchor],
      [_pageLockIndicatorView.bottomAnchor
          constraintEqualToAnchor:_pageHostLabel.bottomAnchor],
      [_pageLockIndicatorView.topAnchor
          constraintEqualToAnchor:_pageHostLabel.topAnchor],
      [_pageLockIndicatorView.widthAnchor
          constraintEqualToAnchor:_pageHostLabel.heightAnchor],

      [_pageTitleLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kVerticalPadding],
      [_pageTitleLabel.bottomAnchor
          constraintEqualToAnchor:_pageHostLabel.topAnchor
                         constant:-kLabelsVerticalSpacing],
      [_pageHostLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],

      [_pageTitleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                   constant:-kHorizontalPadding],
      [_pageHostLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                   constant:-kHorizontalPadding],
    ]];
  }
  return self;
}

#pragma mark - UIView

- (void)updateConstraints {
  _pageTitleLabelLeadingConstraint.active = NO;
  _pageTitleLabelLeadingConstraint =
      _pageFaviconView.image
          ? [_pageTitleLabel.leadingAnchor
                constraintEqualToAnchor:_pageFaviconView.trailingAnchor
                               constant:kFaviconAndLabelsHorizontalSpacing]
          : [_pageTitleLabel.leadingAnchor
                constraintEqualToAnchor:self.contentView.leadingAnchor
                               constant:kHorizontalPadding];
  _pageTitleLabelLeadingConstraint.active = YES;

  _pageHostLabelLeadingConstraint.active = NO;
  _pageHostLabelLeadingConstraint = [_pageHostLabel.leadingAnchor
      constraintEqualToAnchor:_pageLockIndicatorView.image
                                  ? _pageLockIndicatorView.trailingAnchor
                                  : _pageTitleLabel.leadingAnchor];
  _pageHostLabelLeadingConstraint.active = YES;

  [super updateConstraints];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  _pageFaviconView.image = nil;
  _pageTitleLabel.text = nil;
  _pageHostLabel.text = nil;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  AccessibilityLabelBuilder* builder = [[AccessibilityLabelBuilder alloc] init];
  [builder appendItem:self.pageTitleLabel.text];
  [builder appendItem:self.pageHostLabel.text];
  return [builder buildAccessibilityLabel];
}

@end
