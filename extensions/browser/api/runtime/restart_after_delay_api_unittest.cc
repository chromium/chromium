// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/browser/test_runtime_api_delegate.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {

// A RuntimeAPIDelegate that simulates a successful restart request every time.
class DelayedRestartTestApiDelegate : public TestRuntimeAPIDelegate {
 public:
  DelayedRestartTestApiDelegate() {}
  ~DelayedRestartTestApiDelegate() override {}

  // TestRuntimeAPIDelegate:
  bool RestartDevice(std::string* error_message) override {
    if (quit_closure_)
      std::move(quit_closure_).Run();

    *error_message = "Success.";
    restart_done_ = true;

    return true;
  }

  base::TimeTicks WaitForSuccessfulRestart() {
    if (!restart_done_) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    restart_done_ = false;
    return base::TimeTicks::Now();
  }

 private:
  base::OnceClosure quit_closure_;

  bool restart_done_ = false;

  DISALLOW_COPY_AND_ASSIGN(DelayedRestartTestApiDelegate);
};

class DelayedRestartExtensionsBrowserClient
    : public TestExtensionsBrowserClient {
 public:
  DelayedRestartExtensionsBrowserClient(content::BrowserContext* context)
      : TestExtensionsBrowserClient(context) {}
  ~DelayedRestartExtensionsBrowserClient() override {}

  // TestExtensionsBrowserClient:
  PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override {
    return &testing_pref_service_;
  }

  std::unique_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const override {
    const_cast<DelayedRestartExtensionsBrowserClient*>(this)->api_delegate_ =
        new DelayedRestartTestApiDelegate();
    return base::WrapUnique(api_delegate_);
  }

  sync_preferences::TestingPrefServiceSyncable* testing_pref_service() {
    return &testing_pref_service_;
  }

  DelayedRestartTestApiDelegate* api_delegate() const {
    CHECK(api_delegate_);
    return api_delegate_;
  }

 private:
  DelayedRestartTestApiDelegate* api_delegate_ = nullptr;  // Not owned.

  sync_preferences::TestingPrefServiceSyncable testing_pref_service_;

  DISALLOW_COPY_AND_ASSIGN(DelayedRestartExtensionsBrowserClient);
};

}  // namespace

class RestartAfterDelayApiTest : public ApiUnitTest {
 public:
  RestartAfterDelayApiTest() {}
  ~RestartAfterDelayApiTest() override {}

  void SetUp() override {
    // Use our ExtensionsBrowserClient that returns our RuntimeAPIDelegate.
    std::unique_ptr<DelayedRestartExtensionsBrowserClient> test_browser_client =
        std::make_unique<DelayedRestartExtensionsBrowserClient>(
            browser_context());

    // ExtensionsTest takes ownership of the ExtensionsBrowserClient.
    SetExtensionsBrowserClient(std::move(test_browser_client));

    ApiUnitTest::SetUp();

    // The RuntimeAPI should only be accessed (i.e. constructed) after the above
    // ExtensionsBrowserClient has been setup.
    RuntimeAPI* runtime_api =
        RuntimeAPI::GetFactoryInstance()->Get(browser_context());
    runtime_api->set_min_duration_between_restarts_for_testing(
        base::TimeDelta::FromSeconds(2));
    runtime_api->AllowNonKioskAppsInRestartAfterDelayForTesting();

    RuntimeAPI::RegisterPrefs(
        static_cast<DelayedRestartExtensionsBrowserClient*>(
            extensions_browser_client())
            ->testing_pref_service()
            ->registry());
  }

  base::TimeTicks WaitForSuccessfulRestart() {
    return static_cast<DelayedRestartExtensionsBrowserClient*>(
               extensions_browser_client())
        ->api_delegate()
        ->WaitForSuccessfulRestart();
  }

  bool IsDelayedRestartTimerRunning() {
    return RuntimeAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->restart_after_delay_timer_.IsRunning();
  }

  base::TimeTicks desired_restart_time() {
    return RuntimeAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->restart_after_delay_timer_.desired_run_time();
  }

  void RunRestartAfterDelayFunction(const std::string& args,
                                    const std::string& expected_error) {
    RunRestartAfterDelayFunctionForExtention(args, extension(), expected_error);
  }

