// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/showcase_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ShowcaseModel

// Insert additional rows in this array. All rows will be sorted upon
// import into Showcase.
// |kShowcaseClassForDisplayKey| and |kShowcaseClassForInstantiationKey| are
// required. |kShowcaseUseCaseKey| is optional.
+ (NSArray<showcase::ModelRow*>*)model {
  return @[
    @{
      showcase::kClassForDisplayKey : @"ContentSuggestionsViewController",
      showcase::kClassForInstantiationKey : @"SCContentSuggestionsCoordinator",
      showcase::kUseCaseKey : @"Content Suggestions UI",
    },
    @{
      showcase::kClassForDisplayKey : @"PaymentRequestEditViewController",
      showcase::kClassForInstantiationKey : @"SCPaymentsEditorCoordinator",
      showcase::kUseCaseKey : @"Generic payment request editor",
    },
    @{
      showcase::kClassForDisplayKey : @"PaymentRequestPickerViewController",
      showcase::kClassForInstantiationKey : @"SCPaymentsPickerCoordinator",
      showcase::kUseCaseKey : @"Payment request picker view",
    },
    @{
      showcase::kClassForDisplayKey : @"PaymentRequestSelectorViewController",
      showcase::kClassForInstantiationKey : @"SCPaymentsSelectorCoordinator",
      showcase::kUseCaseKey : @"Payment request selector view",
    },
    @{
      showcase::kClassForDisplayKey : @"SettingsViewController",
      showcase::kClassForInstantiationKey : @"SCSettingsCoordinator",
      showcase::kUseCaseKey : @"Main settings screen",
    },
    @{
      showcase::kClassForDisplayKey : @"UITableViewCell",
      showcase::kClassForInstantiationKey : @"UIKitTableViewCellViewController",
      showcase::kUseCaseKey : @"UIKit Table Cells",
    },
    @{
      showcase::kClassForDisplayKey : @"SearchWidgetViewController",
      showcase::kClassForInstantiationKey : @"SCSearchWidgetCoordinator",
      showcase::kUseCaseKey : @"Search Widget",
    },
    @{
      showcase::kClassForDisplayKey : @"ContentWidgetViewController",
      showcase::kClassForInstantiationKey : @"SCContentWidgetCoordinator",
      showcase::kUseCaseKey : @"Content Widget",
    },
    @{
      showcase::kClassForDisplayKey : @"TextBadgeView",
      showcase::kClassForInstantiationKey : @"SCTextBadgeViewController",
      showcase::kUseCaseKey : @"Text badge",
    },
    @{
      showcase::kClassForDisplayKey : @"BubbleViewController",
      showcase::kClassForInstantiationKey : @"SCBubbleCoordinator",
      showcase::kUseCaseKey : @"Bubble",
    },
    @{
      showcase::kClassForDisplayKey : @"GridViewController",
      showcase::kClassForInstantiationKey : @"SCGridCoordinator",
      showcase::kUseCaseKey : @"Grid UI",
    },
    @{
      showcase::kClassForDisplayKey : @"GridCell",
      showcase::kClassForInstantiationKey : @"SCGridCellViewController",
      showcase::kUseCaseKey : @"Grid cells",
    },
    @{
      showcase::kClassForDisplayKey : @"TabGridViewController",
      showcase::kClassForInstantiationKey : @"SCTabGridCoordinator",
      showcase::kUseCaseKey : @"Full tab grid UI",
    },
    @{
      showcase::
      kClassForDisplayKey : @"TabGridTopToolbar, TabGridBottomToolbar",
      showcase::kClassForInstantiationKey : @"SCToolbarsViewController",
      showcase::kUseCaseKey : @"Toolbars for tab grid",
    },
    @{
      showcase::kClassForDisplayKey : @"TableContainerViewController",
      showcase::kClassForInstantiationKey : @"SCTableContainerCoordinator",
      showcase::kUseCaseKey : @"Table View",
    },
    @{
      showcase::kClassForDisplayKey : @"TopAlignedImageView",
      showcase::kClassForInstantiationKey : @"SCImageViewController",
      showcase::kUseCaseKey : @"ImageView with top aligned aspect fill",
    },
    @{
      showcase::kClassForDisplayKey : @"RecentTabsTableViewController",
      showcase::kClassForInstantiationKey : @"SCDarkThemeRecentTabsCoordinator",
      showcase::kUseCaseKey : @"Dark theme recent tabs",
    },
    @{
      showcase::kClassForDisplayKey : @"OmniboxPopupViewController",
      showcase::kClassForInstantiationKey : @"SCOmniboxPopupCoordinator",
      showcase::kUseCaseKey : @"Omnibox popup table view",
    },
    @{
      showcase::kClassForDisplayKey : @"InfobarBannerViewController",
      showcase::kClassForInstantiationKey : @"SCInfobarBannerCoordinator",
      showcase::kUseCaseKey : @"Infobar Banner",
    },
    @{
      showcase::kClassForDisplayKey : @"InfobarBannerViewController",
      showcase::
      kClassForInstantiationKey : @"SCInfobarBannerNoModalCoordinator",
      showcase::kUseCaseKey : @"Infobar Banner No Modal",
    },
    @{
      showcase::kClassForDisplayKey : @"AlertController",
      showcase::kClassForInstantiationKey : @"SCAlertCoordinator",
      showcase::kUseCaseKey : @"Alert",
    },
    @{
      showcase::kClassForDisplayKey : @"BadgeViewController",
      showcase::kClassForInstantiationKey : @"SCBadgeCoordinator",
      showcase::kUseCaseKey : @"Badge View",
    },
  ];
}

@end
