// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.runtime extension API.

#include "extensions/browser/api/runtime/runtime_api.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/child_process_id.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/service_worker/service_worker_task_queue.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"

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

}  // namespace extensions
