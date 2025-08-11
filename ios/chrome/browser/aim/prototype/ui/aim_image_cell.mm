// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_image_cell.h"

@implementation AIMImageCell {
  UIImageView* _imageView;
  UIActivityIndicatorView* _loadingIndicator;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _imageView = [[UIImageView alloc] initWithFrame:self.bounds];
    _imageView.contentMode = UIViewContentModeScaleAspectFill;
    _imageView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.contentView addSubview:_imageView];

    _loadingIndicator = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
    _loadingIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_loadingIndicator];

    [NSLayoutConstraint activateConstraints:@[
      [_loadingIndicator.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_loadingIndicator.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
    ]];

    self.layer.cornerRadius = 16;
    self.clipsToBounds = YES;
  }
  return self;
}

- (void)configureWithItem:(AIMInputItem*)item {
  _imageView.image = item.previewImage;
  if (item.state == AIMInputItemState::kLoading ||
      item.state == AIMInputItemState::kUploading) {
    [_loadingIndicator startAnimating];
    _imageView.alpha = 0.5;
  } else {
    [_loadingIndicator stopAnimating];
    _imageView.alpha = 1.0;
  }
}

@end
