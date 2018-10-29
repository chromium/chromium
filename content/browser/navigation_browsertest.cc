// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_provisional_load_interceptor.h"
#include "ipc/ipc_security_test_util.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace content {

namespace {

class InterceptAndCancelDidCommitProvisionalLoad
    : public DidCommitProvisionalLoadInterceptor {
 public:
  explicit InterceptAndCancelDidCommitProvisionalLoad(WebContents* web_contents)
      : DidCommitProvisionalLoadInterceptor(web_contents) {}
  ~InterceptAndCancelDidCommitProvisionalLoad() override {}

  void Wait(size_t number_of_messages) {
    while (intercepted_messages_.size() < number_of_messages) {
      loop_.reset(new base::RunLoop);
      loop_->Run();
    }
  }

  const std::vector<::FrameHostMsg_DidCommitProvisionalLoad_Params>&
  intercepted_messages() const {
    return intercepted_messages_;
  }

  std::vector<::service_manager::mojom::InterfaceProviderRequest>&
  intercepted_requests() {
    return intercepted_requests_;
  }

 protected:
  bool WillDispatchDidCommitProvisionalLoad(
      RenderFrameHost* render_frame_host,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      service_manager::mojom::InterfaceProviderRequest*
          interface_provider_request) override {
    intercepted_messages_.push_back(*params);
    intercepted_requests_.push_back(std::move(*interface_provider_request));
    if (loop_)
      loop_->Quit();
    // Do not send the message to the RenderFrameHostImpl.
    return false;
  }

  std::vector<::FrameHostMsg_DidCommitProvisionalLoad_Params>
      intercepted_messages_;
  std::vector<::service_manager::mojom::InterfaceProviderRequest>
      intercepted_requests_;
  std::unique_ptr<base::RunLoop> loop_;
};

// Record every WebContentsObserver's event related to navigation. The goal is
// to check these events happen and happen in the expected right order.
class NavigationRecorder : public WebContentsObserver {
 public:
  NavigationRecorder(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // WebContentsObserver implementation.
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    records_.push_back("start " + navigation_handle->GetURL().path());
    WakeUp();
  }
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    records_.push_back("ready-to-commit " + navigation_handle->GetURL().path());
    WakeUp();
  }
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    records_.push_back("did-commit " + navigation_handle->GetURL().path());
    WakeUp();
  }

  void WaitForEvents(size_t numbers_of_events) {
    while (records_.size() < numbers_of_events) {
      loop_.reset(new base::RunLoop);
      loop_->Run();
      loop_.reset();
    }
  }

  const std::vector<std::string> records() { return records_; }

 private:
  void WakeUp() {
    if (loop_)
      loop_->Quit();
  }

  std::unique_ptr<base::RunLoop> loop_;
  std::vector<std::string> records_;
};

// Used to wait for an observed IPC to be received.
class BrowserMessageObserver : public content::BrowserMessageFilter {
 public:
  BrowserMessageObserver(uint32_t observed_message_class,
                         uint32_t observed_message_type)
      : content::BrowserMessageFilter(observed_message_class),
        observed_message_type_(observed_message_type) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == observed_message_type_)
      loop.Quit();
    return false;
  }

  void Wait() { loop.Run(); }

 private:
  ~BrowserMessageObserver() override {}
  uint32_t observed_message_type_;
  base::RunLoop loop;
  DISALLOW_COPY_AND_ASSIGN(BrowserMessageObserver);
};

}  // namespace

// Test about navigation.
// If you don't need a custom embedded test server, please use the next class
// below (NavigationBrowserTest), it will automatically start the
// default server.
class NavigationBaseBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

class NavigationBrowserTest : public NavigationBaseBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    NavigationBaseBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Ensure that browser initiated basic navigations work.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, BrowserInitiatedNavigations) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  // Perform a same site navigation.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should not have changed.
  EXPECT_EQ(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()
                             ->root()
                             ->current_frame_host());

  // Perform a cross-site navigation.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url = embedded_test_server()->GetURL("foo.com", "/title3.html");
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should have changed.
  EXPECT_NE(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()
                             ->root()
                             ->current_frame_host());
}

// Ensure that renderer initiated same-site navigations work.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       RendererInitiatedSameSiteNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/simple_links.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  // Simulate clicking on a same-site link.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "window.domAutomationController.send(clickSameSiteLink());",
        &success));
    EXPECT_TRUE(success);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should not have changed.
  EXPECT_EQ(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()
                             ->root()
                             ->current_frame_host());
}

