// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/model/chrome_browser_state_manager_impl.h"

#import "base/containers/contains.h"
#import "base/scoped_observation.h"
#import "base/test/test_file_util.h"
#import "base/threading/thread_restrictions.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/browser_state/model/constants.h"
#import "ios/chrome/browser/browser_state/model/ios_chrome_io_thread.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_observer_ios.h"
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

// A scoped ChromeBrowserStateManagerObsever which records which events
// have been received.
class ScopedTestChromeBrowserStateManagerObserver final
    : public ChromeBrowserStateManagerObserver {
 public:
  explicit ScopedTestChromeBrowserStateManagerObserver(
      ChromeBrowserStateManager& manager) {
    scoped_observation_.Observe(&manager);
  }

  ~ScopedTestChromeBrowserStateManagerObserver() final = default;

  // Accessor for the booleans used to store which method has been called.
  bool on_chrome_browser_state_created_called() const {
    return on_chrome_browser_state_created_called_;
  }

  bool on_chrome_browser_state_loaded_called() const {
    return on_chrome_browser_state_loaded_called_;
  }

  // ChromeBrowserStateManagerObserver implementation:
  void OnChromeBrowserStateManagerDestroyed(
      ChromeBrowserStateManager* manager) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    scoped_observation_.Reset();
  }

  void OnChromeBrowserStateCreated(ChromeBrowserStateManager* manager,
                                   ChromeBrowserState* browser_state) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    on_chrome_browser_state_created_called_ = true;
  }

  void OnChromeBrowserStateLoaded(ChromeBrowserStateManager* manager,
                                  ChromeBrowserState* browser_state) final {
    DCHECK(scoped_observation_.IsObservingSource(manager));
    on_chrome_browser_state_loaded_called_ = true;
  }

 private:
  base::ScopedObservation<ChromeBrowserStateManager,
                          ChromeBrowserStateManagerObserver>
      scoped_observation_{this};

  bool on_chrome_browser_state_created_called_ = false;
  bool on_chrome_browser_state_loaded_called_ = false;
};

// Returns a callback taking a single parameter and storing it in `output`.
// The `output` must outlive the returned callback as it is captured by copy.
template <typename T>
base::OnceCallback<void(T)> CaptureParam(T* output) {
  return base::BindOnce([](T* output, T value) { *output = value; }, output);
}

}  // namespace

class ChromeBrowserStateManagerImplTest : public PlatformTest {
 public:
  ChromeBrowserStateManagerImplTest()
      : browser_state_manager_(GetApplicationContext()->GetLocalState(),
                               base::CreateUniqueTempDirectoryScopedToTest()) {
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

    // Initialize the prediction model store (required by some KeyedServices).
    optimization_guide::IOSChromePredictionModelStore::GetInstance()
        ->Initialize(base::CreateUniqueTempDirectoryScopedToTest());

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

    // Cleanup the prediction model store (since it is a singleton).
    optimization_guide::IOSChromePredictionModelStore::GetInstance()
        ->ResetForTesting();

    application_context->GetBrowserPolicyConnector()->Shutdown();
    application_context->GetIOSChromeIOThread()->NetworkTearDown();
    application_context->SetChromeBrowserStateManager(nullptr);
    application_context->SetIOSChromeIOThread(nullptr);
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
          browser_state->GetBrowserStateName();

      CHECK(!base::Contains(browser_state_names, browser_state_name));
      browser_state_names.insert(browser_state_name);
    }
    return browser_state_names;
  }

 private:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<IOSChromeIOThread> chrome_io_;
  web::WebTaskEnvironment web_task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD_DELAYED};
  ChromeBrowserStateManagerImpl browser_state_manager_;

  // Some KeyedService requires a VariationsIdsProvider to be installed.
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Tests that GetLoadedBrowserStates() returns an empty list before the
// BrowserStates are loaded, and then a list containing at least one
// BrowserState, and the last used BrowserState is loaded.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStates) {
  // There should be no BrowserState loaded yet.
  EXPECT_EQ(GetLoadedBrowserStateNames(), (std::set<std::string>{}));

  // Register an observer and check that it is correctly notified that
  // a ChromeBrowserState is created and then fully loaded.
  ScopedTestChromeBrowserStateManagerObserver observer(browser_state_manager());
  ASSERT_FALSE(observer.on_chrome_browser_state_created_called());
  ASSERT_FALSE(observer.on_chrome_browser_state_loaded_called());

  // Load the BrowserStates, this will implicitly add "Default" as a
  // BrowserState if there is no saved BrowserStates. Thus it should
  // load exactly one BrowserState.
  browser_state_manager().LoadBrowserStates();

  // Check that the observer has been notified of the creation and load.
  ASSERT_TRUE(observer.on_chrome_browser_state_created_called());
  ASSERT_TRUE(observer.on_chrome_browser_state_loaded_called());

  // Exactly one ChromeBrowserState must be loaded, it must be the last
  // used ChromeBrowserState with name `kIOSChromeInitialBrowserState`.
  ChromeBrowserState* browser_state =
      browser_state_manager().GetLastUsedBrowserStateDeprecatedDoNotUse();

  ASSERT_TRUE(browser_state);
  EXPECT_EQ(browser_state->GetBrowserStateName(),
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
  EXPECT_EQ(browser_state->GetBrowserStateName(), kProfileName1);
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
  EXPECT_EQ(browser_state->GetBrowserStateName(), kProfileName1);
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
  EXPECT_EQ(browser_state->GetBrowserStateName(),
            kIOSChromeInitialBrowserState);
  EXPECT_EQ(
      GetLoadedBrowserStateNames(),
      (std::set<std::string>{kProfileName2, kIOSChromeInitialBrowserState}));
}

