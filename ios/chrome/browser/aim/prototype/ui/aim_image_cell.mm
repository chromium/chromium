// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_image_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@implementation AIMImageCell {
  UIImageView* _imageView;
  UIActivityIndicatorView* _loadingIndicator;
  UIView* _errorScrimView;
  UIImageView* _errorIconView;
  AIMInputItemState _state;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _imageView = [[UIImageView alloc] initWithFrame:self.bounds];
    _imageView.contentMode = UIViewContentModeScaleAspectFill;
    _imageView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.contentView addSubview:_imageView];

    _errorScrimView = [[UIView alloc] initWithFrame:self.bounds];
    _errorScrimView.backgroundColor = [UIColor colorWithRed:1.0
                                                      green:0.0
                                                       blue:0.0
                                                      alpha:0.5];
    _errorScrimView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    _errorScrimView.hidden = YES;
    [self.contentView addSubview:_errorScrimView];

    _loadingIndicator = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
    _loadingIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_loadingIndicator];

    UIImageSymbolConfiguration* symbolConfig =
        [UIImageSymbolConfiguration configurationWithPointSize:24];
    UIImage* errorImage =
        DefaultSymbolWithConfiguration(@"exclamationmark.circle", symbolConfig);
    _errorIconView = [[UIImageView alloc] initWithImage:errorImage];
    _errorIconView.translatesAutoresizingMaskIntoConstraints = NO;
    _errorIconView.tintColor = [UIColor whiteColor];
    _errorIconView.hidden = YES;
    [self.contentView addSubview:_errorIconView];

    [NSLayoutConstraint activateConstraints:@[
      [_loadingIndicator.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_loadingIndicator.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_errorIconView.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_errorIconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    self.layer.cornerRadius = 16;
    self.clipsToBounds = YES;
    _state = AIMInputItemState::kLoaded;
  }
  return self;
}

- (void)configureWithItem:(AIMInputItem*)item {
  if (_state == item.state && _imageView.image == item.previewImage) {
    return;
  }

  _state = item.state;
  _imageView.image = item.previewImage;
  _errorIconView.hidden = YES;
  _errorScrimView.hidden = YES;
  _imageView.alpha = 1.0;

  BOOL isLoading = _state == AIMInputItemState::kLoading ||
                   _state == AIMInputItemState::kUploading;

  if (isLoading) {
    if (!_loadingIndicator.isAnimating) {
      [_loadingIndicator startAnimating];
    }
    _imageView.alpha = 0.5;
  } else {
    if (_loadingIndicator.isAnimating) {
      [_loadingIndicator stopAnimating];
    }
  }

  if (_state == AIMInputItemState::kError) {
    _errorIconView.hidden = NO;
    _errorScrimView.hidden = NO;
    _imageView.alpha = 0.5;
  }
}

@end
