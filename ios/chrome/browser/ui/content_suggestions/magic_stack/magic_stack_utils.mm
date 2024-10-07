// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"

#import "base/metrics/field_trial_params.h"
#import "base/strings/sys_string_conversions.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_prefs.h"

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
                             : kMagicStackSpacing + 1;
}

bool IsPriceTrackingPromoCardEnabled(commerce::ShoppingService* service,
                                     AuthenticationService* auth_service,
                                     PrefService* pref_service) {
  id<SystemIdentity> identity =
      auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  return base::FeatureList::IsEnabled(commerce::kPriceTrackingPromo) &&
         GetApplicationContext()->GetApplicationLocale() == "en-US" &&
         !push_notification_settings::
             GetMobileNotificationPermissionStatusForClient(
                 PushNotificationClientId::kCommerce,
                 base::SysNSStringToUTF8(identity.gaiaID)) &&
         !pref_service->GetBoolean(kPriceTrackingPromoDisabled) &&
         (service->IsShoppingListEligible() ||
          base::GetFieldTrialParamByFeatureAsString(
              segmentation_platform::features::
                  kSegmentationPlatformEphemeralCardRanker,
              segmentation_platform::features::
                  kEphemeralCardRankerForceShowCardParam,
              "") == segmentation_platform::features::
                         kPriceTrackingPromoForceOverride);
}