// Ensure that renderer initiated cross-site navigations work.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       RendererInitiatedCrossSiteNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/simple_links.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()
          ->root()
          ->current_frame_host();

  // Simulate clicking on a cross-site link.
  {
    TestNavigationObserver observer(shell()->web_contents());
    const char kReplacePortNumber[] =
        "window.domAutomationController.send(setPortNumber(%d));";
    uint16_t port_number = embedded_test_server()->port();
    GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf(kReplacePortNumber, port_number),
        &success));
    success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "window.domAutomationController.send(clickCrossSiteLink());",
        &success));
    EXPECT_TRUE(success);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should not have changed unless site-per-process is
  // enabled.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(initial_rfh,
              static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetFrameTree()
                  ->root()
                  ->current_frame_host());
  } else {
    EXPECT_EQ(initial_rfh,
              static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetFrameTree()
                  ->root()
                  ->current_frame_host());
  }
}

// Ensure navigation failures are handled.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, FailedNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Now navigate to an unreachable url.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL error_url(embedded_test_server()->GetURL("/close-socket"));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));
    NavigateToURL(shell(), error_url);
    EXPECT_EQ(error_url, observer.last_navigation_url());
    NavigationEntry* entry =
        shell()->web_contents()->GetController().GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
  }
}

// Ensure that browser initiated navigations to view-source URLs works.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       ViewSourceNavigation_BrowserInitiated) {
  TestNavigationObserver observer(shell()->web_contents());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL view_source_url(content::kViewSourceScheme + std::string(":") +
                       url.spec());
  NavigateToURL(shell(), view_source_url);
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

// Ensure that content initiated navigations to view-sources URLs are blocked.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       ViewSourceNavigation_RendererInitiated) {
  TestNavigationObserver observer(shell()->web_contents());
  GURL kUrl(embedded_test_server()->GetURL("/simple_links.html"));
  NavigateToURL(shell(), kUrl);
  EXPECT_EQ(kUrl, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Not allowed to load local resource: view-source:about:blank"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickViewSourceLink());", &success));
  EXPECT_TRUE(success);
  console_delegate->Wait();
  // Original page shouldn't navigate away.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetURL());
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetController()
                   .GetLastCommittedEntry()
                   ->IsViewSourceMode());
}

// Ensure that closing a page by running its beforeunload handler doesn't hang
// if there's an ongoing navigation.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, UnloadDuringNavigation) {
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(shell()->web_contents()));
  GURL url("chrome://resources/css/tabs.css");
  NavigationHandleObserver handle_observer(shell()->web_contents(), url);
  shell()->LoadURL(url);
  shell()->web_contents()->DispatchBeforeUnload(false /* auto_cancel */);
  close_observer.Wait();
  EXPECT_EQ(net::ERR_ABORTED, handle_observer.net_error_code());
}

// Ensure that the referrer of a navigation is properly sanitized.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SanitizeReferrer) {
  const GURL kInsecureUrl(embedded_test_server()->GetURL("/title1.html"));
  const Referrer kSecureReferrer(
      GURL("https://secure-url.com"),
      network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade);
  ShellNetworkDelegate::SetCancelURLRequestWithPolicyViolatingReferrerHeader(
      true);

  // Navigate to an insecure url with a secure referrer with a policy of no
  // referrer on downgrades. The referrer url should be rewritten right away.
  NavigationController::LoadURLParams load_params(kInsecureUrl);
  load_params.referrer = kSecureReferrer;
  TestNavigationManager manager(shell()->web_contents(), kInsecureUrl);
  shell()->web_contents()->GetController().LoadURLWithParams(load_params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // The referrer should have been sanitized.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  ASSERT_TRUE(root->navigation_request());
  EXPECT_EQ(GURL(),
            root->navigation_request()->navigation_handle()->GetReferrer().url);

  // The navigation should commit without being blocked.
  EXPECT_TRUE(manager.WaitForResponse());
  manager.WaitForNavigationFinished();
  EXPECT_EQ(kInsecureUrl, shell()->web_contents()->GetLastCommittedURL());
}