  void RunRestartAfterDelayFunctionForExtention(
      const std::string& args,
      const Extension* extension,
      const std::string& expected_error) {
    std::string error = RunFunctionGetError(
        new RuntimeRestartAfterDelayFunction(), extension, args);
    ASSERT_EQ(error, expected_error);
  }

  void RunRestartFunctionAssertNoError() {
    std::string error =
        RunFunctionGetError(new RuntimeRestartFunction(), extension(), "[]");
    ASSERT_TRUE(error.empty()) << error;
  }

 private:
  std::string RunFunctionGetError(ExtensionFunction* function,
                                  const Extension* extension,
                                  const std::string& args) {
    scoped_refptr<ExtensionFunction> function_owner(function);
    function->set_extension(extension);
    function->set_has_callback(true);
    api_test_utils::RunFunction(function, args, browser_context());
    return function->GetError();
  }

  DISALLOW_COPY_AND_ASSIGN(RestartAfterDelayApiTest);
};

TEST_F(RestartAfterDelayApiTest, RestartAfterDelayTest) {
  RunRestartAfterDelayFunction("[-1]", "");
  ASSERT_FALSE(IsDelayedRestartTimerRunning());

  RunRestartAfterDelayFunction("[-2]", "Invalid argument: -2.");

  // Request a restart after 3 seconds.
  base::TimeTicks now = base::TimeTicks::Now();
  RunRestartAfterDelayFunction("[3]", "");
  ASSERT_TRUE(IsDelayedRestartTimerRunning());
  ASSERT_GE(desired_restart_time() - now, base::TimeDelta::FromSeconds(3));

  // Request another restart after 4 seconds. It should reschedule the previous
  // request.
  now = base::TimeTicks::Now();
  RunRestartAfterDelayFunction("[4]", "");
  ASSERT_TRUE(IsDelayedRestartTimerRunning());
  ASSERT_GE(desired_restart_time() - now, base::TimeDelta::FromSeconds(4));

  // Create another extension and make it attempt to use the api, and expect a
  // failure.
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("Another App", ExtensionBuilder::Type::PLATFORM_APP)
          .SetLocation(Manifest::INTERNAL)
          .Build();
  RunRestartAfterDelayFunctionForExtention(
      "[5]", test_extension.get(), "Not the first extension to call this API.");

  // Cancel restart requests.
  RunRestartAfterDelayFunction("[-1]", "");
  ASSERT_FALSE(IsDelayedRestartTimerRunning());

  // Schedule a restart and wait for it to happen.
  now = base::TimeTicks::Now();
  RunRestartAfterDelayFunction("[1]", "");
  ASSERT_TRUE(IsDelayedRestartTimerRunning());
  ASSERT_GE(desired_restart_time() - now, base::TimeDelta::FromSeconds(1));
  base::TimeTicks last_restart_time = WaitForSuccessfulRestart();
  ASSERT_FALSE(IsDelayedRestartTimerRunning());
  ASSERT_GE(base::TimeTicks::Now() - now, base::TimeDelta::FromSeconds(1));

  // This is a restart request that will be throttled, because it happens too
  // soon after a successful restart.
  RunRestartAfterDelayFunction(
      "[1]", "Restart was requested too soon. It was throttled instead.");
  ASSERT_TRUE(IsDelayedRestartTimerRunning());
  // Restart will happen 2 seconds later, even though the request was just one
  // second.
  ASSERT_NEAR((desired_restart_time() - last_restart_time).InSecondsF(),
              base::TimeDelta::FromSeconds(2).InSecondsF(), 0.5);

  // Calling chrome.runtime.restart() will not clear the throttle, and any
  // subsequent calls to chrome.runtime.restartAfterDelay will still be
  // throttled.
  WaitForSuccessfulRestart();
  RunRestartFunctionAssertNoError();
  last_restart_time = WaitForSuccessfulRestart();
  RunRestartAfterDelayFunction(
      "[1]", "Restart was requested too soon. It was throttled instead.");
  ASSERT_TRUE(IsDelayedRestartTimerRunning());
  // Restart will happen 2 seconds later, even though the request was just one
  // second.
  ASSERT_NEAR((desired_restart_time() - last_restart_time).InSecondsF(),
              base::TimeDelta::FromSeconds(2).InSecondsF(), 0.5);
}

}  // namespace extensions