// Tests that LoadBrowserStateAsync(...) correctly loads a known BrowserState,
// and that the load is not blocking the main thread.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStateAsync) {
  // Pretends that a BrowserState named `kProfileName1` exists. Required as
  // LoadBrowserStateAsync(...) won't create new BrowserStates.
  browser_state_manager().GetBrowserStateInfoCache()->AddBrowserState(
      kProfileName1, /*gaia_id=*/std::string(), /*user_name=*/std::string());

  base::RunLoop run_loop;
  ChromeBrowserState* created_browser_state = nullptr;
  ChromeBrowserState* loaded_browser_state = nullptr;

  // Load the BrowserState asynchronously while disallowing blocking on the
  // current sequence (to ensure that the method is really asynchronous and
  // does not block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = browser_state_manager().LoadBrowserStateAsync(
        kProfileName1,
        CaptureParam(&loaded_browser_state).Then(run_loop.QuitClosure()),
        CaptureParam(&created_browser_state));

    ASSERT_TRUE(success);
  }

  // The ChromeBrowserState instance should have been created but not yet
  // fully initialized (as the initialisation is asynchronous).
  EXPECT_TRUE(created_browser_state);
  EXPECT_FALSE(loaded_browser_state);

  run_loop.Run();

  // The BrowserState should have been successfully loaded and initialized.
  EXPECT_TRUE(created_browser_state);
  EXPECT_TRUE(loaded_browser_state);

  // The two callbacks were invoked with the same object.
  EXPECT_EQ(created_browser_state, loaded_browser_state);
}

// Tests that calls LoadBrowserStateAsync(...) on a loaded BrowserState return
// the BrowserState immediately and still don't block the main thread.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStateAsync_Reload) {
  // Pretends that a BrowserState named `kProfileName1` exists. Required as
  // LoadBrowserStateAsync(...) won't create new BrowserStates.
  browser_state_manager().GetBrowserStateInfoCache()->AddBrowserState(
      kProfileName1, /*gaia_id=*/std::string(), /*user_name=*/std::string());

  // Load the BrowserState a first time.
  {
    base::RunLoop run_loop;
    ChromeBrowserState* created_browser_state = nullptr;
    ChromeBrowserState* loaded_browser_state = nullptr;

    // Load the BrowserState asynchronously while disallowing blocking on the
    // current sequence (to ensure that the method is really asynchronous and
    // does not block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = browser_state_manager().LoadBrowserStateAsync(
          kProfileName1,
          CaptureParam(&loaded_browser_state).Then(run_loop.QuitClosure()),
          CaptureParam(&created_browser_state));

      ASSERT_TRUE(success);
    }

    // The ChromeBrowserState instance should have been created but not yet
    // fully initialized (as the initialisation is asynchronous).
    EXPECT_TRUE(created_browser_state);
    EXPECT_FALSE(loaded_browser_state);

    run_loop.Run();

    // The BrowserState should have been successfully loaded and initialized.
    EXPECT_TRUE(created_browser_state);
    EXPECT_TRUE(loaded_browser_state);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_browser_state, loaded_browser_state);
  }

  // Load the BrowserState a second time. Since it is already loaded, the
  // callback should be called synchronously and successfully.
  {
    base::RunLoop run_loop;
    ChromeBrowserState* created_browser_state = nullptr;
    ChromeBrowserState* loaded_browser_state = nullptr;

    // Load the BrowserState asynchronously while disallowing blocking on the
    // current sequence (to ensure that the method is really asynchronous and
    // does not block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = browser_state_manager().LoadBrowserStateAsync(
          kProfileName1,
          CaptureParam(&loaded_browser_state).Then(run_loop.QuitClosure()),
          CaptureParam(&created_browser_state));

      ASSERT_TRUE(success);
    }

    // Since the BrowserState has already been loaded, both callback should
    // be invoked synchronously.
    EXPECT_TRUE(created_browser_state);
    EXPECT_TRUE(loaded_browser_state);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_browser_state, loaded_browser_state);

    run_loop.Run();
  }
}