// Test to verify that an exploited renderer process trying to upload a file
// it hasn't been explicitly granted permissions to is correctly terminated.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, PostUploadIllegalFilePath) {
  GURL form_url(
      embedded_test_server()->GetURL("/form_that_posts_to_echoall.html"));
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());

  // Prepare a file for the upload form.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  std::string file_content("test-file-content");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
  ASSERT_LT(
      0, base::WriteFile(file_path, file_content.data(), file_content.size()));

  // Fill out the form to refer to the test file.
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('file').click();"));
  EXPECT_TRUE(delegate->file_chosen());

  // Ensure that the process is allowed to access to the chosen file and
  // does not have access to the other file name.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      rfh->GetProcess()->GetID(), file_path));

  // Revoke the access to the file and submit the form. The renderer process
  // should be terminated.
  RenderProcessHostKillWaiter process_kill_waiter(rfh->GetProcess());
  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->RevokeAllPermissionsForFile(rfh->GetProcess()->GetID(),
                                               file_path);

  // Use ExecuteScriptAndExtractBool and respond back to the browser process
  // before doing the actual submission. This will ensure that the process
  // termination is guaranteed to arrive after the response from the executed
  // JavaScript.
  bool result = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(true);"
      "document.getElementById('file-form').submit();",
      &result));
  EXPECT_TRUE(result);
  EXPECT_EQ(bad_message::RFH_ILLEGAL_UPLOAD_PARAMS, process_kill_waiter.Wait());
}

// Test case to verify that redirects to data: URLs are properly disallowed,
// even when invoked through a reload.
// See https://crbug.com/723796.
//
// Note: This is PlzNavigate specific test, as the behavior of reloads in the
// non-PlzNavigate path differs. The WebURLRequest for the reload is generated
// based on Blink's state instead of the history state in the browser process,
// which ends up loading the originally blocked URL. With PlzNavigate, the
// reload uses the NavigationEntry state to create a navigation and commit it.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       VerifyBlockedErrorPageURL_Reload) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // Navigate to an URL, which redirects to a data: URL, since it is an
  // unsafe redirect and will result in a blocked navigation and error page.
  GURL redirect_to_blank_url(
      embedded_test_server()->GetURL("/server-redirect?data:text/html,Hello!"));
  EXPECT_FALSE(NavigateToURL(shell(), redirect_to_blank_url));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());

  TestNavigationObserver reload_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell(), "location.reload()"));
  reload_observer.Wait();

  // The expectation is that the blocked URL is present in the NavigationEntry,
  // and shows up in both GetURL and GetVirtualURL.
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->GetURL().SchemeIs(url::kDataScheme));
  EXPECT_EQ(redirect_to_blank_url,
            controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(redirect_to_blank_url,
            controller.GetLastCommittedEntry()->GetVirtualURL());
}

class NavigationDisableWebSecurityTest : public NavigationBrowserTest {
 public:
  NavigationDisableWebSecurityTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Simulate a compromised renderer, otherwise the cross-origin request to
    // file: is blocked.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    NavigationBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test to verify that an exploited renderer process trying to specify a
// non-empty URL for base_url_for_data_url on navigation is correctly
// terminated.
// TODO(nasko): This test case belongs better in
// security_exploit_browsertest.cc, so move it there once PlzNavigate is on
// by default.
IN_PROC_BROWSER_TEST_F(NavigationDisableWebSecurityTest,
                       ValidateBaseUrlForDataUrl) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());

  GURL data_url("data:text/html,foo");
  base::FilePath file_path = GetTestFilePath("", "simple_page.html");
  GURL file_url = net::FilePathToFileURL(file_path);

  // To get around DataUrlNavigationThrottle. Other attempts at getting around
  // it don't work, i.e.:
  // -if the request is made in a child frame then the frame is torn down
  // immediately on process killing so the navigation doesn't complete
  // -if it's classified as same document, then a DCHECK in
  // NavigationRequest::CreateRendererInitiated fires
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAllowContentInitiatedDataUrlNavigations);
  // Setup a BeginNavigate IPC with non-empty base_url_for_data_url.
  CommonNavigationParams common_params(
      data_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT, true /* allow_download */,
      false /* should_replace_current_entry */,
      file_url, /* base_url_for_data_url */
      GURL() /* history_url_for_data_url */, PREVIEWS_UNSPECIFIED,
      base::TimeTicks::Now() /* navigation_start */, "GET",
      nullptr /* post_data */, base::Optional<SourceLocation>(),
      false /* started_from_context_menu */, false /* has_user_gesture */,
      InitiatorCSPInfo());
  mojom::BeginNavigationParamsPtr begin_params =
      mojom::BeginNavigationParams::New(
          std::string() /* headers */, net::LOAD_NORMAL,
          false /* skip_service_worker */,
          blink::mojom::RequestContextType::LOCATION,
          blink::WebMixedContentContextType::kBlockable,
          false /* is_form_submission */, GURL() /* searchable_form_url */,
          std::string() /* searchable_form_encoding */,
          url::Origin::Create(data_url), GURL() /* client_side_redirect_url */,
          base::nullopt /* devtools_initiator_info */);

  // Receiving the invalid IPC message should lead to renderer process
  // termination.
  RenderProcessHostKillWaiter process_kill_waiter(rfh->GetProcess());

  mojom::NavigationClientAssociatedPtr navigation_client;
  if (IsPerNavigationMojoInterfaceEnabled()) {
    auto navigation_client_request =
        mojo::MakeRequestAssociatedWithDedicatedPipe(&navigation_client);
    rfh->frame_host_binding_for_testing().impl()->BeginNavigation(
        common_params, std::move(begin_params), nullptr,
        navigation_client.PassInterface(), nullptr);
  } else {
    rfh->frame_host_binding_for_testing().impl()->BeginNavigation(
        common_params, std::move(begin_params), nullptr, nullptr, nullptr);
  }
  EXPECT_EQ(bad_message::RFH_BASE_URL_FOR_DATA_URL_SPECIFIED,
            process_kill_waiter.Wait());

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      rfh->GetProcess()->GetID(), file_path));

  // Reload the page to create another renderer process.
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  tab_observer.Wait();

  // Make an XHR request to check if the page has access.
  std::string script = base::StringPrintf(
      "var xhr = new XMLHttpRequest()\n"
      "xhr.open('GET', '%s', false);\n"
      "xhr.send();\n"
      "window.domAutomationController.send(xhr.responseText);",
      file_url.spec().c_str());
  std::string result;
  EXPECT_TRUE(
      ExecuteScriptAndExtractString(shell()->web_contents(), script, &result));
  EXPECT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, BackFollowedByReload) {
  // First, make two history entries.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  NavigateToURL(shell(), url1);
  NavigateToURL(shell(), url2);

  // Then execute a back navigation in Javascript followed by a reload.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.back(); location.reload();"));
  navigation_observer.Wait();

  // The reload should have cancelled the back navigation, and the last
  // committed URL should still be the second URL.
  EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
}

