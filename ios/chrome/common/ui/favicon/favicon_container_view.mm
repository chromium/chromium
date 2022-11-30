// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/favicon/favicon_container_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The width and height of the favicon ImageView.
const CGFloat kFaviconWidth = 16;
// Corner radius of the favicon ImageView.
const CGFloat kFaviconCornerRadius = 7.0;
// Width of the favicon border ImageView.
const CGFloat kFaviconBorderWidth = 1.5;
// The width and height of the favicon container view.
const CGFloat kFaviconContainerWidth = 30;
}  // namespace

@implementation FaviconContainerView

- (instancetype)init {
  self = [super init];
  if (self) {
    [self.traitCollection performAsCurrentTraitCollection:^{
      if (self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark) {
        self.backgroundColor = [UIColor colorNamed:kSeparatorColor];
      }
      self.layer.borderColor = [UIColor colorNamed:kSeparatorColor].CGColor;
    }];
    self.layer.borderWidth = kFaviconBorderWidth;
    self.layer.cornerRadius = kFaviconCornerRadius;
    self.layer.masksToBounds = YES;

    _faviconView = [[FaviconView alloc] init];
    _faviconView.contentMode = UIViewContentModeScaleAspectFit;
    _faviconView.clipsToBounds = YES;
    [self addSubview:_faviconView];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint activateConstraints:@[
      // The favicon view is a fixed size, is pinned to the leading edge of the
      // content view, and is centered vertically.
      [_faviconView.heightAnchor constraintEqualToConstant:kFaviconWidth],
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconWidth],
      [_faviconView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
      [_faviconView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],

      [self.heightAnchor constraintEqualToConstant:kFaviconContainerWidth],
      [self.widthAnchor constraintEqualToAnchor:self.heightAnchor],
    ]];
  }
  return self;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    self.backgroundColor =
        self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
            ? [UIColor colorNamed:kSeparatorColor]
            : UIColor.clearColor;
    self.layer.borderColor = [UIColor colorNamed:kSeparatorColor].CGColor;
  }
}

@end
