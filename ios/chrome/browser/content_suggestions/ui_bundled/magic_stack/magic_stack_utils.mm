// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_utils.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/variations/service/variations_service_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/model/price_tracking_promo_prefs.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

CGFloat ModuleNarrowerWidthToAllowPeekingForTraitCollection(
    UITraitCollection* traitCollection) {
  BOOL isLandscape = [[UIDevice currentDevice] orientation] ==
                         UIDeviceOrientationLandscapeRight ||
                     [[UIDevice currentDevice] orientation] ==
                         UIDeviceOrientationLandscapeLeft;
  BOOL isLargerWidthLayout =
      traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular ||
      isLandscape;
  // For the narrow width layout, make the module just slightly narrower than
  // the inter-module spacing so the UICollectionView renders the adjacent
  // module(s).
  return isLargerWidthLayout ? kMagicStackPeekInsetLandscape
                             : kMagicStackPeekInset;
}

bool IsPriceTrackingPromoCardEnabled(commerce::ShoppingService* service,
                                     AuthenticationService* auth_service,
                                     PrefService* pref_service) {
  id<SystemIdentity> identity =
      auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  return GetApplicationContext()->GetApplicationLocaleStorage()->Get() ==
             "en-US" &&
         !push_notification_settings::
             GetMobileNotificationPermissionStatusForClient(
                 PushNotificationClientId::kCommerce, identity.gaiaId) &&
         !pref_service->GetBoolean(kPriceTrackingPromoDisabled) &&
         (service->IsShoppingListEligible() ||
          (base::GetFieldTrialParamByFeatureAsString(
               segmentation_platform::features::
                   kSegmentationPlatformEphemeralCardRanker,
               segmentation_platform::features::
                   kEphemeralCardRankerForceShowCardParam,
               "") == segmentation_platform::kPriceTrackingNotificationPromo &&
           base::ToLowerASCII(GetCurrentCountryCode(
               GetApplicationContext()->GetVariationsService())) == "us"));
}

bool isContentOversized(id<UITraitEnvironment> trait_environment) {
  // The preferred content size of the user's device.
  NSString* preferred_content_size =
      trait_environment.traitCollection.preferredContentSizeCategory;
  NSComparisonResult result = UIContentSizeCategoryCompareToCategory(
      preferred_content_size, UIContentSizeCategoryAccessibilityMedium);
  return result != NSOrderedAscending;
}

CGFloat GetMagicStackHeight(id<UITraitEnvironment> trait_environment) {
  // The preferred content size of the user's device.
  NSString* preferred_content_size =
      trait_environment.traitCollection.preferredContentSizeCategory;
  if (isContentOversized(trait_environment)) {
    // The maximum Magic Stack height in px.
    return 190;
  } else if (preferred_content_size ==
             UIContentSizeCategoryExtraExtraExtraLarge) {
    return 180;
  } else if (preferred_content_size == UIContentSizeCategoryExtraExtraLarge) {
    return 170;
  } else if (preferred_content_size == UIContentSizeCategoryExtraLarge) {
    return 160;
  } else {
    // The minimum Magic Stack height in px.
    return 150;
  }
}
