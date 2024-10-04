// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/favicon/favicon_container_view.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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

@interface FaviconContainerView ()

// Store custom background color.
@property(nonatomic, strong) UIColor* customBackgroundColor;

// Store custom border color.
@property(nonatomic, strong) UIColor* customBorderColor;

@end

@implementation FaviconContainerView

- (instancetype)init {
  self = [super init];
  if (self) {
    [self.traitCollection performAsCurrentTraitCollection:^{
      [self resetColor];
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

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = @[
        UITraitUserInterfaceIdiom.self, UITraitUserInterfaceStyle.self,
        UITraitDisplayGamut.self, UITraitAccessibilityContrast.self,
        UITraitUserInterfaceLevel.self
      ];

      [self registerForTraitChanges:traits
                         withAction:@selector(updateColorOnTraitChange:)];
    }
  }
  return self;
}

- (void)setFaviconBackgroundColor:(UIColor*)color {
  self.customBackgroundColor = color;
  if (color) {
    self.backgroundColor = color;
  } else {
    [self resetColor];
  }
}

- (void)setFaviconBorderColor:(UIColor*)color {
  self.customBorderColor = color;
  if (color) {
    self.layer.borderColor = color.CGColor;
  } else {
    [self resetColor];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateColorOnTraitChange:previousTraitCollection];
}
#endif

- (void)resetColor {
  if (self.customBackgroundColor) {
    self.backgroundColor = self.customBackgroundColor;
  } else {
    self.backgroundColor =
        self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
            ? [UIColor colorNamed:kSeparatorColor]
            : UIColor.clearColor;
  }
  self.layer.borderColor = self.customBorderColor
                               ? self.customBorderColor.CGColor
                               : [UIColor colorNamed:kSeparatorColor].CGColor;
}

#pragma mark - Private

- (void)updateColorOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    [self resetColor];
  }
}

@end
