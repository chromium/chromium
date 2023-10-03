// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_UTIL_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_UTIL_H_

#import "ios/chrome/browser/default_browser/model/utils.h"

@protocol TailoredPromoConsumer <NSObject>

// The image. Must be set before the view is loaded.
@property(nonatomic, strong) UIImage* image;

// The headline below the image. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* titleString;

// The subtitle below the title. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* subtitleString;

// The text for the primary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* primaryActionString;

// The text for the secondary action. Must be set before the view is loaded.
@property(nonatomic, copy) NSString* secondaryActionString;

@end

// Set ups the consumer properties for the passed style.
void SetUpTailoredConsumerWithType(id<TailoredPromoConsumer> consumer,
                                   DefaultPromoType type);

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_TAILORED_PROMO_UTIL_H_
