// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/bad_message.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_message_filter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/render_frame_message_filter.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ipc/ipc_security_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

std::string GetCookieFromJS(RenderFrameHost* frame) {
  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame, "window.domAutomationController.send(document.cookie);", &cookie));
  return cookie;
}

mojom::RenderFrameMessageFilter* GetFilterForProcess(
    RenderProcessHost* process) {
  return static_cast<RenderProcessHostImpl*>(process)
      ->render_frame_message_filter_for_testing();
}

}  // namespace

class RenderFrameMessageFilterBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Exercises basic cookie operations via javascript, including an http page
// interacting with secure cookies.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest, Cookies) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // The server sends a HttpOnly cookie. The RenderFrameMessageFilter should
  // never allow this to be sent to any renderer process.
  GURL https_url = https_server.GetURL("/set-cookie?notforjs=1;HttpOnly");
  GURL http_url = embedded_test_server()->GetURL("/frame_with_load_event.html");

  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell(), http_url);
  NavigateToURL(shell2, https_url);

  WebContentsImpl* web_contents_https =
      static_cast<WebContentsImpl*>(shell2->web_contents());
  WebContentsImpl* web_contents_http =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  EXPECT_EQ("http://127.0.0.1/",
            web_contents_http->GetSiteInstance()->GetSiteURL().spec());
  EXPECT_EQ("https://127.0.0.1/",
            web_contents_https->GetSiteInstance()->GetSiteURL().spec());

  EXPECT_NE(web_contents_http->GetSiteInstance()->GetProcess(),
            web_contents_https->GetSiteInstance()->GetProcess());

  EXPECT_EQ("", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // Non-TLS page writes secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_http->GetMainFrame(),
                            "document.cookie = 'A=1; secure;';"));
  EXPECT_EQ("", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_http->GetMainFrame(),
                            "document.cookie = 'B=2';"));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_https->GetMainFrame(),
                            "document.cookie = 'C=3;secure;';"));
  EXPECT_EQ("B=2; C=3",
            GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2", GetCookieFromJS(web_contents_http->GetMainFrame()));

  // TLS page writes not-secure cookie.
  EXPECT_TRUE(ExecuteScript(web_contents_https->GetMainFrame(),
                            "document.cookie = 'D=4';"));
  EXPECT_EQ("B=2; C=3; D=4",
            GetCookieFromJS(web_contents_https->GetMainFrame()));
  EXPECT_EQ("B=2; D=4", GetCookieFromJS(web_contents_http->GetMainFrame()));
}

// Ensure "priority" cookie option is settable via document.cookie.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest, CookiePriority) {
  ASSERT_TRUE(embedded_test_server()->Start());

  struct {
    std::string param;
    net::CookiePriority priority;
  } cases[] = {{"name=value", net::COOKIE_PRIORITY_DEFAULT},
               {"name=value;priority=Low", net::COOKIE_PRIORITY_LOW},
               {"name=value;priority=Medium", net::COOKIE_PRIORITY_MEDIUM},
               {"name=value;priority=High", net::COOKIE_PRIORITY_HIGH}};

  for (auto test_case : cases) {
    GURL url = embedded_test_server()->GetURL("/set_document_cookie.html?" +
                                              test_case.param);
    NavigateToURL(shell(), url);
    std::vector<net::CanonicalCookie> cookies =
        GetCanonicalCookies(shell()->web_contents()->GetBrowserContext(), url);

    EXPECT_EQ(1u, cookies.size());
    EXPECT_EQ("name", cookies[0].Name());
    EXPECT_EQ("value", cookies[0].Value());
    EXPECT_EQ(test_case.priority, cookies[0].Priority());
  }
}

// SameSite cookies (that aren't marked as http-only) should be available to
// JavaScript.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest, SameSiteCookies) {
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // The server sets five cookies on 'a.com' and on 'b.com', then loads a
  // page that frames both 'a.com' and 'b.com' under 'a.com'.
  std::string cookies_to_set =
      "/set-cookie?normal=1"
      "&strict=1;SameSite=Strict"
      "&lax=1;SameSite=Lax"
      "&strict-http=1;SameSite=Strict;httponly"
      "&lax-http=1;SameSite=Lax;httponly";

  GURL url = embedded_test_server()->GetURL("a.com", cookies_to_set);
  NavigateToURL(shell(), url);
  url = embedded_test_server()->GetURL("b.com", cookies_to_set);
  NavigateToURL(shell(), url);
  url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(),b())");
  NavigateToURL(shell(), url);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  RenderFrameHost* main_frame = web_contents->GetMainFrame();
  RenderFrameHost* a_iframe =
      web_contents->GetFrameTree()->root()->child_at(0)->current_frame_host();
  RenderFrameHost* b_iframe =
      web_contents->GetFrameTree()->root()->child_at(1)->current_frame_host();

  // The top-level frame should get both kinds of same-site cookies.
  EXPECT_EQ("normal=1; strict=1; lax=1", GetCookieFromJS(main_frame));

  // Same-site cookies will be delievered to the 'a.com' frame, as it is same-
  // site with its ancestors.
  EXPECT_EQ("normal=1; strict=1; lax=1", GetCookieFromJS(a_iframe));

  // Same-site cookies should not be delievered to the 'b.com' frame, as it
  // isn't same-site with its ancestors.
  EXPECT_EQ("normal=1", GetCookieFromJS(b_iframe));
}

