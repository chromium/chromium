// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/image/image_names.h"

// Branded image names.
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
NSString* const kChromeDefaultBrowserIllustrationImage =
    @"chrome_default_browser_illustration";
NSString* const kChromeDefaultBrowserScreenBannerImage =
    @"chrome_default_browser_screen_banner";
NSString* const kChromeGuidedTourBannerImage = @"chrome_guided_tour_banner";
NSString* const kChromeNotificationsOptInBannerImage =
    @"chrome_notifications_opt_in_banner";
NSString* const kChromeNotificationsOptInBannerLandscapeImage =
    @"chrome_notifications_opt_in_banner_landscape";
NSString* const kChromeSearchEngineChoiceIcon =
    @"chrome_search_engine_choice_icon";
NSString* const kChromeSigninBannerImage = @"chrome_signin_banner";
NSString* const kChromeSigninPromoLogoImage = @"chrome_signin_promo_logo";
NSString* const kGoogleSearchEngineLogoImage = @"google_logo";
NSString* const kGooglePasswordManagerWidgetPromoImage =
    @"google_password_manager_widget_promo";
NSString* const kGooglePasswordManagerWidgetPromoDisabledImage =
    @"google_password_manager_widget_promo_disabled";
NSString* const kGoogleSettingsPasswordsInOtherAppsBannerImage =
    @"google_settings_passwords_in_other_apps_banner";
#else
NSString* const kChromiumDefaultBrowserIllustrationImage =
    @"chromium_default_browser_illustration";
NSString* const kChromiumDefaultBrowserScreenBannerImage =
    @"chromium_default_browser_screen_banner";
NSString* const kChromiumGuidedTourBannerImage = @"chromium_guided_tour_banner";
NSString* const kChromiumNotificationsOptInBannerImage =
    @"chromium_notifications_opt_in_banner";
NSString* const kChromiumNotificationsOptInBannerLandscapeImage =
    @"chromium_notifications_opt_in_banner_landscape";
NSString* const kChromiumPasswordManagerWidgetPromoImage =
    @"chromium_password_manager_widget_promo";
NSString* const kChromiumPasswordManagerWidgetPromoDisabledImage =
    @"chromium_password_manager_widget_promo_disabled";
NSString* const kChromiumSearchEngineChoiceIcon =
    @"chromium_search_engine_choice_icon";
NSString* const kChromiumSettingsPasswordsInOtherAppsBannerImage =
    @"chromium_settings_passwords_in_other_apps_banner";
NSString* const kChromiumSigninBannerImage = @"chromium_signin_banner";
NSString* const kChromiumSigninPromoLogoImage = @"chromium_signin_promo_logo";

#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)

// Custom image names.
NSString* const kPasswordManagerTrustedVaultWidgetPromoImage =
    @"password_manager_trusted_vault_widget_promo";
NSString* const kPasswordManagerTrustedVaultWidgetPromoDisabledImage =
    @"password_manager_trusted_vault_widget_promo_disabled";
