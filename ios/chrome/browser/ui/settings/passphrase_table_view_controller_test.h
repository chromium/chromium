// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSPHRASE_TABLE_VIEW_CONTROLLER_TEST_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSPHRASE_TABLE_VIEW_CONTROLLER_TEST_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios_forward.h"
#include "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

namespace syncer {
class MockSyncService;
}  // namespace syncer

@class AppState;
class Browser;
@class SceneState;
@class SettingsNavigationController;
@class UINavigationController;
@class UIViewController;

// Base class for PassphraseTableViewController tests.
// Sets up a testing profile and a mock profile sync service, along with the
// supporting structure they require.
class PassphraseTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  PassphraseTableViewControllerTest();
  ~PassphraseTableViewControllerTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  // Allow sub-classes to register testing factories in the builder for the
  // new TestProfileIOS.
  virtual void RegisterTestingFactories(TestProfileIOS::Builder& builder);

  void SetUpNavigationController(UIViewController* test_controller);

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  // Weak, owned by profile_.
  raw_ptr<syncer::MockSyncService> fake_sync_service_;

  // Default return values for NiceMock<syncer::MockSyncService>.
  GoogleServiceAuthError default_auth_error_;
  syncer::SyncCycleSnapshot default_sync_cycle_snapshot_;

  // Dummy navigation stack for testing self-removal.
  // Only valid when SetUpNavigationController has been called.
  UIViewController* dummy_controller_;
  SettingsNavigationController* nav_controller_;

  // Dummy scene state.
  SceneState* scene_state_;
  // Dummy app state.
  AppState* app_state_;
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSPHRASE_TABLE_VIEW_CONTROLLER_TEST_H_
