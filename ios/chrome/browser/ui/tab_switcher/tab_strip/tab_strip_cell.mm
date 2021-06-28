// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"

#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Tab close button insets.
const CGFloat kTabBackgroundLeftCapInset = 34.0;
const CGFloat kFaviconInset = 28;
const CGFloat kTitleInset = 10.0;
const CGFloat kFontSize = 14.0;
}  // namespace

@implementation TabStripCell

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setupBackgroundViews];

    UIImage* favicon = [[UIImage imageNamed:@"default_world_favicon"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _faviconView = [[UIImageView alloc] initWithImage:favicon];
    [self.contentView addSubview:_faviconView];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kFaviconInset],
      [_faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    UIImage* close = [[UIImage imageNamed:@"grid_cell_close_button"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [_closeButton setImage:close forState:UIControlStateNormal];
    [self.contentView addSubview:_closeButton];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_closeButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kFaviconInset],
      [_closeButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
    [_closeButton addTarget:self
                     action:@selector(closeButtonTapped:)
           forControlEvents:UIControlEventTouchUpInside];

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.font = [UIFont systemFontOfSize:kFontSize
                                         weight:UIFontWeightMedium];
    [self.contentView addSubview:_titleLabel];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:_faviconView.trailingAnchor
                         constant:kTitleInset],
      [_titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_closeButton.leadingAnchor
                                   constant:-kTitleInset],
      [_titleLabel.centerYAnchor
          constraintEqualToAnchor:_faviconView.centerYAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.itemIdentifier = nil;
  self.selected = NO;
  self.faviconView = nil;
}

- (void)setupBackgroundViews {
  self.backgroundView = [self resizeableBackgroundImageForStateSelected:NO];
  self.selectedBackgroundView =
      [self resizeableBackgroundImageForStateSelected:YES];
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self setupBackgroundViews];
}

#pragma mark - Private

// Updates this tab's style based on the value of |selected| and the current
// incognito style.
- (UIView*)resizeableBackgroundImageForStateSelected:(BOOL)selected {
  // Style the background image first.
  NSString* state = (selected ? @"foreground" : @"background");
  NSString* imageName = [NSString stringWithFormat:@"tabstrip_%@_tab", state];

  // As of iOS 13 Beta 4, resizable images are flaky for dark mode.
  // Radar filled: b/137942721.
  UIImage* resolvedImage = [UIImage imageNamed:imageName
                                      inBundle:nil
                 compatibleWithTraitCollection:self.traitCollection];
  UIEdgeInsets insets = UIEdgeInsetsMake(
      0, kTabBackgroundLeftCapInset, resolvedImage.size.height + 1.0,
      resolvedImage.size.width - kTabBackgroundLeftCapInset + 1.0);
  UIImage* backgroundImage =
      StretchableImageFromUIImage(resolvedImage, kTabBackgroundLeftCapInset, 0);
  return [[UIImageView alloc]
      initWithImage:[backgroundImage resizableImageWithCapInsets:insets]];
}

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  [self.delegate closeButtonTappedForCell:self];
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  // Style the favicon tint color.
  self.faviconView.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                        : [UIColor colorNamed:kGrey500Color];
  // Style the close button tint color.
  self.closeButton.tintColor = selected ? [UIColor colorNamed:kCloseButtonColor]
                                        : [UIColor colorNamed:kGrey500Color];
  // Style the title tint color.
  self.titleLabel.textColor = selected ? [UIColor colorNamed:kTextPrimaryColor]
                                       : [UIColor colorNamed:kGrey600Color];
}

@end
