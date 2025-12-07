// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/omnibox_icon_view.h"

#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/omnibox/model/suggestions/omnibox_icon.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/favicon_retriever.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/image_retriever.h"
#import "ios/chrome/browser/shared/ui/symbols/colorful_background_symbol_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

namespace {

// Scales the symbol dimensions proportionally relative to the parent.
const CGFloat kSymbolInsetMultiplier = 0.7;

}  // namespace

@implementation OmniboxIconView {
  id<OmniboxIcon> _omniboxIcon;
  // The view containing the symbols or the favicons.
  ColorfulBackgroundSymbolView* _colorfulView;
  // The view containing the downloaded images.
  UIImageView* _imageView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _colorfulView = [[ColorfulBackgroundSymbolView alloc] init];
    _colorfulView.translatesAutoresizingMaskIntoConstraints = NO;

    _imageView = [[UIImageView alloc] init];
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.contentMode = UIViewContentModeScaleAspectFit;
    // Have the same corner radius as the other view.
    _imageView.layer.cornerRadius = kColorfulBackgroundSymbolCornerRadius;
    _imageView.layer.masksToBounds = YES;
  }
  return self;
}

- (void)prepareForReuse {
  _imageView.image = nil;
  _imageView.hidden = YES;
  _colorfulView.hidden = NO;
  [_colorfulView resetView];
}

// Override layoutSubviews to set the frame of the the mask
// correctly. It should be the same size as the view itself.
- (void)layoutSubviews {
  [super layoutSubviews];
  self.maskView.frame = self.bounds;
}

- (void)setupLayout {
  [self addSubview:_colorfulView];
  [self addSubview:_imageView];

  AddSameCenterConstraints(_colorfulView, self);
  [NSLayoutConstraint activateConstraints:@[
    [_colorfulView.widthAnchor constraintEqualToAnchor:self.widthAnchor
                                            multiplier:kSymbolInsetMultiplier],
    [_colorfulView.heightAnchor constraintEqualToAnchor:self.heightAnchor
                                             multiplier:kSymbolInsetMultiplier],
  ]];

  AddSameConstraints(_imageView, self);
}

- (void)setOmniboxIcon:(id<OmniboxIcon>)omniboxIcon {
  _omniboxIcon = omniboxIcon;

  // Setup the view layout the first time the cell is setup.
  if (self.subviews.count == 0) {
    [self setupLayout];
  }

  __weak ColorfulBackgroundSymbolView* weakColorfulView = _colorfulView;

  switch (omniboxIcon.iconType) {
    case OmniboxIconTypeImage: {
      __weak UIImageView* weakImageView = _imageView;
      __weak id<OmniboxIcon> weakOmniboxIcon = _omniboxIcon;
      _colorfulView.hidden = YES;
      GURL imageURL = omniboxIcon.imageURL.gurl;
      [self.imageRetriever fetchImage:imageURL
                           completion:^(UIImage* image) {
                             // Make sure cell is still displaying the same
                             // suggestion.
                             if (!weakOmniboxIcon.imageURL ||
                                 weakOmniboxIcon.imageURL.gurl != imageURL) {
                               return;
                             }
                             weakImageView.hidden = NO;
                             weakImageView.image = image;
                           }];
      break;
    }
    case OmniboxIconTypeFavicon: {
      // Set fallback icon
      [_colorfulView setSymbol:omniboxIcon.iconImage];

      // Load favicon.
      GURL pageURL = omniboxIcon.imageURL.gurl;
      __weak id<OmniboxIcon> weakOmniboxIcon = _omniboxIcon;
      [self.faviconRetriever fetchFavicon:pageURL
                               completion:^(UIImage* image) {
                                 if (!weakOmniboxIcon.imageURL ||
                                     pageURL != weakOmniboxIcon.imageURL.gurl) {
                                   return;
                                 }
                                 [weakColorfulView setSymbol:image];
                               }];
      break;
    }
    case OmniboxIconTypeSuggestionIcon:
      [_colorfulView setSymbol:omniboxIcon.iconImage];
      break;
  }
  [UIView performWithoutAnimation:^{
    weakColorfulView.symbolTintColor = omniboxIcon.iconImageTintColor;
    weakColorfulView.backgroundColor = omniboxIcon.backgroundImageTintColor;
    weakColorfulView.borderColor = omniboxIcon.borderColor;
  }];
}

- (void)setHighlighted:(BOOL)highlighted {
  _highlighted = highlighted;

  _colorfulView.symbolTintColor =
      highlighted ? UIColor.whiteColor : _omniboxIcon.iconImageTintColor;
}

@end
