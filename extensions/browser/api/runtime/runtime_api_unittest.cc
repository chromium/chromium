// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.runtime extension API.

#include "extensions/browser/api/runtime/runtime_api.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/common/child_process_id.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/events/lazy_event_dispatch_util.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionRuntimeTest : public ApiUnitTest {
 protected:
  using ApiUnitTest::RunFunction;

  // Call runtime.setUninstallURL() and verify that the call succeeds and
  // the pref store is updated.
  void SetUninstallURL(const std::string& url) {
    RunFunction(base::MakeRefCounted<RuntimeSetUninstallURLFunction>(),
                "[\"" + url + "\"]");

    // Verify that the URL was properly written to the pref store.
    EXPECT_EQ(url, GetUninstallURL());
  }

  // Call runtime.setUninstallURL() and verify that the call throws an error and
  // the pref store is not affected.
  void SetUninstallURLError(const std::string& url) {
    std::string original_url = GetUninstallURL();
    EXPECT_EQ("Invalid URL: \"" + url + "\".",
              RunFunctionAndReturnError(
                  base::MakeRefCounted<RuntimeSetUninstallURLFunction>(),
                  "[\"" + url + "\"]"));

    // Verify that the pref store was not affected.
    EXPECT_EQ(original_url, GetUninstallURL());
  }

  std::string GetUninstallURL() {
    std::string url;
    ExtensionPrefs::Get(browser_context())
        ->ReadPrefAsString(extension()->id(), "uninstall_url", &url);
    return url;
  }
};

TEST_F(ExtensionRuntimeTest, SetUninstallURL) {
  // By default extensions should have no uninstall URLs.
  EXPECT_EQ("", GetUninstallURL());

  SetUninstallURL("https://example.com");

  // Empty URL string is accepted (to remove uninstall URL).
  SetUninstallURL("");

  // URL parameters are accepted.
  SetUninstallURL("https://example.com/abcd?param=efg");

  // Trailing spaces are accepted.
  SetUninstallURL("https://other.example.com/page   ");

  // Leading spaces are accepted.
  SetUninstallURL("   https://other.example.com/some_page");

  // HTTP URLs are accepted.
  SetUninstallURL("http://insecure.example/path");

  // Ensure that only HTTP and HTTPS resources are accepted.
  SetUninstallURLError("ws://impossible");
  SetUninstallURLError("wss://impossible");
  SetUninstallURLError("about:blank");
  SetUninstallURLError("chrome://settings");
  SetUninstallURLError("://example.com");
}

// Unit tests for runtime.markListenerRegistrationComplete() (API availability
// and main-thread errors). Browser tests cover service worker success paths.
class RuntimeMarkListenerRegistrationCompleteTest : public ApiUnitTest {
 public:
  void SetUp() override {
    ApiUnitTest::SetUp();
    set_extension(BuildExtension(/*opted_in=*/true));
  }

 protected:
  // Builds an MV3 service worker extension. `opted_in` sets
  // `background.async_listener_registration`.
  scoped_refptr<const Extension> BuildExtension(bool opted_in) {
    ExtensionBuilder builder("async listener registration");
    builder.SetManifestVersion(3).SetBackgroundContext(
        ExtensionBuilder::BackgroundContext::SERVICE_WORKER);
    if (opted_in) {
      builder.SetManifestPath("background.async_listener_registration", true);
    }
    return builder.Build();
  }

  // Simulates calling the function from an untracked service worker.
  scoped_refptr<ExtensionFunction> CreateFunctionFromWorker() {
    auto function =
        base::MakeRefCounted<RuntimeMarkListenerRegistrationCompleteFunction>();
    function->set_worker_id(
        WorkerId(extension()->id(), content::ChildProcessId::FromUnsafeValue(1),
                 /*version_id=*/1, /*thread_id=*/1));
    return function;
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      extensions_features::kExtensionAsyncListenerRegistration};
};

// The function fails when not called from a service worker.
TEST_F(RuntimeMarkListenerRegistrationCompleteTest, NotFromServiceWorker) {
  EXPECT_THAT(RunFunctionAndReturnError(
                  base::MakeRefCounted<
                      RuntimeMarkListenerRegistrationCompleteFunction>(),
                  "[]"),
              testing::HasSubstr("service worker"));
}