// The RenderFrameMessageFilter will kill processes when they access the cookies
// of sites other than the site the process is dedicated to, under site
// isolation.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest,
                       CrossSiteCookieSecurityEnforcement) {
  // The code under test is only active under site isolation.
  if (!AreAllSitesIsolatedForTesting()) {
    return;
  }

  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_with_load_event.html"));

  WebContentsImpl* tab = static_cast<WebContentsImpl*>(shell()->web_contents());

  // The iframe on the http page should get its own process.
  FrameTreeVisualizer v;
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      v.DepictFrameTree(tab->GetFrameTree()->root()));

  RenderFrameHost* main_frame = tab->GetMainFrame();
  RenderFrameHost* iframe =
      tab->GetFrameTree()->root()->child_at(0)->current_frame_host();

  EXPECT_NE(iframe->GetProcess(), main_frame->GetProcess());

  // Try to get cross-site cookies from the subframe's process and wait for it
  // to be killed.
  {
    RenderProcessHostKillWaiter iframe_kill_waiter(iframe->GetProcess());

    base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
        ->PostTask(FROM_HERE,
                   base::BindOnce(
                       [](RenderFrameHost* frame) {
                         GetFilterForProcess(frame->GetProcess())
                             ->GetCookies(
                                 frame->GetRoutingID(),
                                 GURL("http://127.0.0.1/"),
                                 GURL("http://127.0.0.1/"),
                                 base::BindOnce([](const std::string&) {}));
                       },
                       iframe));

    EXPECT_EQ(bad_message::RFMF_GET_COOKIES_BAD_ORIGIN,
              iframe_kill_waiter.Wait());
  }

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/ (no process)",
      v.DepictFrameTree(tab->GetFrameTree()->root()));

  // Now set a cross-site cookie from the main frame's process and wait for it
  // to be killed.
  {
    RenderProcessHostKillWaiter main_frame_kill_waiter(
        tab->GetMainFrame()->GetProcess());

    base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
        ->PostTask(FROM_HERE, base::BindOnce(
                                  [](RenderFrameHost* frame) {
                                    GetFilterForProcess(frame->GetProcess())
                                        ->SetCookie(frame->GetRoutingID(),
                                                    GURL("https://baz.com/"),
                                                    GURL("https://baz.com/"),
                                                    "pwn=ed",
                                                    base::DoNothing());
                                  },
                                  main_frame));

    EXPECT_EQ(bad_message::RFMF_SET_COOKIE_BAD_ORIGIN,
              main_frame_kill_waiter.Wait());
  }

  EXPECT_EQ(
      " Site A\n"
      "Where A = http://127.0.0.1/ (no process)",
      v.DepictFrameTree(tab->GetFrameTree()->root()));
}

// FrameHostMsg_RenderProcessGone is a synthetic message that's really an
// implementation detail of RenderProcessHostImpl's crash recovery. It should be
// ignored if it arrives over the IPC channel.
IN_PROC_BROWSER_TEST_F(RenderFrameMessageFilterBrowserTest, RenderProcessGone) {
  GURL web_url("http://foo.com/simple_page.html");
  NavigateToURL(shell(), web_url);
  RenderFrameHost* web_rfh = shell()->web_contents()->GetMainFrame();

  ASSERT_TRUE(web_rfh->IsRenderFrameLive());
  RenderProcessHostKillWaiter kill_waiter(web_rfh->GetProcess());
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      web_rfh->GetProcess()->GetChannel(),
      FrameHostMsg_RenderProcessGone(
          web_rfh->GetRoutingID(), base::TERMINATION_STATUS_NORMAL_TERMINATION,
          0));

  // If the message had gone through, we'd have marked the RFH as dead but
  // left the RPH and its connection alive, and the Wait below would hang.
  EXPECT_EQ(bad_message::RFMF_RENDERER_FAKED_ITS_OWN_DEATH, kill_waiter.Wait());

  ASSERT_FALSE(web_rfh->GetProcess()->IsInitializedAndNotDead());
  ASSERT_FALSE(web_rfh->IsRenderFrameLive());
}

}  // namespace content
