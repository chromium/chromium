// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/utils/home_customization_metrics_recorder.h"

#import "base/containers/contains.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/commerce/core/commerce_feature_list.h"

namespace {

// User action names for toggling cells from the customization menu's main page.
const char kMostVisitedToggledAction[] =
    "IOS.HomeCustomization.MainPage.MostVisited.Toggled";
const char kMagicStackToggledAction[] =
    "IOS.HomeCustomization.MainPage.MagicStack.Toggled";
const char kFeedToggledAction[] = "IOS.HomeCustomization.MainPage.Feed.Toggled";

// User action names for toggling cells from the Magic Stack page in the
// customization menu.
const char kSafetyCheckToggledAction[] =
    "IOS.HomeCustomization.MagicStackPage.SafetyCheck.Toggled";
const char kTabResumptionToggledAction[] =
    "IOS.HomeCustomization.MagicStackPage.TabResumption.Toggled";
const char kTipsToggledAction[] =
    "IOS.HomeCustomization.MagicStackPage.Tips.Toggled";
const char kShopCardPriceTrackingAction[] =
    "IOS.HomeCustomization.MagicStackPage.ShopCardPriceTracking.Toggled";

}  // namespace

@implementation HomeCustomizationMetricsRecorder

+ (void)recordCellToggled:(CustomizationToggleType)toggleType {
  switch (toggleType) {
      // Main page toggles.
    case CustomizationToggleType::kMostVisited:
      base::RecordAction(base::UserMetricsAction(kMostVisitedToggledAction));
      return;
    case CustomizationToggleType::kMagicStack:
      base::RecordAction(base::UserMetricsAction(kMagicStackToggledAction));
      return;
    case CustomizationToggleType::kDiscover:
      base::RecordAction(base::UserMetricsAction(kFeedToggledAction));
      return;

      // Magic Stack toggles.
    case CustomizationToggleType::kSafetyCheck:
      base::RecordAction(base::UserMetricsAction(kSafetyCheckToggledAction));
      return;
    case CustomizationToggleType::kTapResumption:
      base::RecordAction(base::UserMetricsAction(kTabResumptionToggledAction));
      return;
    case CustomizationToggleType::kTips:
      base::RecordAction(base::UserMetricsAction(kTipsToggledAction));
      return;
    case CustomizationToggleType::kShopCard:
      base::RecordAction(base::UserMetricsAction(kShopCardPriceTrackingAction));
      return;
  }
}

@end