// Tests that LoadBrowserStateAsync(...) fails to load an unknown BrowserState.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserStateAsync_Missing) {
  // Ensures that no BrowserState named `kProfileName1` exists. This will
  // cause LoadBrowserStateAsync(...) to fail since it does not create new
  // BrowserStates.
  ASSERT_EQ(browser_state_manager()
                .GetBrowserStateInfoCache()
                ->GetIndexOfBrowserStateWithName(kProfileName1),
            std::string::npos);

  base::RunLoop run_loop;
  ChromeBrowserState* created_browser_state = nullptr;
  ChromeBrowserState* loaded_browser_state = nullptr;

  // Load the BrowserState asynchronously while disallowing blocking on the
  // current sequence (to ensure that the method is really asynchronous and
  // does not block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = browser_state_manager().LoadBrowserStateAsync(
        kProfileName1,
        CaptureParam(&loaded_browser_state).Then(run_loop.QuitClosure()),
        CaptureParam(&created_browser_state));

    ASSERT_FALSE(success);
  }

  run_loop.Run();

  // The BrowserState was not loaded nor created.
  EXPECT_FALSE(created_browser_state);
  EXPECT_FALSE(loaded_browser_state);
}

// Tests that CreatesBrowserStateAsync(...) creates and load successfully a
// new BrowserState.
TEST_F(ChromeBrowserStateManagerImplTest, CreateBrowserStateAsync) {
  // Ensures that no BrowserState named `kProfileName1` exists. This will
  // cause CreateBrowserStateAsync(...) to create a new ChromeBrowserSatet.
  ASSERT_EQ(browser_state_manager()
                .GetBrowserStateInfoCache()
                ->GetIndexOfBrowserStateWithName(kProfileName1),
            std::string::npos);

  base::RunLoop run_loop;
  ChromeBrowserState* created_browser_state = nullptr;
  ChromeBrowserState* loaded_browser_state = nullptr;

  // Load the BrowserState asynchronously while disallowing blocking on the
  // current sequence (to ensure that the method is really asynchronous and
  // does not block the sequence).
  {
    base::ScopedDisallowBlocking disallow_blocking;
    const bool success = browser_state_manager().CreateBrowserStateAsync(
        kProfileName1,
        CaptureParam(&loaded_browser_state).Then(run_loop.QuitClosure()),
        CaptureParam(&created_browser_state));

    ASSERT_TRUE(success);
  }

  // The ChromeBrowserState instance should have been created but not yet
  // fully initialized (as the initialisation is asynchronous).
  EXPECT_TRUE(created_browser_state);
  EXPECT_FALSE(loaded_browser_state);

  run_loop.Run();

  // The BrowserState should have been successfully loaded and initialized.
  EXPECT_TRUE(created_browser_state);
  EXPECT_TRUE(loaded_browser_state);

  // The two callbacks were invoked with the same object.
  EXPECT_EQ(created_browser_state, loaded_browser_state);
}

