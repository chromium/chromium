// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_image_container_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

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
    _priceNotificationsImageView.backgroundColor = UIColor.whiteColor;

    AddSameConstraints(self, _priceNotificationsImageView);
    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor
          constraintEqualToConstant:PriceNotificationsImageView::
                                        kPriceNotificationsImageLength],
      [self.widthAnchor constraintEqualToAnchor:self.heightAnchor],
    ]];
  }
  return self;
}

- (void)setImage:(UIImage*)productImage {
  _priceNotificationsImageView.image = productImage;
}

@end
