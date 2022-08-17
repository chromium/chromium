// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_unittest.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PromosManagerTest::PromosManagerTest() {}
PromosManagerTest::~PromosManagerTest() {}

void PromosManagerTest::CreatePromosManager() {
  CreatePrefs();
  promos_manager_ = std::make_unique<PromosManager>(local_state_.get());
}

// Create pref registry for tests.
void PromosManagerTest::CreatePrefs() {
  local_state_ = std::make_unique<TestingPrefServiceSimple>();
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerImpressions);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerActivePromos);
}

// Tests the initializer correctly creates a PromosManager* with the
// specified Pref service.
TEST_F(PromosManagerTest, InitWithPrefService) {
  CreatePromosManager();

  EXPECT_NE(local_state_->FindPreference(prefs::kIosPromosManagerImpressions),
            nullptr);
  EXPECT_NE(local_state_->FindPreference(prefs::kIosPromosManagerActivePromos),
            nullptr);
  EXPECT_FALSE(local_state_->HasPrefPath(prefs::kIosPromosManagerImpressions));
  EXPECT_FALSE(local_state_->HasPrefPath(prefs::kIosPromosManagerActivePromos));
}
