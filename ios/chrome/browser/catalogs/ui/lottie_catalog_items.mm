// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/lottie_catalog_items.h"

#import "ios/chrome/browser/catalogs/ui/lottie_catalog_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

// The visual configurations below (color providers and background colors) are
// duplicated from the production presenters so that the catalog stays
// self-contained. Strings are not mirrored: animations display the English
// texts embedded in their files. When a production configuration changes, the
// matching function here should be updated. Each function references its
// source of truth.

namespace {

// Feature names, displayed in the row subtitles.
NSString* const kBestFeaturesFeature = @"Best Features";
NSString* const kCredentialProviderPromoFeature = @"Credential Provider Promo";
NSString* const kDefaultBrowserFeature = @"Default Browser";
NSString* const kDockingPromoFeature = @"Docking Promo";
NSString* const kMagicStackTipsFeature = @"Magic Stack Tips";
NSString* const kNTPCustomizationFeature = @"NTP Customization";
NSString* const kSafariDataImportFeature = @"Safari Data Import";
NSString* const kTipsNotificationsFeature = @"Tips Notifications";
NSString* const kWhatsNewFeature = @"What's New";

// Shadow colors of the Tips Notifications promos animations.
const int kTipsPromoLightShadowColor = 0xDADCE0;
const int kTipsPromoDarkShadowColor = 0x5F6368;

// Returns an item for the animation `name` of `feature`.
LottieCatalogItem* CatalogItem(NSString* name, NSString* feature) {
  LottieCatalogItem* item = [[LottieCatalogItem alloc] init];
  item.animationName = name;
  item.feature = feature;
  return item;
}

// Returns an item for the animation `name` of `feature`, displayed over
// `backgroundColor`.
LottieCatalogItem* CatalogItemWithBackground(NSString* name,
                                             NSString* feature,
                                             UIColor* backgroundColor) {
  LottieCatalogItem* item = CatalogItem(name, feature);
  item.animationBackgroundColor = backgroundColor;
  return item;
}

// Applies the same color provider in light and dark mode to `item`.
void SetColorProvider(LottieCatalogItem* item,
                      NSDictionary<NSString*, UIColor*>* provider) {
  item.lightModeColorProvider = provider;
  item.darkModeColorProvider = provider;
}

#pragma mark - Best Features

// Source: `first_run/public/best_features_item.mm` and
// `first_run/best_features/ui/feature_highlight_screenshot_view_controller.mm`.

// Returns a Best Features item.
LottieCatalogItem* BestFeaturesItem(NSString* name) {
  return CatalogItemWithBackground(
      name, kBestFeaturesFeature,
      [UIColor colorNamed:kSecondaryBackgroundColor]);
}

// Returns the standard Best Features color provider.
NSDictionary<NSString*, UIColor*>* StandardColorProvider(
    int groupedQuaternaryBackgroundColor,
    int groupedTertiaryBackgroundColor) {
  return @{
    @"text_primary_color" : [UIColor colorNamed:kTextPrimaryColor],
    @"background_color" : [UIColor colorNamed:kBackgroundColor],
    @"tertiary_background_color" :
        [UIColor colorNamed:kTertiaryBackgroundColor],
    @"grouped_tertiary_background_color" :
        UIColorFromRGB(groupedTertiaryBackgroundColor),
    @"grouped_quaternary_background_color" :
        UIColorFromRGB(groupedQuaternaryBackgroundColor),
  };
}

// Applies the standard light and dark color providers to `item`.
void SetStandardColorProviders(LottieCatalogItem* item) {
  item.lightModeColorProvider = StandardColorProvider(0xFFFFFF, 0xE8EAED);
  item.darkModeColorProvider = StandardColorProvider(0x5F6368, 0x5F6368);
}

// Returns the color provider of the Lens animation.
NSDictionary<NSString*, UIColor*>* LensColorProvider(int omniboxColor,
                                                     int lensBackgroundColor) {
  return @{
    @"Omnibox" : UIColorFromRGB(omniboxColor),
    @"Lens_Icon_Background" : UIColorFromRGB(lensBackgroundColor),
  };
}

// Returns the color provider of the Search With Lens animation.
NSDictionary<NSString*, UIColor*>* SearchWithLensColorProvider(
    int ntpBackgroundColor,
    int cardBackgroundColor,
    int omniboxBackgroundColor,
    int lensIconBackgroundColor,
    int magicStackContentColor,
    int resultsInnerCardColor) {
  return @{
    @"ntp_background_color" : UIColorFromRGB(ntpBackgroundColor),
    @"card_background_color" : UIColorFromRGB(cardBackgroundColor),
    @"omnibox_background_color" : UIColorFromRGB(omniboxBackgroundColor),
    @"lens_icon_background_color" : UIColorFromRGB(lensIconBackgroundColor),
    @"shadow_background_color" : [UIColor colorNamed:kTertiaryBackgroundColor],
    @"omnibox_text_color" : [UIColor colorNamed:kTextSecondaryColor],
    @"magic_stack_content_color" : UIColorFromRGB(magicStackContentColor),
    @"results_inner_card_color" : UIColorFromRGB(resultsInnerCardColor),
  };
}

// Returns the color provider of the Price Tracking animation.
// TODO(crbug.com/585553993): The callers pass an alpha byte in
// `gradientGroupedPrimaryBackgroundColor`, which `UIColorFromRGB` ignores.
// Update alongside the fix in `best_features_item.mm`.
NSDictionary<NSString*, UIColor*>* PriceTrackingColorProvider(
    int gradientGroupedPrimaryBackgroundColor) {
  return @{
    @"grouped_primary_background_color" :
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor],
    @"grouped_secondary_background_color" :
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor],
    @"blue_color" : [UIColor colorNamed:kBlueColor],
    @"separator_color" : [UIColor colorNamed:kSeparatorColor],
    @"tertiary_background_color" :
        [UIColor colorNamed:kTertiaryBackgroundColor],
    @"gradient_grouped_primary_background_color" :
        UIColorFromRGB(gradientGroupedPrimaryBackgroundColor),
    @"Smart Watch" : [UIColor colorNamed:kTextPrimaryColor],
    @"Price $100-$180" : [UIColor colorNamed:kTextSecondaryColor],
    @"Track Price" : [UIColor colorNamed:kInvertedTextPrimaryColor],
  };
}