// Test that a navigation response can be entirely fetched, even after the
// NavigationURLLoader has been deleted.
IN_PROC_BROWSER_TEST_F(NavigationBaseBrowserTest,
                       FetchResponseAfterNavigationURLLoaderDeleted) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a new document.
  GURL url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // The navigation starts.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // A NavigationRequest exists at this point.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  EXPECT_TRUE(root->navigation_request());

  // The response's headers are received.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "...");
  EXPECT_TRUE(navigation_manager.WaitForResponse());
  navigation_manager.ResumeNavigation();

  // The renderer commits the navigation and the browser deletes its
  // NavigationRequest.
  navigation_manager.WaitForNavigationFinished();
  EXPECT_FALSE(root->navigation_request());

  // The NavigationURLLoader has been deleted by now. Check that the renderer
  // can still receive more bytes.
  DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(
      shell()->web_contents()->GetMainFrame()));
  response.Send(
      "<script>window.domAutomationController.send('done');</script>");
  std::string done;
  EXPECT_TRUE(dom_message_queue.WaitForMessage(&done));
  EXPECT_EQ("\"done\"", done);
}

// Navigation are started in the browser process. After the headers are
// received, the URLLoaderClient is transferred from the browser process to the
// renderer process. This test ensures that when the the URLLoader is deleted
// (in the browser process), the URLLoaderClient (in the renderer process) stops
// properly.
IN_PROC_BROWSER_TEST_F(NavigationBaseBrowserTest,
                       CancelRequestAfterReadyToCommit) {
  // This test cancels the request using the ResourceDispatchHost. With the
  // NetworkService, it is not used so the request is not canceled.
  // TODO(arthursonzogni): Find a way to cancel a request from the browser
  // with the NetworkService.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "/main_document");
  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Load a new document. Commit the navigation but do not send the full
  //    response's body.
  GURL url(embedded_test_server()->GetURL("/main_document"));
  TestNavigationManager navigation_manager(shell()->web_contents(), url);
  shell()->LoadURL(url);

  // Let the navigation start.
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();

  // The server sends the first part of the response and waits.
  response.WaitForRequest();
  response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<html><body> ... ");

  EXPECT_TRUE(navigation_manager.WaitForResponse());
  GlobalRequestID global_id =
      navigation_manager.GetNavigationHandle()->GetGlobalRequestID();
  navigation_manager.ResumeNavigation();

  // The navigation commits successfully. The renderer is waiting for the
  // response's body.
  navigation_manager.WaitForNavigationFinished();

  // 2) The ResourceDispatcherHost cancels the request.
  auto cancel_request = [](GlobalRequestID global_id) {
    ResourceDispatcherHostImpl* rdh =
        static_cast<ResourceDispatcherHostImpl*>(ResourceDispatcherHost::Get());
    rdh->CancelRequest(global_id.child_id, global_id.request_id);
  };
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(cancel_request, global_id));

  // 3) Check that the load stops properly.
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

