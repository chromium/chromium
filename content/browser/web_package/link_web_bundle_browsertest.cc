// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kUrnUuidURL[] = "urn:uuid:429fcc4e-0696-4bad-b099-ee9175f023ae";

class TestBrowserClient : public ContentBrowserClient {
 public:
  TestBrowserClient() = default;
  ~TestBrowserClient() override = default;
  bool HandleExternalProtocol(
      const GURL& url,
      base::OnceCallback<WebContents*()> web_contents_getter,
      int child_id,
      NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override {
    EXPECT_FALSE(observed_url_.has_value());
    observed_url_ = url;
    return true;
  }

  GURL observed_url() const { return observed_url_ ? *observed_url_ : GURL(); }

 private:
  base::Optional<GURL> observed_url_;
};

class FinishNavigationObserver : public WebContentsObserver {
 public:
  FinishNavigationObserver() = default;
  ~FinishNavigationObserver() override = default;
  explicit FinishNavigationObserver(WebContents* contents,
                                    const GURL& expected_url,
                                    base::OnceClosure done_closure)
      : WebContentsObserver(contents),
        expected_url_(expected_url),
        done_closure_(std::move(done_closure)) {}

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() == expected_url_) {
      error_code_ = navigation_handle->GetNetErrorCode();
      std::move(done_closure_).Run();
    }
  }

  const base::Optional<net::Error>& error_code() const { return error_code_; }

 private:
  GURL expected_url_;
  base::OnceClosure done_closure_;
  base::Optional<net::Error> error_code_;
};

}  // namespace

class LinkWebBundleBrowserTest : public ContentBrowserTest {
 protected:
  LinkWebBundleBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSubresourceWebBundles);
  }
  ~LinkWebBundleBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&browser_client_);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ContentBrowserTest::TearDownOnMainThread();
    SetBrowserClientForTesting(original_client_);
  }

  void CreateIframeAndWaitForOnload(const std::string& url) {
    DOMMessageQueue dom_message_queue(shell()->web_contents());
    std::string message;
    ExecuteScriptAsync(
        shell(),
        "let iframe = document.createElement('iframe');"
        "iframe.src = '" +
            url +
            "';"
            "iframe.onload = function() {"
            "   window.domAutomationController.send('iframe.onload');"
            "};"
            "document.body.appendChild(iframe);");
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  GURL GetObservedUnknownSchemeUrl() { return browser_client_.observed_url(); }

 private:
  ContentBrowserClient* original_client_ = nullptr;
  TestBrowserClient browser_client_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LinkWebBundleBrowserTest, SubframeLoad) {
  GURL url(embedded_test_server()->GetURL("/web_bundle/link_web_bundle.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Create an iframe with a urn:uuid resource in a bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUrnUuidURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(),
      "let iframe = document.createElement('iframe');"
      "iframe.src = 'urn:uuid:429fcc4e-0696-4bad-b099-ee9175f023ae';"
      "document.body.appendChild(iframe);");
  run_loop.Run();
  EXPECT_EQ(net::OK, *finish_navigation_observer.error_code());
}

IN_PROC_BROWSER_TEST_F(LinkWebBundleBrowserTest, FollowLink) {
  GURL url(embedded_test_server()->GetURL("/web_bundle/link_web_bundle.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Clicking a link to a urn:uuid resource in a bundle should not be loaded
  // from the bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUrnUuidURL), run_loop.QuitClosure());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('link').click();"));
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUrnUuidURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(LinkWebBundleBrowserTest, IframeChangeSource) {
  GURL main_url(embedded_test_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe whose document has <link rel="webbundle">.
  CreateIframeAndWaitForOnload("/web_bundle/link_web_bundle.html");

  // Attempt to navigate the iframe to a bundled resource.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUrnUuidURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(), "iframe.src = 'urn:uuid:429fcc4e-0696-4bad-b099-ee9175f023ae';");
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUrnUuidURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(LinkWebBundleBrowserTest, IframeFollowLink) {
  GURL main_url(embedded_test_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe whose document has <link rel="webbundle">.
  CreateIframeAndWaitForOnload("/web_bundle/link_web_bundle.html");

  // Click a link inside the iframe. The resource should not be loaded from
  // the bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUrnUuidURL), run_loop.QuitClosure());
  ExecuteScriptAsync(shell(),
                     "iframe.contentDocument.getElementById('link').click();");
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUrnUuidURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(LinkWebBundleBrowserTest, NavigationFromSiblingFrame) {
  GURL main_url(
      embedded_test_server()->GetURL("/web_bundle/link_web_bundle.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe and wait for the initial load.
  DOMMessageQueue dom_message_queue(shell()->web_contents());
  std::string message;
  ExecuteScriptAsync(shell(),
                     "let iframe1 = document.createElement('iframe');"
                     "iframe1.name = 'iframe1';"
                     "iframe1.src = '/simple_page.html';"
                     "iframe1.onload = function() {"
                     "   window.domAutomationController.send('iframe1.onload');"
                     "};"
                     "document.body.appendChild(iframe1);");
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"iframe1.onload\"", message);

  // Create another iframe and wait for the initial load.
  ExecuteScriptAsync(shell(),
                     "let iframe2 = document.createElement('iframe');"
                     "iframe2.name = 'iframe2';"
                     "iframe2.src = '/simple_page.html';"
                     "iframe2.onload = function() {"
                     "   window.domAutomationController.send('iframe2.onload');"
                     "};"
                     "document.body.appendChild(iframe2);");
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"iframe2.onload\"", message);

  // Navigate iframe2 to a urn:uuid URL by clicking a link in iframe1, which
  // should not be loaded from the WebBundle associated with the parent
  // document.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUrnUuidURL), run_loop.QuitClosure());
  ExecuteScriptAsync(shell(),
                     "let a = iframe1.contentDocument.createElement('a');"
                     "a.href = 'urn:uuid:429fcc4e-0696-4bad-b099-ee9175f023ae';"
                     "a.target = 'iframe2';"
                     "iframe1.contentDocument.body.appendChild(a);"
                     "a.click();");
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUrnUuidURL), GetObservedUnknownSchemeUrl());
}

IN_PROC_BROWSER_TEST_F(LinkWebBundleBrowserTest,
                       GrandChildShouldNotBeLoadedFromBundle) {
  GURL main_url(
      embedded_test_server()->GetURL("/web_bundle/link_web_bundle.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Create an iframe with a urn:uuid resource, which has a nested iframe with
  // another urn:uuid resource in the bundle. The resource for the nested should
  // iframe not be loaded from the bundle.
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(
      shell()->web_contents(), GURL(kUrnUuidURL), run_loop.QuitClosure());
  ExecuteScriptAsync(
      shell(),
      "let iframe = document.createElement('iframe');"
      "iframe.src = 'urn:uuid:1084e1fc-2122-4155-a4dd-28efb2e8ccb1';"
      "document.body.appendChild(iframe);");
  run_loop.Run();
  EXPECT_EQ(net::ERR_ABORTED, *finish_navigation_observer.error_code());
  EXPECT_EQ(GURL(kUrnUuidURL), GetObservedUnknownSchemeUrl());
}

}  // namespace content