// Returns the color provider of the Safe Browsing Tutorial animation. Keys are
// prepended with a dot to force deep keypath matching.
NSDictionary<NSString*, UIColor*>* SafeBrowsingColorProvider() {
  return @{
    @".kGroupedPrimaryBackgroundColor" :
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor],
    @".kGroupedSecondaryBackgroundColor" :
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor],
    @".kTertiaryBackgroundColor" :
        [UIColor colorNamed:kTertiaryBackgroundColor],
    @".kBlueColor" : [UIColor colorNamed:kBlueColor],
    @".kBackgroundStateSelected" : [UIColor colorNamed:kBlueHaloColor],
    @".Safe Browsing 2" : [UIColor colorNamed:kTextPrimaryColor],
    @".Enhanced Protection" : [UIColor colorNamed:kTextPrimaryColor],
    @".Standard Protection" : [UIColor colorNamed:kTextPrimaryColor],
    @".No Protection" : [UIColor colorNamed:kTextPrimaryColor],
  };
}

// Returns the color provider of the Share Passwords animation.
NSDictionary<NSString*, UIColor*>* SharePasswordsColorProvider() {
  return @{
    @"secondary_background_color" :
        [UIColor colorNamed:kSecondaryBackgroundColor],
    @"primary_background_color" : [UIColor colorNamed:kPrimaryBackgroundColor],
    @"blue_color" : [UIColor colorNamed:kBlueColor],
    @"Share Your Passwords" : [UIColor colorNamed:kTextPrimaryColor],
    @"Share your passwords" : [UIColor colorNamed:kInvertedTextPrimaryColor],
  };
}

// Returns the Best Features items of the Incognito animations.
NSArray<LottieCatalogItem*>* IncognitoItems() {
  LottieCatalogItem* browse = BestFeaturesItem(@"browse_in_incognito");
  SetColorProvider(browse, @{
    @"kTertiaryBackgroundColor" : [UIColor colorNamed:kTertiaryBackgroundColor],
  });

  LottieCatalogItem* locked = BestFeaturesItem(@"locked_incognito_tabs");
  SetStandardColorProviders(locked);

  LottieCatalogItem* lockedDarkMode =
      BestFeaturesItem(@"locked_incognito_tabs_darkmode");
  SetStandardColorProviders(lockedDarkMode);
  return @[ browse, locked, lockedDarkMode ];
}

