// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/model/chrome_browser_state_manager_impl.h"

#import "base/containers/contains.h"
#import "ios/chrome/browser/browser_state/model/constants.h"
#import "ios/chrome/browser/browser_state/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Profile names.
const char kProfileName1[] = "Profile1";
const char kProfileName2[] = "Profile2";

// Returns the name of the ChromeBrowserState.
std::string GetChromeBrowserStateName(ChromeBrowserState* browser_state) {
  // The name of the ChromeBrowserState is the basename of its StatePath.
  // This is an invariant of ChromeBrowserStateManagerImpl.
  return browser_state->GetStatePath().BaseName().AsUTF8Unsafe();
}

}  // namespace

class ChromeBrowserStateManagerImplTest : public PlatformTest {
 public:
  ChromeBrowserStateManagerImplTest()
      : web_task_environment_(
            web::WebTaskEnvironment::IOThreadType::REAL_THREAD_DELAYED) {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    // IOSChromeIOThread needs to be created before the IO thread is started.
    // Thus DELAY_IO_THREAD_START is set in WebTaskEnvironment's options. The
    // thread is then started after the creation of IOSChromeIOThread.
    chrome_io_ = std::make_unique<IOSChromeIOThread>(
        application_context->GetLocalState(), application_context->GetNetLog());

    // Register the objects with the TestingApplicationContext.
    application_context->SetIOSChromeIOThread(chrome_io_.get());
    application_context->SetChromeBrowserStateManager(&browser_state_manager_);

    // Start the IO thread.
    web_task_environment_.StartIOThread();

    // Post a task to initialize the IOSChromeIOThread object on the IO thread.
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&IOSChromeIOThread::InitOnIO,
                                  base::Unretained(chrome_io_.get())));

    // Init the BrowserPolicyConnect as this is required to create
    // ChromeBrowserStateImpl.
    application_context->GetBrowserPolicyConnector()->Init(
        application_context->GetLocalState(),
        application_context->GetSharedURLLoaderFactory());

    // IOSChromeIOThread requires the SystemURLRequestContextGetter() to be
    // created before the object is shutdown, so force its creation here.
    std::ignore = chrome_io_->system_url_request_context_getter();
  }

  ~ChromeBrowserStateManagerImplTest() override {
    TestingApplicationContext* application_context =
        TestingApplicationContext::GetGlobal();

    application_context->GetBrowserPolicyConnector()->Shutdown();
    application_context->GetIOSChromeIOThread()->NetworkTearDown();
    application_context->SetChromeBrowserStateManager(nullptr);
    application_context->SetIOSChromeIOThread(nullptr);

    // Creating a ChromeBrowserState with ChromeBrowserStateManagerImpl will
    // create and initialize some KeyedService. Some of those services start
    // background task that hops between IO and UI threads. Post a task on
    // the IO thread and wait for the reply on UI thread to give time for
    // the services to complete their initialisation on IO thread.
    base::RunLoop run_loop;
    web::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

  ChromeBrowserStateManagerImpl& browser_state_manager() {
    return browser_state_manager_;
  }

  // Returns the name of the loaded ChromeBrowserStates.
  std::set<std::string> GetLoadedBrowserStateNames() {
    std::set<std::string> browser_state_names;
    for (ChromeBrowserState* browser_state :
         browser_state_manager_.GetLoadedBrowserStates()) {
      CHECK(browser_state);

      // The name of the ChromeBrowserState is the basename of its StatePath.
      // This is an invariant of ChromeBrowserStateManagerImpl.
      const std::string browser_state_name =
          GetChromeBrowserStateName(browser_state);

      CHECK(!base::Contains(browser_state_names, browser_state_name));
      browser_state_names.insert(browser_state_name);
    }
    return browser_state_names;
  }

 private:
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<IOSChromeIOThread> chrome_io_;
  web::WebTaskEnvironment web_task_environment_;
  ChromeBrowserStateManagerImpl browser_state_manager_;
};

// Tests that GetLoadedBrowserStates() returns an empty list before the
// BrowserStates are loaded, and then a list containing at least one
// BrowserState, and the last used BrowserState is loaded.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStates) {
  // There should be no BrowserState loaded yet.
  EXPECT_EQ(GetLoadedBrowserStateNames(), (std::set<std::string>{}));

  // Load the BrowserStates, this will implicitly add "Default" as a
  // BrowserState if there is no saved BrowserStates. Thus it should
  // load exactly one BrowserState.
  browser_state_manager().LoadBrowserStates();

  // Exactly one ChromeBrowserState must be loaded, it must be the last
  // used ChromeBrowserState with name `kIOSChromeInitialBrowserState`.
  ChromeBrowserState* browser_state =
      browser_state_manager().GetLastUsedBrowserStateDeprecatedDoNotUse();

  ASSERT_TRUE(browser_state);
  EXPECT_EQ(GetChromeBrowserStateName(browser_state),
            kIOSChromeInitialBrowserState);
  EXPECT_EQ(GetLoadedBrowserStateNames(),
            (std::set<std::string>{kIOSChromeInitialBrowserState}));
}

