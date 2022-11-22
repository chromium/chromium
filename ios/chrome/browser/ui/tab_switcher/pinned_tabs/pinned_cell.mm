// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_cell.h"

#import "ios/chrome/browser/ui/tab_switcher/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Default favicon image.
NSString* const kDefaultFaviconImage = @"default_world_favicon";

}  // namespace

@implementation PinnedCell

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.contentView.layer.cornerRadius = kPinnedCellCornerRadius;
    self.contentView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];

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

- (BOOL)hasIdentifier:(NSString*)identifier {
  return [self.itemIdentifier isEqualToString:identifier];
}

#pragma mark - Private

// Sets up the favicon view.
- (void)setupFaviconView {
  UIImage* favicon = [[UIImage imageNamed:kDefaultFaviconImage]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  _faviconView = [[UIImageView alloc] initWithImage:favicon];
  [self.contentView addSubview:_faviconView];

  _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [_faviconView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kPinnedCellHorizontalPadding],
    [_faviconView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],

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
        constraintEqualToAnchor:_faviconView.trailingAnchor
                       constant:kPinnedCellTitleLeadingPadding],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                 constant:-kPinnedCellHorizontalPadding],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:_faviconView.centerYAnchor],
  ]];
}

@end
