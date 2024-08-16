// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_frame_host.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/shell/browser/shell_extension_host_delegate.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"
#include "extensions/shell/browser/shell_extensions_browser_client.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestExtensionFrameHost : public ExtensionFrameHost {
 public:
  explicit TestExtensionFrameHost(content::WebContents* web_contents)
      : ExtensionFrameHost(web_contents) {}
  TestExtensionFrameHost(const TestExtensionFrameHost&) = delete;
  TestExtensionFrameHost& operator=(const TestExtensionFrameHost&) = delete;
  ~TestExtensionFrameHost() override = default;

  void SetInvalidRequest(const std::string& name) { invalid_request_ = name; }

 private:
  // mojom::LocalFrameHost:
  void Request(mojom::RequestParamsPtr params,
               RequestCallback callback) override {
    // If the name of |params| is set to an invalid request, it sets it to
    // an empty string so that the request causes an error.
    if (invalid_request_ == params->name) {
      params->name = std::string();
    }
    ExtensionFrameHost::Request(std::move(params), std::move(callback));
  }

  std::string invalid_request_;
};

class TestShellExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<
          TestShellExtensionWebContentsObserver> {
 public:
  TestShellExtensionWebContentsObserver(
      const TestShellExtensionWebContentsObserver&) = delete;
  TestShellExtensionWebContentsObserver& operator=(
      const TestShellExtensionWebContentsObserver&) = delete;
  ~TestShellExtensionWebContentsObserver() override = default;

  // Creates and initializes an instance of this class for the given
  // |web_contents|, if it doesn't already exist.
  static void CreateForWebContents(content::WebContents* web_contents) {
    content::WebContentsUserData<TestShellExtensionWebContentsObserver>::
        CreateForWebContents(web_contents);
    // Initialize this instance if necessary.
    FromWebContents(web_contents)->Initialize();
  }

  // Overrides to create TestExtensionFrameHost.
  std::unique_ptr<ExtensionFrameHost> CreateExtensionFrameHost(
      content::WebContents* web_contents) override {
    return std::make_unique<TestExtensionFrameHost>(web_contents);
  }

 private:
  friend class content::WebContentsUserData<
      TestShellExtensionWebContentsObserver>;

  explicit TestShellExtensionWebContentsObserver(
      content::WebContents* web_contents)
      : ExtensionWebContentsObserver(web_contents),
        content::WebContentsUserData<TestShellExtensionWebContentsObserver>(
            *web_contents) {}

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TestShellExtensionWebContentsObserver);

class TestShellExtensionHostDelegate : public ShellExtensionHostDelegate {
 public:
  TestShellExtensionHostDelegate() = default;
  TestShellExtensionHostDelegate(const TestShellExtensionHostDelegate&) =
      delete;
  TestShellExtensionHostDelegate& operator=(
      const TestShellExtensionHostDelegate&) = delete;
  ~TestShellExtensionHostDelegate() override = default;

  // Overrides to create TestShellExtensionWebContentsObserver.
  void OnExtensionHostCreated(content::WebContents* web_contents) override {
    TestShellExtensionWebContentsObserver::CreateForWebContents(web_contents);
  }
};

class ExtensionFrameHostTestExtensionsBrowserClient
    : public ShellExtensionsBrowserClient {
 public:
  ExtensionFrameHostTestExtensionsBrowserClient() = default;
  ExtensionFrameHostTestExtensionsBrowserClient(
      const ExtensionFrameHostTestExtensionsBrowserClient&) = delete;
  ExtensionFrameHostTestExtensionsBrowserClient& operator=(
      const ExtensionFrameHostTestExtensionsBrowserClient&) = delete;
  ~ExtensionFrameHostTestExtensionsBrowserClient() override = default;

  // Overrides to create TestShellExtensionHostDelegate.
  std::unique_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate()
      override {
    return std::make_unique<TestShellExtensionHostDelegate>();
  }

  // Overrides to return TestShellExtensionWebContentsObserver.
  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override {
    return TestShellExtensionWebContentsObserver::FromWebContents(web_contents);
  }
};

}  // namespace

class ExtensionFrameHostBrowserTest : public ShellApiTest {
 public:
  ExtensionFrameHostBrowserTest() = default;
  ExtensionFrameHostBrowserTest(const ExtensionFrameHostBrowserTest&) = delete;
  ExtensionFrameHostBrowserTest& operator=(
      const ExtensionFrameHostBrowserTest&) = delete;
  ~ExtensionFrameHostBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    extensions_browser_client_ =
        std::make_unique<ExtensionFrameHostTestExtensionsBrowserClient>();
    extensions_browser_client_->InitWithBrowserContext(
        browser_context(),
        ExtensionPrefs::Get(browser_context())->pref_service());
    ExtensionsBrowserClient::Set(extensions_browser_client_.get());

    extension_ = LoadExtension("extension");
    ASSERT_TRUE(extension_.get());
    ResultCatcher catcher;
    ASSERT_TRUE(catcher.GetNextResult());
  }

 protected:
  const Extension* extension() const { return extension_.get(); }

  void SetInvalidNameOnRequest(const std::string& method_name) {
    ExtensionHost* host =
        ProcessManager::Get(browser_context())
            ->GetBackgroundHostForExtension(extension()->id());
    ASSERT_TRUE(host);
    ASSERT_TRUE(host->host_contents());
    ExtensionWebContentsObserver* observer =
        extensions_browser_client_->GetExtensionWebContentsObserver(
            host->host_contents());
    static_cast<TestExtensionFrameHost*>(
        observer->extension_frame_host_for_testing())
        ->SetInvalidRequest(method_name);
  }

 private:
  scoped_refptr<const Extension> extension_;
  std::unique_ptr<ExtensionFrameHostTestExtensionsBrowserClient>
      extensions_browser_client_;
};

// Test that when ExtensionFrameHost dispatches an invalid request it gets
// an error associated with it. This is a regression test for
// https://crbug.com/1196377.
IN_PROC_BROWSER_TEST_F(ExtensionFrameHostBrowserTest, InValidNameRequest) {
  // Set 'test.getConfig' is invalid request.
  SetInvalidNameOnRequest("test.getConfig");
  // Run a script asynchronously that passes the test.
  ResultCatcher catcher;
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser_context(), extension()->id(), R"(
        chrome.test.getConfig(() => {
          const expectedError = 'Access to extension API denied.';
          if (chrome.runtime.lastError &&
            expectedError == chrome.runtime.lastError.message) {
            chrome.test.notifyPass();
          } else {
            chrome.test.notifyFail('TestFailed');
          }
        });)"));

  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace extensions