// Data URLs can have a reference fragment like any other URLs. This test makes
// sure it is taken into account.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, DataURLWithReferenceFragment) {
  GURL url("data:text/html,body#foo");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::string body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send(document.body.textContent);",
      &body));
  // TODO(arthursonzogni): This is wrong. The correct value for |body| is
  // "body", not "body#foo". See https://crbug.com/123004.
  EXPECT_EQ("body#foo", body);

  std::string reference_fragment;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(), "window.domAutomationController.send(location.hash);",
      &reference_fragment));
  EXPECT_EQ("#foo", reference_fragment);
}

// Regression test for https://crbug.com/796561.
// 1) Start on a document with history.length == 1.
// 2) Create an iframe and call history.pushState at the same time.
// 3) history.back() must work.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       IframeAndPushStateSimultaneously) {
  GURL main_url = embedded_test_server()->GetURL("/simple_page.html");
  GURL iframe_url = embedded_test_server()->GetURL("/hello.html");

  // 1) Start on a new document such that history.length == 1.
  {
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    int history_length;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        shell(), "window.domAutomationController.send(history.length)",
        &history_length));
    EXPECT_EQ(1, history_length);
  }

  // 2) Create an iframe and call history.pushState at the same time.
  {
    TestNavigationManager iframe_navigation(shell()->web_contents(),
                                            iframe_url);
    ExecuteScriptAsync(shell(),
                       "let iframe = document.createElement('iframe');"
                       "iframe.src = '/hello.html';"
                       "document.body.appendChild(iframe);");
    EXPECT_TRUE(iframe_navigation.WaitForRequestStart());

    // The iframe navigation is paused. In the meantime, a pushState navigation
    // begins and ends.
    TestNavigationManager push_state_navigation(shell()->web_contents(),
                                                main_url);
    ExecuteScriptAsync(shell(), "window.history.pushState({}, null);");
    push_state_navigation.WaitForNavigationFinished();

    // The iframe navigation is resumed.
    iframe_navigation.WaitForNavigationFinished();
  }

  // 3) history.back() must work.
  {
    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell()->web_contents(), "history.back();"));
    navigation_observer.Wait();
  }
}

// Regression test for https://crbug.com/260144
// Back/Forward navigation in an iframe must not stop ongoing XHR.
IN_PROC_BROWSER_TEST_F(NavigationBaseBrowserTest,
                       IframeNavigationsDoNotStopXHR) {
  // A response for the XHR request. It will be delayed until the end of all the
  // navigations.
  net::test_server::ControllableHttpResponse xhr_response(
      embedded_test_server(), "/xhr");
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);

  DOMMessageQueue dom_message_queue(WebContents::FromRenderFrameHost(
      shell()->web_contents()->GetMainFrame()));
  std::string message;

  // 1) Send an XHR.
  ExecuteScriptAsync(
      shell(),
      "let xhr = new XMLHttpRequest();"
      "xhr.open('GET', './xhr', true);"
      "xhr.onabort = () => window.domAutomationController.send('xhr.onabort');"
      "xhr.onerror = () => window.domAutomationController.send('xhr.onerror');"
      "xhr.onload = () => window.domAutomationController.send('xhr.onload');"
      "xhr.send();");

  // 2) Create an iframe and wait for the initial load.
  {
    ExecuteScriptAsync(
        shell(),
        "var iframe = document.createElement('iframe');"
        "iframe.src = './title1.html';"
        "iframe.onload = function() {"
        "   window.domAutomationController.send('iframe.onload');"
        "};"
        "document.body.appendChild(iframe);");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 3) Navigate the iframe elsewhere.
  {
    ExecuteScriptAsync(shell(),
                       "var iframe = document.querySelector('iframe');"
                       "iframe.src = './title2.html';");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 4) history.back() in the iframe.
  {
    ExecuteScriptAsync(shell(),
                       "var iframe = document.querySelector('iframe');"
                       "iframe.contentWindow.history.back()");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 5) history.forward() in the iframe.
  {
    ExecuteScriptAsync(shell(),
                       "var iframe = document.querySelector('iframe');"
                       "iframe.contentWindow.history.forward()");

    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"iframe.onload\"", message);
  }

  // 6) Wait for the XHR.
  {
    xhr_response.WaitForRequest();
    xhr_response.Send(
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Length: 2\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "\r\n"
        "OK");
    xhr_response.Done();
    EXPECT_TRUE(dom_message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"xhr.onload\"", message);
  }

  EXPECT_FALSE(dom_message_queue.PopMessage(&message));
}

