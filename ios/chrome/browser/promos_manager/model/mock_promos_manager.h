// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_MOCK_PROMOS_MANAGER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_MOCK_PROMOS_MANAGER_H_

#import "ios/chrome/browser/promos_manager/model/promos_manager.h"

#import <Foundation/Foundation.h>

#import <map>
#import <optional>
#import <set>

#import "base/containers/small_map.h"
#import "base/time/time.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/impression_limit.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "testing/gmock/include/gmock/gmock.h"

// Mock version of PromosManager.
class MockPromosManager : public PromosManager {
 public:
  MockPromosManager();
  ~MockPromosManager() override;

  // PromosManager implementation.
  MOCK_METHOD(void, Init, (), (override));
  MOCK_METHOD(void,
              InitializePromoConfigs,
              ((PromoConfigsSet promo_impression_limits)),
              (override));
  MOCK_METHOD(void,
              DeregisterAfterDisplay,
              (promos_manager::Promo promo),
              (override));
  MOCK_METHOD(std::optional<promos_manager::Promo>,
              NextPromoForDisplay,
              (),
              (override));
  MOCK_METHOD(void,
              RegisterPromoForContinuousDisplay,
              (promos_manager::Promo promo),
              (override));
  MOCK_METHOD(void,
              RegisterPromoForSingleDisplay,
              (promos_manager::Promo promo),
              (override));
  MOCK_METHOD(void,
              RegisterPromoForSingleDisplay,
              (promos_manager::Promo promo,
               base::TimeDelta becomes_active_after_period),
              (override));
  MOCK_METHOD(void, DeregisterPromo, (promos_manager::Promo promo), (override));
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_MOCK_PROMOS_MANAGER_H_