// Returns the Best Features items of the passwords animations.
NSArray<LottieCatalogItem*>* PasswordsItems() {
  LottieCatalogItem* sharing = BestFeaturesItem(@"password_sharing");
  SetColorProvider(sharing, SharePasswordsColorProvider());
  return @[
    BestFeaturesItem(@"CPE_promo_animation_edu_autofill"),
    BestFeaturesItem(@"CPE_promo_animation_edu_autofill_darkmode"), sharing
  ];
}

// Returns the Best Features items of the Safe Browsing animations.
NSArray<LottieCatalogItem*>* SafeBrowsingItems() {
  LottieCatalogItem* enhanced =
      BestFeaturesItem(@"enhanced_safe_browsing_promo");
  SetStandardColorProviders(enhanced);

  LottieCatalogItem* enhancedDarkMode =
      BestFeaturesItem(@"enhanced_safe_browsing_promo_darkmode");
  SetStandardColorProviders(enhancedDarkMode);

  LottieCatalogItem* tutorial = BestFeaturesItem(@"safe_browsing_tutorial");
  SetColorProvider(tutorial, SafeBrowsingColorProvider());
  return @[ enhanced, enhancedDarkMode, tutorial ];
}

// Returns the Best Features items of the Lens animations.
NSArray<LottieCatalogItem*>* LensItems() {
  LottieCatalogItem* lens = BestFeaturesItem(@"lens_promo");
  lens.lightModeColorProvider = LensColorProvider(0xEDF4FE, 0xFFFFFF);
  lens.darkModeColorProvider = LensColorProvider(0x232428, 0x464A4E);

  LottieCatalogItem* searchWithLens =
      BestFeaturesItem(@"search_with_lens_promo");
  searchWithLens.lightModeColorProvider = SearchWithLensColorProvider(
      0xEDF4FE, 0xFFFFFF, 0x022771, 0xFFFFFF, 0xE8F0FE, 0xEFF4FE);
  searchWithLens.darkModeColorProvider = SearchWithLensColorProvider(
      0x35363A, 0x3E4042, 0x80868B, 0x3E4042, 0x4A4D50, 0x35363A);
  return @[ lens, searchWithLens ];
}

// Returns the Best Features items of the shopping and tab animations.
NSArray<LottieCatalogItem*>* ShoppingAndTabsItems() {
  LottieCatalogItem* priceTracking = BestFeaturesItem(@"price_tracking");
  priceTracking.lightModeColorProvider = PriceTrackingColorProvider(0xF1F3F480);
  priceTracking.darkModeColorProvider = PriceTrackingColorProvider(0x20212480);

  LottieCatalogItem* tabGroups = BestFeaturesItem(@"tab_groups");
  SetStandardColorProviders(tabGroups);
  return @[ priceTracking, tabGroups ];
}

// Returns the Best Features items.
NSArray<LottieCatalogItem*>* BestFeaturesItems() {
  NSMutableArray<LottieCatalogItem*>* items = [NSMutableArray array];
  [items addObjectsFromArray:IncognitoItems()];
  [items addObjectsFromArray:PasswordsItems()];
  [items addObjectsFromArray:SafeBrowsingItems()];
  [items addObjectsFromArray:LensItems()];
  [items addObjectsFromArray:ShoppingAndTabsItems()];
  return items;
}

#pragma mark - Credential Provider Promo

// Source: `credential_provider_promo/ui_bundled/
// credential_provider_promo_view_controller.mm`.
NSArray<LottieCatalogItem*>* CredentialProviderPromoItems() {
  UIColor* backgroundColor = [UIColor colorNamed:kGrey100Color];
  return @[
    CatalogItemWithBackground(@"CPE_promo_animation_edu_how_to_enable",
                              kCredentialProviderPromoFeature, backgroundColor),
    CatalogItemWithBackground(@"CPE_promo_animation_edu_how_to_enable_darkmode",
                              kCredentialProviderPromoFeature, backgroundColor),
  ];
}

#pragma mark - Default Browser

// Source: `default_browser/promo/ui/
// default_browser_instructions_view_controller.mm`.