// Regression test for https://crbug.com/856396.
IN_PROC_BROWSER_TEST_F(NavigationBaseBrowserTest,
                       ReplacingDocumentLoaderFiresLoadEvent) {
  net::test_server::ControllableHttpResponse main_document_response(
      embedded_test_server(), "/main_document");
  net::test_server::ControllableHttpResponse iframe_response(
      embedded_test_server(), "/iframe");

  ASSERT_TRUE(embedded_test_server()->Start());

  // 1) Load the main document.
  shell()->LoadURL(embedded_test_server()->GetURL("/main_document"));
  main_document_response.WaitForRequest();
  main_document_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "<script>"
      "  var detach_iframe = function() {"
      "    var iframe = document.querySelector('iframe');"
      "    iframe.parentNode.removeChild(iframe);"
      "  }"
      "</script>"
      "<body onload='detach_iframe()'>"
      "  <iframe src='/iframe'></iframe>"
      "</body>");
  main_document_response.Done();

  // 2) The iframe starts to load, but the server only have time to send the
  // response's headers, not the response's body. A provisional DocumentLoader
  // will be created in the renderer process, but it will never commit.
  iframe_response.WaitForRequest();
  iframe_response.Send(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n");

  // 3) In the meantime the iframe navigates elsewhere. It causes the previous
  // provisional DocumentLoader to be replaced by the new one. Removing it may
  // trigger the 'load' event and delete the iframe.
  EXPECT_TRUE(ExecuteScript(
      shell(), "document.querySelector('iframe').src = '/title1.html'"));

  // Wait for the iframe to be deleted and check the renderer process is still
  // alive.
  int iframe_count = 1;
  while (iframe_count != 0) {
    ASSERT_TRUE(ExecuteScriptAndExtractInt(
        shell(),
        "var iframe_count = document.getElementsByTagName('iframe').length;"
        "window.domAutomationController.send(iframe_count);",
        &iframe_count));
  }
}

class NavigationDownloadBrowserTest : public NavigationBaseBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    NavigationBaseBrowserTest::SetUpOnMainThread();

    // Set up a test download directory, in order to prevent prompting for
    // handling downloads.
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

 private:
  base::ScopedTempDir downloads_directory_;
};

// Regression test for https://crbug.com/855033
// 1) A page contains many scripts and DOM elements. It forces the parser to
//    yield CPU to other tasks. That way the response body's data are not fully
//    read when URLLoaderClient::OnComplete(..) is received.
// 2) A script makes the document navigates elsewhere while it is still loading.
//    It cancels the parser of the current document. Due to a bug, the document
//    loader was not marked to be 'loaded' at this step.
// 3) The request for the new navigation starts and it turns out it is a
//    download. The navigation is dropped.
// 4) There are no more possibilities for DidStopLoading() to be sent.
IN_PROC_BROWSER_TEST_F(NavigationDownloadBrowserTest,
                       StopLoadingAfterDroppedNavigation) {
  net::test_server::ControllableHttpResponse main_response(
      embedded_test_server(), "/main");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL main_url(embedded_test_server()->GetURL("/main"));
  GURL download_url(embedded_test_server()->GetURL("/download-test1.lib"));

  shell()->LoadURL(main_url);
  main_response.WaitForRequest();
  std::string headers =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n";

  // Craft special HTML to make the blink::DocumentParser yield CPU to other
  // tasks. The goal is to ensure the response body datapipe is not fully read
  // when URLLoaderClient::OnComplete() is called.
  // This relies on the  HTMLParserScheduler::ShouldYield() heuristics.
  std::string mix_of_script_and_div = "<script></script><div></div>";
  for (size_t i = 0; i < 10; ++i) {
    mix_of_script_and_div += mix_of_script_and_div;  // Exponential growth.
  }

  std::string navigate_to_download =
      "<script>location.href='" + download_url.spec() + "'</script>";

  main_response.Send(headers + navigate_to_download + mix_of_script_and_div);
  main_response.Done();

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
}

