// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"

#import "base/metrics/field_trial_params.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"

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

bool IsPriceTrackingPromoCardEnabled(commerce::ShoppingService* service) {
  return base::FeatureList::IsEnabled(commerce::kPriceTrackingPromo) &&
         (service->IsShoppingListEligible() ||
          base::GetFieldTrialParamByFeatureAsString(
              segmentation_platform::features::
                  kSegmentationPlatformEphemeralCardRanker,
              segmentation_platform::features::
                  kEphemeralCardRankerForceShowCardParam,
              "") == segmentation_platform::features::
                         kPriceTrackingPromoForceOverride);
}
