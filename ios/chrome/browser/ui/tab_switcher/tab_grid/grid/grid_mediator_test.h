// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MEDIATOR_TEST_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MEDIATOR_TEST_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios_forward.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/platform_test.h"

class AuthenticationService;
@class BaseGridMediator;
class Browser;
class BrowserList;
@class FakeTabCollectionConsumer;
@class FakeTabGridToolbarsMediator;
class GURL;
class IOSChromeScopedTestingLocalState;
class PlatformTest;
@class SceneState;
class TestSceneUrlLoadingService;
class UrlLoadingBrowserAgent;
@class FakeURLLoadingDelegate;

namespace web {
class FakeWebState;
}  // namespace web

class GridMediatorTestClass : public PlatformTest {
 public:
  GridMediatorTestClass();
  ~GridMediatorTestClass() override;

  void SetUp() override;
  void TearDown() override;

  // Creates a FakeWebState with a navigation history containing exactly only
  // the given `url`.
  std::unique_ptr<web::FakeWebState> CreateFakeWebStateWithURL(const GURL& url);

  // Waits the consumer to be fully updated.
  bool WaitForConsumerUpdates(size_t expected_count);

 protected:
  // Inits feature flags.
  virtual void InitializeFeatureFlags();

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeTabCollectionConsumer* consumer_;
  std::vector<web::WebStateID> original_identifiers_;
  web::WebStateID original_selected_identifier_;
  __strong SceneState* scene_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<Browser> other_browser_;
  raw_ptr<BrowserList> browser_list_;
  base::UserActionTester user_action_tester_;
  raw_ptr<AuthenticationService> auth_service_;
  FakeTabGridToolbarsMediator* fake_toolbars_mediator_;
  std::unique_ptr<TestSceneUrlLoadingService> scene_loader_;
  raw_ptr<UrlLoadingBrowserAgent> loader_;
  FakeURLLoadingDelegate* url_loading_delegate_;
};

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MEDIATOR_TEST_H_