// Returns the color provider of the iPad Default Browser animations.
NSDictionary<NSString*, UIColor*>* DefaultBrowserIpadColorProvider() {
  return @{
    kSeparatorColor : [UIColor colorNamed:kSeparatorColor],
    kBlueColor : [UIColor colorNamed:kBlueColor],
    kGrey200Color : [UIColor colorNamed:kGrey200Color],
    kBackgroundColor : [UIColor colorNamed:kBackgroundColor],
    kGroupedPrimaryBackgroundColor :
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor],
    kGroupedSecondaryBackgroundColor :
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor],
    @"IDS_CHROME" : [UIColor colorNamed:kTextPrimaryColor],
    @"IDS_CHROME_SECONDARY" : [UIColor colorNamed:kTextSecondaryColor],
    @"IDS_DEFAULT_BROWSER_APP" : [UIColor colorNamed:kTextPrimaryColor],
    @"IDS_IOS_SETTINGS" : [UIColor colorNamed:kTextPrimaryColor],
  };
}

// Returns the Default Browser items.
NSArray<LottieCatalogItem*>* DefaultBrowserItems() {
  UIColor* backgroundColor = [UIColor colorNamed:kGrey100Color];
  NSMutableArray<LottieCatalogItem*>* items = [NSMutableArray array];
  for (NSString* name in @[
         @"default_browser_animation", @"default_browser_animation_darkmode",
         @"default_browser_animation_rtl",
         @"default_browser_animation_rtl_darkmode",
         @"default_browser_default_apps_animation",
         @"default_browser_default_apps_animation_darkmode",
         @"default_browser_default_apps_animation_rtl",
         @"default_browser_default_apps_animation_rtl_darkmode"
       ]) {
    [items addObject:CatalogItemWithBackground(name, kDefaultBrowserFeature,
                                               backgroundColor)];
  }
  // The iPad animations use dynamic colors.
  for (NSString* name in @[
         @"default_browser_animation_ipad",
         @"default_browser_animation_rtl_ipad"
       ]) {
    LottieCatalogItem* item = CatalogItemWithBackground(
        name, kDefaultBrowserFeature, backgroundColor);
    SetColorProvider(item, DefaultBrowserIpadColorProvider());
    [items addObject:item];
  }
  return items;
}

#pragma mark - Docking Promo

// Source: `docking_promo/ui/docking_promo_view_controller.mm`.
NSArray<LottieCatalogItem*>* DockingPromoItems() {
  UIColor* backgroundColor = [UIColor
      colorWithDynamicProvider:^UIColor*(UITraitCollection* traitCollection) {
        return traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
                   ? [UIColor colorNamed:kBackgroundColor]
                   : [UIColor colorNamed:kGrey100Color];
      }];
  return @[
    CatalogItemWithBackground(@"docking_promo", kDockingPromoFeature,
                              backgroundColor),
    CatalogItemWithBackground(@"docking_promo_darkmode", kDockingPromoFeature,
                              backgroundColor),
  ];
}

#pragma mark - Magic Stack Tips

// Source: `content_suggestions/tips/ui/
// save_passwords_instructional_view_controller.mm` and
// `use_autofill_instructional_view_controller.mm`.
NSArray<LottieCatalogItem*>* MagicStackTipsItems() {
  UIColor* backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  LottieCatalogItem* useAutofill = CatalogItemWithBackground(
      @"use_autofill", kMagicStackTipsFeature, backgroundColor);
  SetColorProvider(useAutofill, @{
    @"background_color" : [UIColor colorNamed:kBackgroundColor],
    @"secondary_background_color" :
        [UIColor colorNamed:kSecondaryBackgroundColor],
    @"blue_color" : [UIColor colorNamed:kBlueColor],
    @"separator_color" : [UIColor colorNamed:kSeparatorColor],
    @"tertiary_background_color" :
        [UIColor colorNamed:kTertiaryBackgroundColor],
    @"text_secondary_color" : [UIColor colorNamed:kTextSecondaryColor],
    @"Use Keyboard" : [UIColor colorNamed:kBlueColor],
    @"Use Password" : [UIColor colorNamed:kInvertedTextPrimaryColor],
  });
  return @[
    CatalogItemWithBackground(@"save_passwords", kMagicStackTipsFeature,
                              backgroundColor),
    CatalogItemWithBackground(@"save_passwords_darkmode",
                              kMagicStackTipsFeature, backgroundColor),
    useAutofill,
  ];
}

#pragma mark - NTP Customization

