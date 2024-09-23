// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/browser/shell_app_view_guest_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/browser/shell_extensions_browser_client.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

class MockShellAppDelegate : public extensions::ShellAppDelegate {
 public:
  MockShellAppDelegate() {
    EXPECT_EQ(instance_, nullptr);
    instance_ = this;
  }
  ~MockShellAppDelegate() override { instance_ = nullptr; }

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension) override {
    media_access_requested_ = true;
    if (media_access_request_quit_closure_) {
      std::move(media_access_request_quit_closure_).Run();
    }
  }

  void WaitForRequestMediaPermission() {
    if (media_access_requested_) {
      return;
    }
    base::RunLoop run_loop;
    media_access_request_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  static MockShellAppDelegate* Get() { return instance_; }

 private:
  bool media_access_requested_ = false;
  base::OnceClosure media_access_request_quit_closure_;

  static MockShellAppDelegate* instance_;
};

MockShellAppDelegate* MockShellAppDelegate::instance_ = nullptr;

class MockShellAppViewGuestDelegate
    : public extensions::ShellAppViewGuestDelegate {
 public:
  MockShellAppViewGuestDelegate() = default;

  extensions::AppDelegate* CreateAppDelegate(
      content::BrowserContext* browser_context) override {
    return new MockShellAppDelegate();
  }
};

class MockExtensionsAPIClient : public extensions::ShellExtensionsAPIClient {
 public:
  MockExtensionsAPIClient() = default;

  extensions::AppViewGuestDelegate* CreateAppViewGuestDelegate()
      const override {
    return new MockShellAppViewGuestDelegate();
  }
};

}  // namespace

namespace extensions {

class AppViewTest : public AppShellTest {
 protected:
  AppViewTest() = default;

  content::WebContents* GetFirstAppWindowWebContents() {
    const AppWindowRegistry::AppWindowList& app_window_list =
        AppWindowRegistry::Get(browser_context_)->app_windows();
    EXPECT_EQ(1U, app_window_list.size());
    return (*app_window_list.begin())->web_contents();
  }

  const Extension* LoadApp(const std::string& app_location) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    base::PathService::Get(DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII(app_location.c_str());
    return extension_system_->LoadApp(test_data_dir);
  }

  void RunTest(const std::string& test_name,
               const std::string& app_location,
               const std::string& app_to_embed) {
    const Extension* app_embedder = LoadApp(app_location);
    ASSERT_TRUE(app_embedder);
    const Extension* app_embedded = LoadApp(app_to_embed);
    ASSERT_TRUE(app_embedded);

    extension_system_->LaunchApp(app_embedder->id());

    ExtensionTestMessageListener launch_listener("LAUNCHED");
    ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

    ExtensionTestMessageListener done_listener("TEST_PASSED");
    done_listener.set_failure_message("TEST_FAILED");
    ASSERT_TRUE(content::ExecJs(
        GetFirstAppWindowWebContents(),
        base::StringPrintf("runTest('%s', '%s')", test_name.c_str(),
                           app_embedded->id().c_str())))
        << "Unable to start test.";
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }
};

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewGoodDataShouldSucceed) {
  RunTest("testAppViewGoodDataShouldSucceed",
          "app_view/apitest",
          "app_view/apitest/skeleton");
}

// Tests that <appview> can handle media permission requests.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewMediaRequest) {
  static_cast<ShellExtensionsBrowserClient*>(ExtensionsBrowserClient::Get())
      ->SetAPIClientForTest(nullptr);
  static_cast<ShellExtensionsBrowserClient*>(ExtensionsBrowserClient::Get())
      ->SetAPIClientForTest(new MockExtensionsAPIClient);

  RunTest("testAppViewMediaRequest", "app_view/apitest",
          "app_view/apitest/media_request");

  MockShellAppDelegate::Get()->WaitForRequestMediaPermission();
}

// Tests that <appview> correctly processes parameters passed on connect.
// This test should fail to connect because the embedded app (skeleton) will
// refuse the data passed by the embedder app and deny the request.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewRefusedDataShouldFail) {
  RunTest("testAppViewRefusedDataShouldFail",
          "app_view/apitest",
          "app_view/apitest/skeleton");
}

// Tests that <appview> is able to navigate to another installed app.
IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewWithUndefinedDataShouldSucceed) {
  RunTest("testAppViewWithUndefinedDataShouldSucceed",
          "app_view/apitest",
          "app_view/apitest/skeleton");
}

IN_PROC_BROWSER_TEST_F(AppViewTest, TestAppViewNoEmbedRequestListener) {
  RunTest("testAppViewNoEmbedRequestListener", "app_view/apitest",
          "app_view/apitest/no_embed_request_listener");
}

}  // namespace extensions
