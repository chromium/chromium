// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_cell.h"

#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PinnedCell {
  // Container for the `_faviconView`.
  UIView* _faviconContainerView;
  // View for displaying the favicon.
  UIImageView* _faviconView;
  // Title is displayed by this label.
  UILabel* _titleLabel;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.contentView.layer.cornerRadius = kPinnedCellCornerRadius;
    self.contentView.backgroundColor =
        [UIColor colorNamed:kSecondaryBackgroundColor];

    [self setupSelectedBackgroundView];
    [self setupFaviconContainerView];
    [self setupFaviconView];
    [self setupTitleLabel];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.itemIdentifier = nil;
  self.icon = nil;
  self.title = nil;
}

#pragma mark - Public

- (UIImage*)icon {
  return _faviconView.image;
}

- (void)setIcon:(UIImage*)icon {
  _faviconView.image = icon;
}

- (NSString*)title {
  return _titleLabel.text;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = [title copy];
  self.accessibilityLabel = [title copy];
}

- (UIDragPreviewParameters*)dragPreviewParameters {
  UIBezierPath* visiblePath = [UIBezierPath
      bezierPathWithRoundedRect:self.bounds
                   cornerRadius:self.contentView.layer.cornerRadius];
  UIDragPreviewParameters* params = [[UIDragPreviewParameters alloc] init];
  params.visiblePath = visiblePath;
  return params;
}

#pragma mark - Private

// Sets up the selection border.
- (void)setupSelectedBackgroundView {
  UIView* selectedBackgroundBorderView = [[UIView alloc] init];
  selectedBackgroundBorderView.translatesAutoresizingMaskIntoConstraints = NO;
  selectedBackgroundBorderView.layer.cornerRadius =
      kPinnedCellCornerRadius + kPinnedCellSelectionRingGapWidth +
      kPinnedCellSelectionRingTintWidth;
  selectedBackgroundBorderView.layer.borderWidth =
      kPinnedCellSelectionRingTintWidth;
  selectedBackgroundBorderView.layer.borderColor =
      UseSymbols()
          ? [UIColor colorNamed:kStaticBlue400Color].CGColor
          : [UIColor colorNamed:@"grid_theme_selection_tint_color"].CGColor;

  UIView* selectedBackgroundView = [[UIView alloc] init];
  [selectedBackgroundView addSubview:selectedBackgroundBorderView];

  [NSLayoutConstraint activateConstraints:@[
    [selectedBackgroundBorderView.topAnchor
        constraintEqualToAnchor:selectedBackgroundView.topAnchor
                       constant:-kPinnedCellSelectionRingPadding],
    [selectedBackgroundBorderView.leadingAnchor
        constraintEqualToAnchor:selectedBackgroundView.leadingAnchor
                       constant:-kPinnedCellSelectionRingPadding],
    [selectedBackgroundBorderView.trailingAnchor
        constraintEqualToAnchor:selectedBackgroundView.trailingAnchor
                       constant:kPinnedCellSelectionRingPadding],
    [selectedBackgroundBorderView.bottomAnchor
        constraintEqualToAnchor:selectedBackgroundView.bottomAnchor
                       constant:kPinnedCellSelectionRingPadding]
  ]];

  self.selectedBackgroundView = selectedBackgroundView;
}

// Sets up the `_faviconContainerView` view.
- (void)setupFaviconContainerView {
  UIView* faviconContainerView = [[UIView alloc] init];
  [self.contentView addSubview:faviconContainerView];

  faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconContainerView.backgroundColor = UIColor.whiteColor;

  faviconContainerView.layer.borderColor =
      [UIColor colorNamed:kSeparatorColor].CGColor;
  faviconContainerView.layer.borderWidth = kPinnedCellFaviconBorderWidth;
  faviconContainerView.layer.cornerRadius =
      kPinnedCellFaviconContainerCornerRadius;
  faviconContainerView.layer.masksToBounds = YES;

  [NSLayoutConstraint activateConstraints:@[
    [faviconContainerView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kPinnedCellHorizontalPadding],
    [faviconContainerView.widthAnchor
        constraintEqualToConstant:kPinnedCellFaviconContainerWidth],
    [faviconContainerView.heightAnchor
        constraintEqualToAnchor:faviconContainerView.widthAnchor],
    [faviconContainerView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor]
  ]];

  _faviconContainerView = faviconContainerView;
}

// Sets up the `_faviconView` view.
- (void)setupFaviconView {
  UIImageView* faviconView = [[UIImageView alloc] init];
  [_faviconContainerView addSubview:faviconView];

  faviconView.layer.cornerRadius = kPinnedCellFaviconCornerRadius;
  faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  faviconView.contentMode = UIViewContentModeScaleAspectFit;
  faviconView.clipsToBounds = YES;

  [NSLayoutConstraint activateConstraints:@[
    [faviconView.widthAnchor constraintEqualToConstant:kPinnedCellFaviconWidth],
    [faviconView.heightAnchor constraintEqualToAnchor:faviconView.widthAnchor],
    [faviconView.centerYAnchor
        constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
    [faviconView.centerXAnchor
        constraintEqualToAnchor:_faviconContainerView.centerXAnchor],
  ]];

  _faviconView = faviconView;
}

// Sets up the title label.
- (void)setupTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  [self.contentView addSubview:titleLabel];

  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [titleLabel.leadingAnchor
        constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                       constant:kPinnedCellTitleLeadingPadding],
    [titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                 constant:-kPinnedCellHorizontalPadding],
    [titleLabel.centerYAnchor
        constraintEqualToAnchor:_faviconContainerView.centerYAnchor],
  ]];

  _titleLabel = titleLabel;
}

@end