// The function fails for an extension that does not declare the
// "background.async_listener_registration" manifest key.
TEST_F(RuntimeMarkListenerRegistrationCompleteTest, NotOptedIn) {
  set_extension(BuildExtension(/*opted_in=*/false));
  EXPECT_THAT(RunFunctionAndReturnError(CreateFunctionFromWorker(), "[]"),
              testing::HasSubstr("background.async_listener_registration"));
}

// A completion from an untracked worker fails, with or without a current
// activation.
TEST_F(RuntimeMarkListenerRegistrationCompleteTest, UntrackedWorkerFails) {
  // No current activation.
  EXPECT_THAT(RunFunctionAndReturnError(CreateFunctionFromWorker(), "[]"),
              testing::HasSubstr("No listener registration is in progress"));

  // A current activation, but no tracked worker.
  ExtensionRegistry::Get(browser_context())->AddEnabled(extension());
  ServiceWorkerTaskQueue::Get(browser_context())
      ->ActivateExtension(extension());
  EXPECT_THAT(RunFunctionAndReturnError(CreateFunctionFromWorker(), "[]"),
              testing::HasSubstr("No listener registration is in progress"));
}

// Test fixture for `runtime.onExtensionLoaded` event dispatching logic,
// parameterized over all `api::runtime::OnLoadedReason` values.
class RuntimeOnExtensionLoadedEventTest
    : public ApiUnitTest,
      public testing::WithParamInterface<api::runtime::OnLoadedReason> {
 public:
  void SetUp() override {
    ApiUnitTest::SetUp();
    // Ensure `EventRouter` is created for `browser_context()`.
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<EventRouter>(context,
                                               ExtensionPrefs::Get(context));
        }));
    // Register the extension as enabled in
    // `ExtensionRegistry::enabled_extensions()`.
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension());
  }

 protected:
  // Adds a lazy event listener for `runtime.onExtensionLoaded` to
  // `EventRouter`.
  void AddOnExtensionLoadedListener() {
    auto listener = EventListener::CreateLazyListener(
        api::runtime::OnExtensionLoaded::kEventName, extension()->id(),
        browser_context(), /*is_for_service_worker=*/false,
        /*service_worker_scope=*/GURL(), /*filter=*/std::nullopt);
    EventRouter::Get(browser_context())
        ->listeners()
        .AddListener(std::move(listener));
  }
};

// Verifies that `details.previous_version` is populated when the load reason is
// `kUpdate` or `kEnable` and omitted for all other `OnLoadedReason` values,
// even when a `previous_version` string is supplied.
TEST_P(RuntimeOnExtensionLoadedEventTest, PreviousVersionHandling) {
  TestEventRouterObserver observer(EventRouter::Get(browser_context()));
  AddOnExtensionLoadedListener();

  const api::runtime::OnLoadedReason reason = GetParam();

  // Dispatch `runtime.onExtensionLoaded` with `reason` and a `previous_version`
  // string.
  RuntimeEventRouter::DispatchOnExtensionLoadedEvent(
      browser_context(), extension()->id(), reason,
      /*previous_version=*/"1.0");

  // Verify the dispatched event payload.
  ASSERT_TRUE(
      observer.events().contains(api::runtime::OnExtensionLoaded::kEventName));
  const Event* dispatched_event =
      observer.events().at(api::runtime::OnExtensionLoaded::kEventName).get();
  ASSERT_TRUE(dispatched_event);
  ASSERT_EQ(1u, dispatched_event->args().size());
  ASSERT_TRUE(dispatched_event->args()[0].is_dict());
  const base::DictValue& details_dict = dispatched_event->args()[0].GetDict();
  EXPECT_EQ(api::runtime::ToString(reason), *details_dict.FindString("reason"));

  if (reason == api::runtime::OnLoadedReason::kUpdate ||
      reason == api::runtime::OnLoadedReason::kEnable) {
    EXPECT_EQ("1.0", *details_dict.FindString("previousVersion"));
  } else {
    EXPECT_EQ(nullptr, details_dict.FindString("previousVersion"));
  }
}

