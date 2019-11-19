// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSPHRASE_TABLE_VIEW_CONTROLLER_TEST_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSPHRASE_TABLE_VIEW_CONTROLLER_TEST_H_

#include "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"

#include "base/compiler_specific.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

namespace syncer {
class MockSyncService;
}  // namespace syncer

namespace web {
class BrowserState;
}  // namespace web

class Browser;
class TestChromeBrowserState;
@class UINavigationController;
@class UIViewController;

// Base class for PassphraseTableViewController tests.
// Sets up a testing profile and a mock profile sync service, along with the
// supporting structure they require.
class PassphraseTableViewControllerTest : public ChromeTableViewControllerTest {
 public:
  static std::unique_ptr<KeyedService> CreateNiceMockSyncService(
      web::BrowserState* context);

  PassphraseTableViewControllerTest();
  ~PassphraseTableViewControllerTest() override;

 protected:
  void SetUp() override;

  void SetUpNavigationController(UIViewController* test_controller);

  web::WebTaskEnvironment task_environment_;

  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  // Weak, owned by chrome_browser_state_.
  syncer::MockSyncService* fake_sync_service_;

  // Default return values for NiceMock<syncer::MockSyncService>.
  GoogleServiceAuthError default_auth_error_;
  syncer::SyncCycleSnapshot default_sync_cycle_snapshot_;

  // Dummy navigation stack for testing self-removal.
  // Only valid when SetUpNavigationController has been called.
  UIViewController* dummy_controller_;
  UINavigationController* nav_controller_;
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSPHRASE_TABLE_VIEW_CONTROLLER_TEST_H_
