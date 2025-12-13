// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_IMAGE_IMAGE_NAMES_H_
#define IOS_CHROME_BROWSER_SHARED_UI_IMAGE_IMAGE_NAMES_H_

#import <UIKit/UIKit.h>

#import "build/build_config.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"

// Branded image names.
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
extern NSString* const kChromeDefaultBrowserScreenBannerImage;
extern NSString* const kChromeGuidedTourBannerImage;
extern NSString* const kChromeNotificationsOptInBannerImage;
extern NSString* const kChromeNotificationsOptInBannerLandscapeImage;
extern NSString* const kChromeSearchEngineChoiceIcon;
extern NSString* const kChromeSigninBannerImage;
extern NSString* const kChromeSigninPromoLogoImage;
extern NSString* const kGoogleSearchEngineLogoImage;
extern NSString* const kGooglePasswordManagerWidgetPromoImage;
extern NSString* const kGooglePasswordManagerWidgetPromoDisabledImage;
extern NSString* const kGoogleSettingsPasswordsInOtherAppsBannerImage;
#else
extern NSString* const kChromiumDefaultBrowserIllustrationImage;
extern NSString* const kChromiumDefaultBrowserScreenBannerImage;
extern NSString* const kChromiumGuidedTourBannerImage;
extern NSString* const kChromiumNotificationsOptInBannerImage;
extern NSString* const kChromiumNotificationsOptInBannerLandscapeImage;
extern NSString* const kChromiumPasswordManagerWidgetPromoImage;
extern NSString* const kChromiumPasswordManagerWidgetPromoDisabledImage;
extern NSString* const kChromiumSearchEngineChoiceIcon;
extern NSString* const kChromiumSettingsPasswordsInOtherAppsBannerImage;
extern NSString* const kChromiumSigninBannerImage;
extern NSString* const kChromiumSigninPromoLogoImage;
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)

// Custom image names.
extern NSString* const kPasswordManagerTrustedVaultWidgetPromoImage;
extern NSString* const kPasswordManagerTrustedVaultWidgetPromoDisabledImage;

#endif  // IOS_CHROME_BROWSER_SHARED_UI_IMAGE_IMAGE_NAMES_H_
