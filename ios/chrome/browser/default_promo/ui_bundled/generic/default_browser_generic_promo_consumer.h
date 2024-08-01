// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_CONSUMER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for object that will create a view with messaging dependent
// on user classification from the Segmentation Platform.
@protocol DefaultBrowserGenericPromoConsumer <NSObject>

// Sets the title of the animated Default Browser promo.
- (void)setPromoTitle:(NSString*)titleText;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_CONSUMER_H_
