// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_image_container_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The width and height of the Price Notifications ImageView.
const CGFloat kPriceNotificationsImageLength = 64;
// Corner radius of the Price Notifications ImageView.
const CGFloat kPriceNotificationsCornerRadius = 13.0;

}  // namespace

@interface PriceNotificationsImageContainerView ()

// The imageview containing the product's image.
@property(nonatomic, strong) UIImageView* priceNotificationsImageView;

@end

@implementation PriceNotificationsImageContainerView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.layer.cornerRadius = kPriceNotificationsCornerRadius;
    self.layer.masksToBounds = YES;

    _priceNotificationsImageView = [[UIImageView alloc] init];
    _priceNotificationsImageView.contentMode = UIViewContentModeScaleAspectFit;
    [self addSubview:_priceNotificationsImageView];
    _priceNotificationsImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _priceNotificationsImageView.backgroundColor =
        [UIColor colorNamed:kGrey100Color];

    AddSameConstraints(self, _priceNotificationsImageView);
    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor
          constraintEqualToConstant:kPriceNotificationsImageLength],
      [self.widthAnchor constraintEqualToAnchor:self.heightAnchor],
    ]];
  }
  return self;
}

- (void)setImage:(UIImage*)productImage {
  _priceNotificationsImageView.image = productImage;
}

@end
