// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_view_controller_delegate.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@protocol NewTabPageURLLoaderDelegate;

// Main view controller for the Incognito interstitial, to be managed by the
// associated `IncognitoInterstitialCoordinator`.
@interface IncognitoInterstitialViewController : PromoStyleViewController

@property(nonatomic, weak) id<IncognitoInterstitialViewControllerDelegate>
    delegate;

// Some URLs in the controlled view can be loaded.
@property(nonatomic, weak) id<NewTabPageURLLoaderDelegate> URLLoaderDelegate;

// URL text to display.
@property(nonatomic, copy) NSString* URLText;

@end

#endif  // IOS_CHROME_BROWSER_INCOGNITO_INTERSTITIAL_UI_BUNDLED_INCOGNITO_INTERSTITIAL_VIEW_CONTROLLER_H_