// Verifies that `runtime.onExtensionLoaded` is not dispatched for any
// `OnLoadedReason` when the extension has no registered listener, avoiding
// waking up dormant service workers or creating unnecessary external requests,
// and is dispatched once a listener is registered.
TEST_P(RuntimeOnExtensionLoadedEventTest,
       EventListenerFilteringAvoidsWakingDormantWorkers) {
  TestEventRouterObserver observer(EventRouter::Get(browser_context()));
  const api::runtime::OnLoadedReason reason = GetParam();
  const std::optional<std::string> previous_version =
      (reason == api::runtime::OnLoadedReason::kUpdate ||
       reason == api::runtime::OnLoadedReason::kEnable)
          ? std::make_optional<std::string>("1.0")
          : std::nullopt;

  // With no listener registered, verify early return without dispatching.
  RuntimeEventRouter::DispatchOnExtensionLoadedEvent(
      browser_context(), extension()->id(), reason, previous_version);
  EXPECT_FALSE(
      observer.events().contains(api::runtime::OnExtensionLoaded::kEventName));

  // Register a listener and verify that the event now dispatches.
  AddOnExtensionLoadedListener();
  RuntimeEventRouter::DispatchOnExtensionLoadedEvent(
      browser_context(), extension()->id(), reason, previous_version);
  EXPECT_TRUE(
      observer.events().contains(api::runtime::OnExtensionLoaded::kEventName));
}

// Verifies that `runtime.onExtensionLoaded` is not dispatched if the extension
// is not present in `ExtensionRegistry::enabled_extensions()`.
TEST_P(RuntimeOnExtensionLoadedEventTest,
       DispatchIgnoredForDisabledOrMissingExtension) {
  TestEventRouterObserver observer(EventRouter::Get(browser_context()));
  AddOnExtensionLoadedListener();
  const api::runtime::OnLoadedReason reason = GetParam();
  const std::optional<std::string> previous_version =
      (reason == api::runtime::OnLoadedReason::kUpdate ||
       reason == api::runtime::OnLoadedReason::kEnable)
          ? std::make_optional<std::string>("1.0")
          : std::nullopt;

  // Remove the extension from `ExtensionRegistry::enabled_extensions()`.
  ExtensionRegistry::Get(browser_context())->RemoveEnabled(extension()->id());

  // Attempt to dispatch for `reason`.
  RuntimeEventRouter::DispatchOnExtensionLoadedEvent(
      browser_context(), extension()->id(), reason, previous_version);

  // Verify that the event was not dispatched because the extension is not
  // enabled.
  EXPECT_FALSE(
      observer.events().contains(api::runtime::OnExtensionLoaded::kEventName));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RuntimeOnExtensionLoadedEventTest,
    testing::Values(api::runtime::OnLoadedReason::kInstall,
                    api::runtime::OnLoadedReason::kUpdate,
                    api::runtime::OnLoadedReason::kBrowserUpdate,
                    api::runtime::OnLoadedReason::kEnable,
                    api::runtime::OnLoadedReason::kStartup,
                    api::runtime::OnLoadedReason::kReload),
    [](const testing::TestParamInfo<api::runtime::OnLoadedReason>& info) {
      return std::string(api::runtime::ToString(info.param));
    });