// Source: `content_suggestions/tips/ui/
// ntp_customization_promo_view_controller.mm`.
LottieCatalogItem* NTPCustomizationItem() {
  LottieCatalogItem* item =
      CatalogItemWithBackground(@"ntp_customization", kNTPCustomizationFeature,
                                [UIColor colorNamed:kSecondaryBackgroundColor]);
  SetColorProvider(item, @{
    kGroupedSecondaryBackgroundColor :
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor],
    kNewTabPageBackgroundColor :
        [UIColor colorNamed:kNewTabPageBackgroundColor],
    kNTPQuickActionChipColor : [UIColor colorNamed:kNTPQuickActionChipColor],
    kPrimaryBackgroundColor : [UIColor colorNamed:kPrimaryBackgroundColor],
  });
  return item;
}

#pragma mark - Safari Data Import

// Source: `safari_data_import/ui/
// safari_data_import_export_view_controller.mm`.
NSArray<LottieCatalogItem*>* SafariDataImportItems() {
  UIColor* backgroundColor = [UIColor colorNamed:kBlueHaloColor];
  NSMutableArray<LottieCatalogItem*>* items = [NSMutableArray array];
  for (NSString* name in @[
         @"safari_data_export_education_dark",
         @"safari_data_export_education_dark_rtl",
         @"safari_data_export_education_light",
         @"safari_data_export_education_light_rtl"
       ]) {
    [items addObject:CatalogItemWithBackground(name, kSafariDataImportFeature,
                                               backgroundColor)];
  }
  return items;
}

#pragma mark - Tips Notifications

// Applies the Tips Notifications promos shadow colors to `item`.
void SetTipsPromoShadowColors(LottieCatalogItem* item) {
  NSString* const shadowKeypath = @"Shadow.*.*.Color";
  item.lightModeColorProvider =
      @{shadowKeypath : UIColorFromRGB(kTipsPromoLightShadowColor)};
  item.darkModeColorProvider =
      @{shadowKeypath : UIColorFromRGB(kTipsPromoDarkShadowColor)};
}

// Source: `tips_notifications/ui/search_what_you_see_promo_view_controller.mm`,
// `price_tracking_promo_view_controller.mm` and
// `tab_groups_promo_view_controller.mm`.
NSArray<LottieCatalogItem*>* TipsNotificationsItems() {
  LottieCatalogItem* searchWhatYouSee =
      CatalogItem(@"search_what_you_see_promo", kTipsNotificationsFeature);
  SetColorProvider(searchWhatYouSee, @{
    @"grouped_primary_background_color" :
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor],
    @"tertiary_background_color" :
        [UIColor colorNamed:kTertiaryBackgroundColor],
    @"blue_color" : [UIColor colorNamed:kBlueColor],
  });

  LottieCatalogItem* priceTracking =
      CatalogItem(@"price_tracking_promo", kTipsNotificationsFeature);
  SetTipsPromoShadowColors(priceTracking);

  LottieCatalogItem* tabGroups =
      CatalogItem(@"tab_groups_promo", kTipsNotificationsFeature);
  SetTipsPromoShadowColors(tabGroups);
  return @[ searchWhatYouSee, priceTracking, tabGroups ];
}

#pragma mark - What's New

// Source: `whats_new/ui/whats_new_screenshot_view_controller.mm`.
NSArray<LottieCatalogItem*>* WhatsNewItems() {
  UIColor* backgroundColor = [UIColor colorNamed:kGrey100Color];
  NSMutableArray<LottieCatalogItem*>* items = [NSMutableArray array];
  for (NSString* name in @[
         @"BlingEnterpriseProfiles", @"BlingEnterpriseProfiles_darkmode",
         @"DockingBling", @"DockingBling_darkmode", @"LensSearchWhatYouSee",
         @"LensSearchWhatYouSee_darkmode"
       ]) {
    [items addObject:CatalogItemWithBackground(name, kWhatsNewFeature,
                                               backgroundColor)];
  }
  return items;
}

}  // namespace

NSArray<LottieCatalogItem*>* CreateLottieCatalogItems() {
  NSMutableArray<LottieCatalogItem*>* items = [NSMutableArray array];
  [items addObjectsFromArray:BestFeaturesItems()];
  [items addObjectsFromArray:CredentialProviderPromoItems()];
  [items addObjectsFromArray:DefaultBrowserItems()];
  [items addObjectsFromArray:DockingPromoItems()];
  [items addObjectsFromArray:MagicStackTipsItems()];
  [items addObject:NTPCustomizationItem()];
  [items addObjectsFromArray:SafariDataImportItems()];
  [items addObjectsFromArray:TipsNotificationsItems()];
  [items addObjectsFromArray:WhatsNewItems()];
  return items;
}
