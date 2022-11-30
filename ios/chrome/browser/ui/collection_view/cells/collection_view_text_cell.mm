// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kMargin = 16;
const CGFloat kMinimalHeight = 48;
}

@implementation CollectionViewTextCell

@synthesize textLabel = _textLabel;
@synthesize detailTextLabel = _detailTextLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UIView* containerView = [[UIView alloc] initWithFrame:CGRectZero];
    containerView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:containerView];

    _textLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [containerView addSubview:_textLabel];

    _detailTextLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailTextLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [containerView addSubview:_detailTextLabel];

    CGFloat margin = kMargin;

    [NSLayoutConstraint activateConstraints:@[
      // Total height.
      // The MDC specs ask for at least 48 pt.
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToConstant:kMinimalHeight],

      // Container.
      [containerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:margin],
      [containerView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-margin],
      [containerView.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                      constant:margin],
      [containerView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                   constant:-margin],
      [containerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],

      // Labels.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:containerView.leadingAnchor],
      [_textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:containerView.trailingAnchor],
      [_textLabel.topAnchor constraintEqualToAnchor:containerView.topAnchor],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:_detailTextLabel.topAnchor],
      [_detailTextLabel.leadingAnchor
          constraintEqualToAnchor:_textLabel.leadingAnchor],
      [_detailTextLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:containerView.trailingAnchor],
      [_detailTextLabel.bottomAnchor
          constraintLessThanOrEqualToAnchor:containerView.bottomAnchor],
    ]];
  }
  return self;
}

+ (CGFloat)heightForTitleLabel:(UILabel*)titleLabel
               detailTextLabel:(UILabel*)detailTextLabel
                         width:(CGFloat)width {
  CGSize sizeForLabel = CGSizeMake(width - 2 * kMargin, 500);

  CGFloat cellHeight = 2 * kMargin;
  cellHeight += [titleLabel sizeThatFits:sizeForLabel].height;
  cellHeight += [detailTextLabel sizeThatFits:sizeForLabel].height;

  return MAX(cellHeight, kMinimalHeight);
}

// Implement -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];
  // Adjust the text and detailText label preferredMaxLayoutWidth when the
  // parent's width changes, for instance on screen rotation.
  CGFloat preferedMaxLayoutWidth =
      CGRectGetWidth(self.contentView.frame) - 2 * kMargin;
  _textLabel.preferredMaxLayoutWidth = preferedMaxLayoutWidth;
  _detailTextLabel.preferredMaxLayoutWidth = preferedMaxLayoutWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.detailTextLabel.text = nil;
}

@end