// Tests that calling CreatesBrowserStateAsync(...) a second time returns
// the BrowserState that has already been laoded.
TEST_F(ChromeBrowserStateManagerImplTest, CreateBrowserStateAsync_Reload) {
  // Ensures that no BrowserState named `kProfileName1` exists. This will
  // cause CreateBrowserStateAsync(...) to create a new ChromeBrowserSatet.
  ASSERT_EQ(browser_state_manager()
                .GetBrowserStateInfoCache()
                ->GetIndexOfBrowserStateWithName(kProfileName1),
            std::string::npos);

  // Load the BrowserState a first time.
  {
    base::RunLoop run_loop;
    ChromeBrowserState* created_browser_state = nullptr;
    ChromeBrowserState* loaded_browser_state = nullptr;

    // Load the BrowserState asynchronously while disallowing blocking on the
    // current sequence (to ensure that the method is really asynchronous and
    // does not block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = browser_state_manager().CreateBrowserStateAsync(
          kProfileName1,
          CaptureParam(&loaded_browser_state).Then(run_loop.QuitClosure()),
          CaptureParam(&created_browser_state));

      ASSERT_TRUE(success);
    }

    // The ChromeBrowserState instance should have been created but not yet
    // fully initialized (as the initialisation is asynchronous).
    EXPECT_TRUE(created_browser_state);
    EXPECT_FALSE(loaded_browser_state);

    run_loop.Run();

    // The BrowserState should have been successfully loaded and initialized.
    EXPECT_TRUE(created_browser_state);
    EXPECT_TRUE(loaded_browser_state);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_browser_state, loaded_browser_state);
  }

  // Load the BrowserState a second time. Since it is already loaded, the
  // callback should be called synchronously and successfully.
  {
    base::RunLoop run_loop;
    ChromeBrowserState* created_browser_state = nullptr;
    ChromeBrowserState* loaded_browser_state = nullptr;

    // Load the BrowserState asynchronously while disallowing blocking on the
    // current sequence (to ensure that the method is really asynchronous and
    // does not block the sequence).
    {
      base::ScopedDisallowBlocking disallow_blocking;
      const bool success = browser_state_manager().CreateBrowserStateAsync(
          kProfileName1,
          CaptureParam(&loaded_browser_state).Then(run_loop.QuitClosure()),
          CaptureParam(&created_browser_state));

      ASSERT_TRUE(success);
    }

    // Since the BrowserState has already been loaded, both callback should
    // be invoked synchronously.
    EXPECT_TRUE(created_browser_state);
    EXPECT_TRUE(loaded_browser_state);

    // The two callbacks were invoked with the same object.
    EXPECT_EQ(created_browser_state, loaded_browser_state);

    run_loop.Run();
  }
}

// Tests that LoadBrowserState(...) correctly loads a known BrowserState in
// a synchronous fashion (i.e. blocks the main thread).
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserState) {
  // Pretends that a BrowserState named `kProfileName1` exists. Required as
  // LoadBrowserState(...) won't create new BrowserStates.
  browser_state_manager().GetBrowserStateInfoCache()->AddBrowserState(
      kProfileName1, /*gaia_id=*/std::string(), /*user_name=*/std::string());

  // Load the BrowserState synchronously.
  ChromeBrowserState* browser_state =
      browser_state_manager().LoadBrowserState(kProfileName1);

  // The BrowserState should have been successfully loaded and initialized.
  EXPECT_TRUE(browser_state);

  // Calling LoadBrowserState(...) a second time should return the same
  // object.
  EXPECT_EQ(browser_state,
            browser_state_manager().LoadBrowserState(kProfileName1));
}

// Tests that LoadBrowserState(...) fails to load an unknown BrowserState.
TEST_F(ChromeBrowserStateManagerImplTest, LoadBrowserState_Missing) {
  // Ensures that no BrowserState named `kProfileName1` exists. This will
  // cause LoadBrowserState(...) to fail since it does not create new
  // BrowserStates.
  ASSERT_EQ(browser_state_manager()
                .GetBrowserStateInfoCache()
                ->GetIndexOfBrowserStateWithName(kProfileName1),
            std::string::npos);

  // Load the BrowserState synchronously.
  ChromeBrowserState* browser_state =
      browser_state_manager().LoadBrowserState(kProfileName1);

  // The BrowserState was not loaded nor created.
  EXPECT_FALSE(browser_state);
}

// Tests that CreatesBrowserState(...) creates and load successfully a new
// BrowserState in a synchronous fashion (i.e. blocks the main thread).
TEST_F(ChromeBrowserStateManagerImplTest, CreateBrowserState) {
  // Ensures that no BrowserState named `kProfileName1` exists. This will
  // cause CreateBrowserStateAsync(...) to create a new ChromeBrowserSatet.
  ASSERT_EQ(browser_state_manager()
                .GetBrowserStateInfoCache()
                ->GetIndexOfBrowserStateWithName(kProfileName1),
            std::string::npos);

  // Create the BrowserState synchronously.
  ChromeBrowserState* browser_state =
      browser_state_manager().CreateBrowserState(kProfileName1);

  // The BrowserState should have been successfully loaded and initialized.
  EXPECT_TRUE(browser_state);

  // Calling CreateBrowserState(...) a second time should return the same
  // object.
  EXPECT_EQ(browser_state,
            browser_state_manager().CreateBrowserState(kProfileName1));
}