// Test fixture that sends `RuntimeAPI` the `LazyEventDispatchUtil::Observer`
// and `ExtensionRegistryObserver` notifications for an install, update, unload
// or enable, to verify which `runtime.onExtensionLoaded` reason `RuntimeAPI`
// dispatches when more than one of them applies (see
// `RuntimeEventRouter::DispatchOnExtensionLoadedEvent()` in `runtime_api.h`
// for the precedence of reasons).
class RuntimeAPIOnExtensionLoadedPrecedenceTest : public ApiUnitTest {
 public:
  void SetUp() override {
    ApiUnitTest::SetUp();
    // Ensure `EventRouter` is created for `browser_context()`.
    EventRouterFactory::GetInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<EventRouter>(context,
                                               ExtensionPrefs::Get(context));
        }));

    // Create `RuntimeAPI`. `RuntimeAPI::kServiceIsNULLWhileTesting` stops its
    // factory from creating it in tests, so use a testing factory.
    runtime_api_ =
        RuntimeAPI::GetFactoryInstance()->SetTestingSubclassFactoryAndUse(
            browser_context(),
            base::BindOnce([](content::BrowserContext* context) {
              return std::make_unique<RuntimeAPI>(context);
            }));
    ASSERT_TRUE(runtime_api_);

    // `RunPostedDispatchTasks()` relies on each `runtime.onExtensionLoaded`
    // dispatch reaching `EventRouter` synchronously from its posted task. That
    // holds only if the extension has no lazy background context, since
    // `RuntimeEventRouter::DispatchOnExtensionLoadedEvent()` would otherwise
    // queue `kInstall` and `kUpdate` on the `LazyContextTaskQueue`.
    ASSERT_FALSE(BackgroundInfo::HasLazyContext(extension()));

    // Register the extension as enabled and add a lazy listener for
    // `runtime.onExtensionLoaded`, so that the dispatched events reach
    // `EventRouter`.
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension());
    EventRouter::Get(browser_context())
        ->listeners()
        .AddListener(EventListener::CreateLazyListener(
            api::runtime::OnExtensionLoaded::kEventName, extension()->id(),
            browser_context(), /*is_for_service_worker=*/false,
            /*service_worker_scope=*/GURL(), /*filter=*/std::nullopt));
  }

  void TearDown() override {
    runtime_api_ = nullptr;
    ApiUnitTest::TearDown();
  }

 protected:
  // Sends `RuntimeAPI` the notification that `LazyEventDispatchUtil` sends when
  // the extension loads after it was installed or updated. `previous_version`
  // is invalid for a fresh install.
  void NotifyInstalledAndLoaded(const base::Version& previous_version) {
    LazyEventDispatchUtil::Observer* observer = runtime_api_;
    observer->OnExtensionInstalledAndLoaded(browser_context(), extension(),
                                            previous_version);
  }

  // Sends `RuntimeAPI` the notification that `ExtensionRegistry` sends when the
  // extension is unloaded because it was disabled.
  void NotifyUnloaded() {
    ExtensionRegistryObserver* observer = runtime_api_;
    observer->OnExtensionUnloaded(browser_context(), extension(),
                                  UnloadedExtensionReason::DISABLE);
  }

  // Sends `RuntimeAPI` the notification that `ExtensionRegistry` sends when the
  // extension is enabled.
  void NotifyEnabled() {
    ExtensionRegistryObserver* observer = runtime_api_;
    observer->OnExtensionEnabled(browser_context(), extension());
  }

  // Runs, in order, the tasks that the `Notify*()` calls posted to the current
  // thread, and returns once they have all run. `RuntimeAPI` posts each
  // `runtime.onExtensionLoaded` dispatch as one task to
  // `base::SingleThreadTaskRunner::GetCurrentDefault()`, and each of those
  // tasks dispatches to `EventRouter` synchronously. Posting the quit closure
  // after them means that once this returns:
  // - Every event that the notifications dispatched has been recorded by
  //   `TestEventRouterObserver`.
  // - Any event missing from `TestEventRouterObserver` was not dispatched, so
  //   the negative expectations don't depend on a timeout.
  void RunPostedDispatchTasks() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Returns the `details.reason` and `details.previousVersion` (or an empty
  // string if it's absent) of each `runtime.onExtensionLoaded` event that
  // `observer` saw, in dispatch order.
  static std::vector<std::pair<std::string, std::string>>
  GetOnExtensionLoadedDetails(const TestEventRouterObserver& observer) {
    std::vector<std::pair<std::string, std::string>> details;
    for (const std::unique_ptr<Event>& event : observer.all_events()) {
      if (event->event_name != api::runtime::OnExtensionLoaded::kEventName) {
        continue;
      }
      const base::DictValue& details_dict = event->args()[0].GetDict();
      const std::string* previous_version =
          details_dict.FindString("previousVersion");
      details.emplace_back(*details_dict.FindString("reason"),
                           previous_version ? *previous_version : "");
    }
    return details;
  }

 private:
  raw_ptr<RuntimeAPI> runtime_api_ = nullptr;
};

