// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Tab close button insets.
const CGFloat kTabBackgroundLeftCapInset = 34.0;
const CGFloat kFaviconInset = 28;
const CGFloat kTitleInset = 10.0;
}  // namespace

@implementation TabStripCell

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    NSString* imageName = @"tabstrip_background_tab";
    UIImage* image = [UIImage imageNamed:imageName];
    UIEdgeInsets insets =
        UIEdgeInsetsMake(0, kTabBackgroundLeftCapInset, image.size.height + 1.0,
                         image.size.width - kTabBackgroundLeftCapInset + 1.0);
    self.backgroundView = [[UIImageView alloc]
        initWithImage:[image resizableImageWithCapInsets:insets]];

    UIImage* favicon = [[UIImage imageNamed:@"default_world_favicon"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* faviconView = [[UIImageView alloc] initWithImage:favicon];
    faviconView.tintColor = [UIColor colorNamed:kGrey500Color];
    [self.contentView addSubview:faviconView];
    faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [faviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kFaviconInset],
      [faviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    UIImage* close = [[UIImage imageNamed:@"grid_cell_close_button"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    [closeButton setImage:close forState:UIControlStateNormal];
    closeButton.tintColor = [UIColor colorNamed:kGrey500Color];
    [self.contentView addSubview:closeButton];
    closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [closeButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kFaviconInset],
      [closeButton.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];
    [closeButton addTarget:self
                    action:@selector(closeButtonTapped:)
          forControlEvents:UIControlEventTouchUpInside];

    UILabel* titleLabel = [[UILabel alloc] init];
    [self.contentView addSubview:titleLabel];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [titleLabel.leadingAnchor
          constraintEqualToAnchor:faviconView.trailingAnchor
                         constant:kTitleInset],
      [titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:closeButton.leadingAnchor
                                   constant:-kTitleInset],
      [titleLabel.centerYAnchor
          constraintEqualToAnchor:faviconView.centerYAnchor],
    ]];
    self.titleLabel = titleLabel;
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
  self.itemIdentifier = nil;
}

// Selector registered to the close button.
- (void)closeButtonTapped:(id)sender {
  [self.delegate closeButtonTappedForCell:self];
}

@end