// This test reproduces the following race condition:
// 1) A first navigation starts, the headers are received, the navigation
//    reaches ready-to-commit. It is sent to the renderer to be committed.
// 2) In the meantime, a second navigation reaches ready-to-commit in the
//    browser.
// 3) Before the renderer gets notified of the new navigation, the
//    first navigation is committed.
// 4) The browser gets notified of the commit of the first navigation. This
//    should not destroy the NavigationRequest corresponding to the second
//    navigation.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       RaceNewNavigationCommitWhileOldOneFinishesLoading) {
  // Start the test with an initial document.
  GURL main_url(embedded_test_server()->GetURL("/simple_page.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  RenderFrameHostImpl* render_frame_host = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());

  NavigationRecorder recorder(shell()->web_contents());
  // Note: These two pages contain an image that will never load. The goal is to
  // prevent RenderFrameHostImpl::DidStopLoading() to be called since it will
  // cancel any pending navigation.
  GURL page_1(embedded_test_server()->GetURL("/infinite_load_1.html"));
  GURL page_2(embedded_test_server()->GetURL("/infinite_load_2.html"));
  // Intercept and cancel any FrameMsgHost_DidCommitProvisionalLoad events.
  InterceptAndCancelDidCommitProvisionalLoad interceptor(
      shell()->web_contents());

  // 1) Navigate to page_1.
  shell()->LoadURL(page_1);

  // 2) The browser receives the response's headers. The navigation commits in
  //    the browser.
  recorder.WaitForEvents(2);
  EXPECT_EQ(2u, recorder.records().size());
  EXPECT_STREQ("start /infinite_load_1.html", recorder.records()[0].c_str());
  EXPECT_STREQ("ready-to-commit /infinite_load_1.html",
               recorder.records()[1].c_str());

  // 3) Wait for the renderer to receive the response's body, but do not notify
  //    the browser of it right now. It is delayed in 6).
  interceptor.Wait(1);
  EXPECT_EQ(1u, interceptor.intercepted_messages().size());

  // 4) In the meantime, the browser starts a navigation to page_2.
  shell()->LoadURL(page_2);

  // 5) The response's headers are received, the navigation reaches
  // ready-to-commit in the browser. This should not delete the ongoing
  // NavigationRequest.
  recorder.WaitForEvents(4);
  EXPECT_EQ(4u, recorder.records().size());
  EXPECT_STREQ("start /infinite_load_2.html", recorder.records()[2].c_str());
  EXPECT_STREQ("ready-to-commit /infinite_load_2.html",
               recorder.records()[3].c_str());

  // 6) The browser receives the first DidCommitProvisionalLoad message. This
  //    should not delete the second navigation. This is the end of the first
  //    navigation.
  render_frame_host->DidCommitProvisionalLoadForTesting(
      std::make_unique<::FrameHostMsg_DidCommitProvisionalLoad_Params>(
          interceptor.intercepted_messages()[0]),
      std::move(interceptor.intercepted_requests()[0]));
  recorder.WaitForEvents(5);
  EXPECT_EQ(5u, recorder.records().size());
  EXPECT_STREQ("did-commit /infinite_load_1.html",
               recorder.records()[4].c_str());

  // 7) Wait for the renderer to receive the second response's body. This is the
  //    end of the second navigation.
  interceptor.Wait(2);
  EXPECT_EQ(2u, interceptor.intercepted_messages().size());
  render_frame_host->DidCommitProvisionalLoadForTesting(
      std::make_unique<::FrameHostMsg_DidCommitProvisionalLoad_Params>(
          interceptor.intercepted_messages()[1]),
      std::move(interceptor.intercepted_requests()[1]));
  recorder.WaitForEvents(6);
  EXPECT_EQ(6u, recorder.records().size());
  EXPECT_STREQ("did-commit /infinite_load_2.html",
               recorder.records()[5].c_str());
}

// Renderer initiated back/forward navigation in beforeunload should not prevent
// the user to navigate away from a website.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HistoryBackInBeforeUnload) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "onbeforeunload = function() {"
                            "  history.pushState({}, null, '/');"
                            "  history.back();"
                            "};"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
}

// Same as 'HistoryBackInBeforeUnload', but wraps history.back() inside
// window.setTimeout(). Thus it is executed "outside" of its beforeunload
// handler and thus avoid basic navigation circumventions.
// Regression test for: https://crbug.com/879965.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       HistoryBackInBeforeUnloadAfterSetTimeout) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "onbeforeunload = function() {"
                            "  history.pushState({}, null, '/');"
                            "  setTimeout(()=>history.back());"
                            "};"));
  TestNavigationManager navigation(shell()->web_contents(), url_2);
  auto ipc_observer = base::MakeRefCounted<BrowserMessageObserver>(
      ViewMsgStart, ViewHostMsg_GoToEntryAtOffset::ID);
  static_cast<RenderFrameHostImpl*>(shell()->web_contents()->GetMainFrame())
      ->GetProcess()
      ->AddFilter(ipc_observer.get());

  shell()->LoadURL(url_2);
  ipc_observer->Wait();
  navigation.WaitForNavigationFinished();

  EXPECT_TRUE(navigation.was_successful());
}