// Verifies that when an extension that was installed while disabled is
// enabled, `runtime.onExtensionLoaded` is dispatched only with reason
// `kInstall`, since `kInstall` takes precedence over `kEnable`.
TEST_F(RuntimeAPIOnExtensionLoadedPrecedenceTest,
       EnableAfterInstallDispatchesOnlyInstall) {
  TestEventRouterObserver observer(EventRouter::Get(browser_context()));

  // Send the notifications in the order that
  // `ExtensionRegistrar::EnableExtension()` triggers them for an extension that
  // was installed while disabled, then let the posted dispatches run.
  NotifyInstalledAndLoaded(/*previous_version=*/base::Version());
  NotifyEnabled();
  RunPostedDispatchTasks();

  // Verify that only `kInstall` was dispatched.
  EXPECT_THAT(GetOnExtensionLoadedDetails(observer),
              testing::ElementsAre(testing::Pair("install", "")));
}

// Verifies that when an extension that was updated while disabled is enabled,
// `runtime.onExtensionLoaded` is dispatched only with reason `kUpdate` and
// the version from before the update, since `kUpdate` takes precedence over
// `kEnable`.
TEST_F(RuntimeAPIOnExtensionLoadedPrecedenceTest,
       EnableAfterUpdateDispatchesOnlyUpdate) {
  TestEventRouterObserver observer(EventRouter::Get(browser_context()));

  // Send the notifications in the order that
  // `ExtensionRegistrar::EnableExtension()` triggers them for an extension that
  // was updated while disabled, then let the posted dispatches run.
  NotifyInstalledAndLoaded(/*previous_version=*/base::Version("0.0.1"));
  NotifyEnabled();
  RunPostedDispatchTasks();

  // Verify that only `kUpdate` was dispatched, with the pre-update version.
  EXPECT_THAT(GetOnExtensionLoadedDetails(observer),
              testing::ElementsAre(testing::Pair("update", "0.0.1")));
}

// Verifies that if an extension is installed and then unloaded and enabled
// again before the posted `kInstall` dispatch runs:
// - `RuntimeAPI::OnExtensionUnloaded()` drops the pending `kInstall`.
// - `runtime.onExtensionLoaded` is dispatched only with reason `kEnable` and
//   the extension's current version.
TEST_F(RuntimeAPIOnExtensionLoadedPrecedenceTest,
       EnableAfterInstallThenUnloadDispatchesOnlyEnable) {
  TestEventRouterObserver observer(EventRouter::Get(browser_context()));

  // Install the extension, then unload and enable it before any posted task
  // runs, then let the posted dispatches run.
  NotifyInstalledAndLoaded(/*previous_version=*/base::Version());
  NotifyUnloaded();
  NotifyEnabled();
  RunPostedDispatchTasks();

  // Verify that only `kEnable` was dispatched, with the current version.
  EXPECT_THAT(GetOnExtensionLoadedDetails(observer),
              testing::ElementsAre(
                  testing::Pair("enable", extension()->version().GetString())));
}

// Verifies that if an extension is updated and then unloaded and enabled
// again before the posted `kUpdate` dispatch runs:
// - `RuntimeAPI::OnExtensionUnloaded()` drops the pending `kUpdate`.
// - `runtime.onExtensionLoaded` is dispatched only with reason `kEnable` and
//   the extension's current version, not the version from before the update.
TEST_F(RuntimeAPIOnExtensionLoadedPrecedenceTest,
       EnableAfterUpdateThenUnloadDispatchesOnlyEnable) {
  TestEventRouterObserver observer(EventRouter::Get(browser_context()));

  // Update the extension, then unload and enable it before any posted task
  // runs, then let the posted dispatches run.
  NotifyInstalledAndLoaded(/*previous_version=*/base::Version("0.0.1"));
  NotifyUnloaded();
  NotifyEnabled();
  RunPostedDispatchTasks();

  // Verify that only `kEnable` was dispatched, with the current version.
  EXPECT_THAT(GetOnExtensionLoadedDetails(observer),
              testing::ElementsAre(
                  testing::Pair("enable", extension()->version().GetString())));
}

}  // namespace extensions
