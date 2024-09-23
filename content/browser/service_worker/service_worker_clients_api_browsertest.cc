// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/test_content_browser_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "url/gurl.h"

namespace content {

// Tests for the service worker Clients API.
class ServiceWorkerClientsApiBrowserTest : public ContentBrowserTest {
 public:
  ServiceWorkerClientsApiBrowserTest() = default;

  ServiceWorkerClientsApiBrowserTest(
      const ServiceWorkerClientsApiBrowserTest&) = delete;
  ServiceWorkerClientsApiBrowserTest& operator=(
      const ServiceWorkerClientsApiBrowserTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();

    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
};

// Tests a successful WindowClient.navigate() call.
IN_PROC_BROWSER_TEST_F(ServiceWorkerClientsApiBrowserTest, Navigate) {
  // Load a page that registers a service worker.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('client_api_worker.js');"));

  // Load the page again so we are controlled.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ(true, EvalJs(shell(), "!!navigator.serviceWorker.controller"));

  // Have the service worker call client.navigate() on the page.
  const std::string navigate_script = R"(
    (async () => {
      const registration = await navigator.serviceWorker.ready;
      registration.active.postMessage({command: 'navigate', url: 'empty.html'});
      return true;
    })();
  )";
  EXPECT_EQ(true, EvalJs(shell(), navigate_script));

  // The page should be navigated to empty.html.
  const std::u16string title = u"ServiceWorker test - empty page";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

// Tests a WindowClient.navigate() call during a browser-initiated navigation.
// Regression test for https://crbug.com/930154.
IN_PROC_BROWSER_TEST_F(ServiceWorkerClientsApiBrowserTest,
                       NavigateDuringBrowserNavigation) {
  // Load a page that registers a service worker.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('client_api_worker.js');"));

  // Load the test page.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/service_worker/request_navigate.html")));

  // Start a browser-initiated navigation.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  TestNavigationManager navigation(shell()->web_contents(), url);
  shell()->LoadURL(url);
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // Have the service worker call client.navigate() to try to go to another
  // URL. It should fail.
  EXPECT_EQ("navigate failed", EvalJs(shell(), "requestToNavigate();"));

  // The browser-initiated navigation should finish.
  ASSERT_TRUE(navigation.WaitForNavigationFinished());  // Resume navigation.
  EXPECT_TRUE(navigation.was_successful());
}

// Tests a successful Clients.openWindow() call.
IN_PROC_BROWSER_TEST_F(ServiceWorkerClientsApiBrowserTest, OpenWindow) {
  // Load a page that registers a service worker.
  GURL page_url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('client_api_worker.js');"));

  // Tell the service worker to call clients.openWindow(target_url). Do it from
  // a notification click event so it has a user interaction token that allows
  // popups.
  content::WebContents* new_window = nullptr;
  GURL target_url =
      embedded_test_server()->GetURL("/service_worker/empty.html");
  {
    GURL scope_url = embedded_test_server()->GetURL("/service_worker/");
    blink::PlatformNotificationData notification_data;
    notification_data.body = base::UTF8ToUTF16(target_url.spec());

    content::WebContentsAddedObserver new_window_observer;
    content::DispatchServiceWorkerNotificationClick(wrapper(), scope_url,
                                                    notification_data);
    new_window = new_window_observer.GetWebContents();
  }

  // Verify that the new window has navigated successfully.
  content::TestNavigationObserver nav_observer(new_window, 1);
  nav_observer.Wait();
  EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(target_url, nav_observer.last_navigation_url());
}

}  // namespace content
