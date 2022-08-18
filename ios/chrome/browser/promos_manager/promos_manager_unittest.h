// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_UNITTEST_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_UNITTEST_H_

#import <Foundation/Foundation.h>

#import "base/test/scoped_feature_list.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "ios/chrome/browser/promos_manager/promo.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "testing/platform_test.h"

class PromosManagerTest : public PlatformTest {
 public:
  PromosManagerTest();
  ~PromosManagerTest() override;

  // Creates a mock promo without impression limits.
  Promo* TestPromo();

  // Creates a mock promo with impression limits.
  Promo* TestPromoWithImpressionLimits();

  // Creates mock impression limits.
  NSArray<ImpressionLimit*>* TestImpressionLimits();

 protected:
  // Creates PromosManager with empty pref data.
  void CreatePromosManager();

  // Create pref registry for tests.
  void CreatePrefs();

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<PromosManager> promos_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_UNITTEST_H_
