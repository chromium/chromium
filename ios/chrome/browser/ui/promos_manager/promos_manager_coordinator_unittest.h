// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_UNITTEST_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_UNITTEST_H_

#import "base/mac/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/platform_test.h"

class PromosManagerCoordinatorTest : public PlatformTest {
 public:
  PromosManagerCoordinatorTest();
  ~PromosManagerCoordinatorTest() override;

 protected:
  // Creates PromosManagerCoordinator.
  void CreatePromosManagerCoordinator();

  // Create pref registry for tests.
  void CreatePrefs();

  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  PromosManagerCoordinator* coordinator_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
};

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_COORDINATOR_UNITTEST_H_
