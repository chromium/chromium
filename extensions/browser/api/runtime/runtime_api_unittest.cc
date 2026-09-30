// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.runtime extension API.

#include "extensions/browser/api/runtime/runtime_api.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/public/common/child_process_id.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
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
// `kUpdate` and omitted for all other `OnLoadedReason` values, even when a
// `previous_version` string is supplied.
// TODO(crbug.com/550447466): Follow up on whether `details.previous_version`
// should also be populated when `reason` is `kEnable`.
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

  if (reason == api::runtime::OnLoadedReason::kUpdate) {
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
      reason == api::runtime::OnLoadedReason::kUpdate
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
      reason == api::runtime::OnLoadedReason::kUpdate
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

}  // namespace extensions
