// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_view.h"

#import "ios/chrome/browser/ui/omnibox/popup/favicon_retriever.h"
#import "ios/chrome/browser/ui/omnibox/popup/image_retriever.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxIconView ()

@property(nonatomic, strong) UIImageView* backgroundImageView;
@property(nonatomic, strong) UIImageView* mainImageView;
@property(nonatomic, strong) UIImageView* overlayImageView;

@end

@implementation OmniboxIconView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _backgroundImageView = [[UIImageView alloc] initWithImage:nil];
    _backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;

    _mainImageView = [[UIImageView alloc] initWithImage:nil];
    _mainImageView.translatesAutoresizingMaskIntoConstraints = NO;

    _overlayImageView = [[UIImageView alloc] initWithImage:nil];
    _overlayImageView.translatesAutoresizingMaskIntoConstraints = NO;

    UIImageView* mask = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"background_solid"]];
    self.maskView = mask;
  }
  return self;
}

- (void)prepareForReuse {
  self.backgroundImageView.image = nil;
  self.mainImageView.image = nil;
  [self.overlayImageView removeFromSuperview];
  self.mainImageView.contentMode = UIViewContentModeCenter;
}

// Override layoutSubviews to set the frame of the the mask
// correctly. It should be the same size as the view itself.
- (void)layoutSubviews {
  [super layoutSubviews];
  self.maskView.frame = self.bounds;
}

- (void)setupLayout {
  [self addSubview:self.backgroundImageView];
  [self addSubview:self.mainImageView];

  self.mainImageView.contentMode = UIViewContentModeCenter;

  [NSLayoutConstraint activateConstraints:@[
    [self.mainImageView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [self.mainImageView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [self.mainImageView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [self.mainImageView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],

    [self.backgroundImageView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [self.backgroundImageView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [self.backgroundImageView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [self.backgroundImageView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
  ]];
}

- (void)addOverlayImageView {
  [self addSubview:self.overlayImageView];
  [NSLayoutConstraint activateConstraints:@[
    [self.overlayImageView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [self.overlayImageView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [self.overlayImageView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [self.overlayImageView.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor],
  ]];
}

- (void)setOmniboxIcon:(id<OmniboxIcon>)omniboxIcon {
  // Setup the view layout the first time the cell is setup.
  if (self.subviews.count == 0) {
    [self setupLayout];
  }

  switch (omniboxIcon.iconType) {
    case OmniboxIconTypeImage: {
      self.mainImageView.contentMode = UIViewContentModeScaleAspectFill;
      __weak OmniboxIconView* weakSelf = self;
      GURL imageURL = omniboxIcon.imageURL;
      [self.imageRetriever fetchImage:imageURL
                           completion:^(UIImage* image) {
                             // Make sure cell is still displaying the same
                             // suggestion.
                             if (omniboxIcon.imageURL != imageURL) {
                               return;
                             }
                             [weakSelf addOverlayImageView];
                             weakSelf.overlayImageView.image =
                                 omniboxIcon.overlayImage;
                             weakSelf.overlayImageView.tintColor =
                                 omniboxIcon.overlayImageTintColor;
                             weakSelf.mainImageView.image = image;
                           }];
      break;
    }
    case OmniboxIconTypeFavicon: {
      // Set fallback icon
      self.mainImageView.image = omniboxIcon.iconImage;

      // Load favicon.
      GURL pageURL = omniboxIcon.imageURL;
      __weak OmniboxIconView* weakSelf = self;
      [self.faviconRetriever fetchFavicon:pageURL
                               completion:^(UIImage* image) {
                                 if (pageURL == omniboxIcon.imageURL) {
                                   weakSelf.mainImageView.image = image;
                                 }
                               }];
      break;
    }
    case OmniboxIconTypeSuggestionIcon:
      self.mainImageView.image = omniboxIcon.iconImage;
      break;
  }
  self.mainImageView.tintColor = omniboxIcon.iconImageTintColor;

  self.backgroundImageView.image = omniboxIcon.backgroundImage;
  self.backgroundImageView.tintColor = omniboxIcon.backgroundImageTintColor;
}

- (UIImage*)mainImage {
  return self.mainImageView.image;
}

@end
