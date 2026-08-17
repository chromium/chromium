// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_PARAMS_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_PARAMS_H_

#import <Foundation/Foundation.h>

// Parameters for configuring the backend promo custom UI modal.
//
// Copy strings are provided dynamically at runtime by a remote backend server
// rather than loaded from local localized string resources.
@interface BackendPromoCustomUIParams : NSObject

// Title text received from the backend server for the promo modal.
@property(nonatomic, copy) NSString* title;

// Body text received from the backend server for the promo modal.
@property(nonatomic, copy) NSString* body;

// Title string received from the backend server for the primary action button.
@property(nonatomic, copy) NSString* primaryActionTitle;

// Title string received from the backend server for the secondary action
// button.
@property(nonatomic, copy) NSString* secondaryActionTitle;

// Optional image URL string received from the backend server for the promo
// image.
@property(nonatomic, copy) NSString* imageURL;

@end

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_PARAMS_H_
