// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_cell.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Default favicon image.
NSString* const kDefaultFaviconImage = @"default_world_favicon";

}  // namespace

@implementation PinnedCell {
  // Container for the `_faviconView`.
  UIView* _faviconContainerView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.contentView.layer.cornerRadius = kPinnedCellCornerRadius;
    self.contentView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];

    _faviconContainerView = [[UIView alloc] init];
    [self.contentView addSubview:_faviconContainerView];

    UIImage* favicon = [[UIImage imageNamed:kDefaultFaviconImage]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _faviconView = [[UIImageView alloc] initWithImage:favicon];
    [_faviconContainerView addSubview:_faviconView];

    [self setupFaviconContainerView];
    [self setupFaviconView];
    [self setupTitleLabel];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.itemIdentifier = nil;
  self.faviconView = nil;
}

#pragma mark - Public

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath = [UIBezierPath
      bezierPathWithRoundedRect:self.bounds
                   cornerRadius:self.contentView.layer.cornerRadius];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - Private

// Sets up the `_faviconContainerView` view.
- (void)setupFaviconContainerView {
  _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  _faviconContainerView.backgroundColor = UIColor.whiteColor;

  _faviconContainerView.layer.borderColor =
      [UIColor colorNamed:kSeparatorColor].CGColor;
  _faviconContainerView.layer.borderWidth = kPinnedCellFaviconBorderWidth;
  _faviconContainerView.layer.cornerRadius =
      kPinnedCellFaviconContainerCornerRadius;
  _faviconContainerView.layer.masksToBounds = YES;

  [NSLayoutConstraint activateConstraints:@[
    [_faviconContainerView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kPinnedCellHorizontalPadding],
    [_faviconContainerView.widthAnchor
        constraintEqualToConstant:kPinnedCellFaviconContainerWidth],
    [_faviconContainerView.heightAnchor
        constraintEqualToAnchor:_faviconContainerView.widthAnchor],
    [_faviconContainerView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor]
  ]];
}

// Sets up the `_faviconView` view.
- (void)setupFaviconView {
  _faviconView.layer.cornerRadius = kPinnedCellFaviconCornerRadius;
  _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  _faviconView.contentMode = UIViewContentModeScaleAspectFit;
  _faviconView.clipsToBounds = YES;

  [NSLayoutConstraint activateConstraints:@[
    [_faviconView.widthAnchor
        constraintEqualToConstant:kPinnedCellFaviconWidth],
    [_faviconView.heightAnchor
        constraintEqualToAnchor:_faviconView.widthAnchor],
    [_faviconView.centerYAnchor
        constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
    [_faviconView.centerXAnchor
        constraintEqualToAnchor:_faviconContainerView.centerXAnchor],
  ]];
}

// Sets up the title label.
- (void)setupTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  [self.contentView addSubview:_titleLabel];

  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                       constant:kPinnedCellTitleLeadingPadding],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                 constant:-kPinnedCellHorizontalPadding],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
  ]];
}

@end
