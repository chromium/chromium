// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_IMAGE_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_IMAGE_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

namespace PriceNotificationsImageView {

// The width and height of the Price Notifications ImageView.
const CGFloat kPriceNotificationsImageLength = 64;

}  // namespace PriceNotificationsImageView

// A UIView that contains the PriceNotification item's image.
@interface PriceNotificationsImageContainerView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

- (void)setImage:(UIImage*)productImage;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_CELLS_PRICE_NOTIFICATIONS_IMAGE_CONTAINER_VIEW_H_
