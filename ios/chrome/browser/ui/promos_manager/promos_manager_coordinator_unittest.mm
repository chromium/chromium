// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator_unittest.h"

#import <Foundation/Foundation.h>

#import "base/mac/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PromosManagerCoordinatorTest::PromosManagerCoordinatorTest() {
  browser_state_ = TestChromeBrowserState::Builder().Build();
  browser_ = std::make_unique<TestBrowser>(browser_state_.get());
  view_controller_ = [[UIViewController alloc] init];
  [scoped_key_window_.Get() setRootViewController:view_controller_];
}
PromosManagerCoordinatorTest::~PromosManagerCoordinatorTest() {}

void PromosManagerCoordinatorTest::CreatePromosManagerCoordinator() {
  CreatePromosManager();

  coordinator_ = [[PromosManagerCoordinator alloc]
      initWithBaseViewController:view_controller_
                         browser:browser_.get()];
}

void PromosManagerCoordinatorTest::CreatePromosManager() {
  CreatePrefs();
  promos_manager_ = std::make_unique<PromosManager>(local_state_.get());
  promos_manager_->Init();
}

// Create pref registry for tests.
void PromosManagerCoordinatorTest::CreatePrefs() {
  local_state_ = std::make_unique<TestingPrefServiceSimple>();

  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerImpressions);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerActivePromos);
  local_state_->registry()->RegisterListPref(
      prefs::kIosPromosManagerSingleDisplayActivePromos);
}

// Tests the initializer correctly creates a PromosManagerMediator.
TEST_F(PromosManagerCoordinatorTest, InitCreatesMediator) {
  scoped_feature_list_.InitWithFeatures(
      {kFullscreenPromosManager,
       post_restore_signin::features::kIOSNewPostRestoreExperience},
      {});
  CreatePromosManagerCoordinator();

  EXPECT_NE(coordinator_.mediator, nil);
}

// Tests the initializer does not create a PromosManagerMediator, because there
// are no promos registered with the coordinator.
TEST_F(PromosManagerCoordinatorTest, InitDoesNotCreatesMediator) {
  scoped_feature_list_.InitWithFeatures({kFullscreenPromosManager}, {});
  CreatePromosManagerCoordinator();

  EXPECT_EQ(coordinator_.mediator, nil);
}