// Renderer initiated back/forward navigation can't cancel an ongoing browser
// initiated navigation if it is not user initiated.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       HistoryBackCancelPendingNavigationNoUserGesture) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // 1) A pending browser initiated navigation (omnibox, ...) starts.
  TestNavigationManager navigation(shell()->web_contents(), url_2);
  shell()->LoadURL(url_2);
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // 2) history.back() is sent but is not user initiated.
  EXPECT_TRUE(
      ExecuteScriptWithoutUserGesture(shell()->web_contents(),
                                      "history.pushState({}, null, '/');"
                                      "history.back();"));

  // 3) The first pending navigation is not canceled and can continue.
  navigation.WaitForNavigationFinished();  // Resume navigation.
  EXPECT_TRUE(navigation.was_successful());
}

// Renderer initiated back/forward navigation can cancel an ongoing browser
// initiated navigation if it is user initiated.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       HistoryBackCancelPendingNavigationUserGesture) {
  GURL url_1(embedded_test_server()->GetURL("/title1.html"));
  GURL url_2(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  // 1) A pending browser initiated navigation (omnibox, ...) starts.
  TestNavigationManager navigation(shell()->web_contents(), url_2);
  shell()->LoadURL(url_2);
  EXPECT_TRUE(navigation.WaitForRequestStart());

  // 2) history.back() is sent and is user initiated.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.pushState({}, null, '/');"
                            "history.back();"));

  // 3) Check the first pending navigation has been canceled.
  navigation.WaitForNavigationFinished();  // Resume navigation.
  EXPECT_FALSE(navigation.was_successful());
}

namespace {

// Checks whether the given urls are requested, and that GetPreviewsState()
// returns the appropriate value when the Previews are set.
class PreviewsStateContentBrowserClient : public ContentBrowserClient {
 public:
  PreviewsStateContentBrowserClient(const GURL& main_frame_url)
      : main_frame_url_(main_frame_url),
        main_frame_url_seen_(false),
        previews_state_(PREVIEWS_OFF),
        determine_allowed_previews_called_(false),
        determine_committed_previews_called_(false) {}

  ~PreviewsStateContentBrowserClient() override {}

  content::PreviewsState DetermineAllowedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    EXPECT_FALSE(determine_allowed_previews_called_);
    determine_allowed_previews_called_ = true;
    main_frame_url_seen_ = true;
    EXPECT_EQ(main_frame_url_, navigation_handle->GetURL());
    return previews_state_;
  }

  content::PreviewsState DetermineCommittedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const net::HttpResponseHeaders* response_headers) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_EQ(previews_state_, initial_state);
    determine_committed_previews_called_ = true;
    return initial_state;
  }

  void SetClient() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    content::SetBrowserClientForTesting(this);
  }

  void Reset(PreviewsState previews_state) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    main_frame_url_seen_ = false;
    previews_state_ = previews_state;
    determine_allowed_previews_called_ = false;
  }

  void CheckResourcesRequested() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    EXPECT_TRUE(determine_allowed_previews_called_);
    EXPECT_TRUE(determine_committed_previews_called_);
    EXPECT_TRUE(main_frame_url_seen_);
  }

 private:
  const GURL main_frame_url_;

  bool main_frame_url_seen_;
  PreviewsState previews_state_;
  bool determine_allowed_previews_called_;
  bool determine_committed_previews_called_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsStateContentBrowserClient);
};

}  // namespace

class PreviewsStateBrowserTest : public ContentBrowserTest {
 public:
  ~PreviewsStateBrowserTest() override {}

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    client_.reset(new PreviewsStateContentBrowserClient(
        embedded_test_server()->GetURL("/title1.html")));

    client_->SetClient();
  }

  void Reset(PreviewsState previews_state) { client_->Reset(previews_state); }

  void CheckResourcesRequested() { client_->CheckResourcesRequested(); }

 private:
  std::unique_ptr<PreviewsStateContentBrowserClient> client_;
};

// Test that navigating calls GetPreviewsState with SERVER_LOFI_ON.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest, ShouldEnableLoFiModeOn) {
  // Navigate with LoFi on.
  Reset(SERVER_LOFI_ON);
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/title1.html"), 1);
  CheckResourcesRequested();
}

// Test that navigating calls GetPreviewsState returning PREVIEWS_OFF.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest, ShouldEnableLoFiModeOff) {
  // Navigate with No Previews.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/title1.html"), 1);
  CheckResourcesRequested();
}

// Test that reloading calls GetPreviewsState again and changes the Previews
// state.
IN_PROC_BROWSER_TEST_F(PreviewsStateBrowserTest, ShouldEnableLoFiModeReload) {
  // Navigate with GetPreviewsState returning PREVIEWS_OFF.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/title1.html"), 1);
  CheckResourcesRequested();

  // Reload. GetPreviewsState should be called.
  Reset(SERVER_LOFI_ON);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  CheckResourcesRequested();
}

}  // namespace content
