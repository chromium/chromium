// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/policy_watcher_browser_agent.h"

#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;
using web::WebTaskEnvironment;

class PolicyWatcherBrowserAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    chrome_browser_state_ = builder.Build();
  }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    PrefServiceMockFactory factory;
    scoped_refptr<PrefRegistrySyncable> registry(new PrefRegistrySyncable);
    std::unique_ptr<PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

#pragma mark - Tests.

// Tests that the browser agent monitors the kSigninAllowed pref and dispatches
// the appropriate command when the pref becomes false.
TEST_F(PolicyWatcherBrowserAgentTest, observesSigninAllowed) {
  // Set the initial pref value.
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, true);

  // Set up the test browser and attach the browser agent under test.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(chrome_browser_state_.get());
  PolicyWatcherBrowserAgent::CreateForBrowser(browser.get());

  // Set up the mock ApplicationCommands handler and inject it in the browser
  // agent.
  id applicationCommandHandler =
      [OCMockObject mockForProtocol:@protocol(ApplicationCommands)];
  [((id<ApplicationCommands>)[applicationCommandHandler expect]) forceSignOut];
  PolicyWatcherBrowserAgent::FromBrowser(browser.get())
      ->SetApplicationCommandsHandler(applicationCommandHandler);

  // Action: disable browser sign-in.
  chrome_browser_state_->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);

  // Verify the forceSignOut command was dispatched by the browser agent.
  EXPECT_OCMOCK_VERIFY(applicationCommandHandler);
}
