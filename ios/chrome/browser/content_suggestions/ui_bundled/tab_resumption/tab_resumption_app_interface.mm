// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/tab_resumption_app_interface.h"

#import "components/commerce/core/mock_shopping_service.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "testing/gmock/include/gmock/gmock.h"

@implementation TabResumptionAppInterface : NSObject

+ (void)setUpMockShoppingService {
  commerce::ShoppingServiceFactory::GetInstance()->SetTestingFactory(
      chrome_test_util::GetOriginalProfile(),
      base::BindOnce([](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
        auto service = std::make_unique<
            testing::NiceMock<commerce::MockShoppingService>>();
        service->SetIsShoppingListEligible(true);
        service->SetGetAllPriceTrackedBookmarksCallbackValue({});
        return service;
      }));
}

@end