// Tests that LoadBrowserStates() always loads the "last used BrowserState"
// when `kBrowserStateLastUsed` and `kBrowserStatesLastActive` are out of
// sync.
//
// See https://crbug.com/345478758 for crashes related to this.
//
// Specifically, this test case check that even if both properties are set
// but `kBrowserStateLastUsed` is not `kIOSChromeInitialBrowserState` and
// not in `kBrowserStatesLastActive`, then the last used ChromeBrowserState
// is still loaded.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStates_IncoherentPrefs_1) {
  ASSERT_NE(kProfileName1, kIOSChromeInitialBrowserState);
  ASSERT_NE(kProfileName2, kIOSChromeInitialBrowserState);

  // There should be no BrowserState loaded yet.
  EXPECT_EQ(GetLoadedBrowserStateNames(), (std::set<std::string>{}));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetString(prefs::kBrowserStateLastUsed, kProfileName1);
  local_state->SetList(prefs::kBrowserStatesLastActive,
                       base::Value::List().Append(kProfileName2));

  browser_state_manager().LoadBrowserStates();

  // Exactly two ChromeBrowserState must be loaded, named `kProfileName1`
  // and `kProfileName2`, and the last used ChromeBrowserState is the one
  // named `kProfile1`.
  ChromeBrowserState* browser_state =
      browser_state_manager().GetLastUsedBrowserStateDeprecatedDoNotUse();

  ASSERT_TRUE(browser_state);
  EXPECT_EQ(GetChromeBrowserStateName(browser_state), kProfileName1);
  EXPECT_EQ(GetLoadedBrowserStateNames(),
            (std::set<std::string>{kProfileName1, kProfileName2}));
}

// Tests that LoadBrowserStates() always loads the "last used BrowserState"
// when `kBrowserStateLastUsed` and `kBrowserStatesLastActive` are out of
// sync.
//
// See https://crbug.com/345478758 for crashes related to this.
//
// Specifically, this test case check that if `kBrowserStatesLastActive` is
// not set and `kBrowserStateLastUsed` is not `kIOSChromeInitialBrowserState`,
// then the last used ChromeBrowserState is still loaded.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStates_IncoherentPrefs_2) {
  ASSERT_NE(kProfileName1, kIOSChromeInitialBrowserState);
  ASSERT_NE(kProfileName2, kIOSChromeInitialBrowserState);

  // There should be no BrowserState loaded yet.
  EXPECT_EQ(GetLoadedBrowserStateNames(), (std::set<std::string>{}));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetString(prefs::kBrowserStateLastUsed, kProfileName1);
  local_state->SetList(prefs::kBrowserStatesLastActive, base::Value::List());

  browser_state_manager().LoadBrowserStates();

  // Exactly one ChromeBrowserState must be loaded, it must be the last
  // used ChromeBrowserState with name `kProfileName1`.
  ChromeBrowserState* browser_state =
      browser_state_manager().GetLastUsedBrowserStateDeprecatedDoNotUse();

  ASSERT_TRUE(browser_state);
  EXPECT_EQ(GetChromeBrowserStateName(browser_state), kProfileName1);
  EXPECT_EQ(GetLoadedBrowserStateNames(),
            (std::set<std::string>{kProfileName1}));
}

// Tests that LoadBrowserStates() always loads the "last used BrowserState"
// when `kBrowserStateLastUsed` and `kBrowserStatesLastActive` are out of
// sync.
//
// See https://crbug.com/345478758 for crashes related to this.
//
// Specifically, this test case check that if `kBrowserStatesLastActive` is
// set but does not contains the value `kIOSChromeInitialBrowserState` and
// `kBrowserStateLastUsed` is unset, then the last used ChromeBrowserState
// is still loaded.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStates_IncoherentPrefs_3) {
  ASSERT_NE(kProfileName1, kIOSChromeInitialBrowserState);
  ASSERT_NE(kProfileName2, kIOSChromeInitialBrowserState);

  // There should be no BrowserState loaded yet.
  EXPECT_EQ(GetLoadedBrowserStateNames(), (std::set<std::string>{}));

  PrefService* local_state = GetApplicationContext()->GetLocalState();
  local_state->SetString(prefs::kBrowserStateLastUsed, std::string());
  local_state->SetList(prefs::kBrowserStatesLastActive,
                       base::Value::List().Append(kProfileName2));

  browser_state_manager().LoadBrowserStates();

  // Exactly two ChromeBrowserState must be loaded, named `kProfileName2`
  // and `kIOSChromeInitialBrowserState`, and the last used ChromeBrowserState
  // is the one named `kIOSChromeInitialBrowserState`.
  ChromeBrowserState* browser_state =
      browser_state_manager().GetLastUsedBrowserStateDeprecatedDoNotUse();

  ASSERT_TRUE(browser_state);
  EXPECT_EQ(GetChromeBrowserStateName(browser_state),
            kIOSChromeInitialBrowserState);
  EXPECT_EQ(
      GetLoadedBrowserStateNames(),
      (std::set<std::string>{kProfileName2, kIOSChromeInitialBrowserState}));
}
