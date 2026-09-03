// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_PARAMS_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_PARAMS_H_

#import <Foundation/Foundation.h>

@class BackendPromoLottieParams;

// Holds the parameters needed to configure a custom backend promo UI.
@interface BackendPromoCustomUIParams : NSObject

// Title string displayed at the top of the promo.
@property(nonatomic, copy) NSString* title;

// Body text explaining the promo details.
@property(nonatomic, copy) NSString* body;

// Title string for the primary action button.
@property(nonatomic, copy) NSString* primaryActionTitle;

// Title string for the secondary action button.
@property(nonatomic, copy) NSString* secondaryActionTitle;

// Image asset name or URL string for the promo image/animation.
@property(nonatomic, copy) NSString* imageURL;

// Lottie animation configuration parameters.
@property(nonatomic, strong) BackendPromoLottieParams* lottieParams;

// Ordered list of instruction step strings displayed below the title.
@property(nonatomic, copy) NSArray<NSString*>* instructionSteps;

@end

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_PARAMS_H_
