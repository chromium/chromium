// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/mock_web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/ui_base_switches.h"

using testing::_;
using testing::NotNull;

namespace content {

class WebContentsObserverBrowserTest : public ContentBrowserTest {
 public:
  WebContentsObserverBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Some platforms are flaky due to relatively slow loading interacting
  // with deferred commits.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* top_frame_host() {
    return static_cast<RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
  }

  base::test::ScopedFeatureList feature_list_;
};

namespace {

class ServiceWorkerAccessObserver : public WebContentsObserver {
 public:
  explicit ServiceWorkerAccessObserver(WebContentsImpl* web_contents)
      : WebContentsObserver(web_contents) {}

  MOCK_METHOD3(OnServiceWorkerAccessed,
               void(NavigationHandle*, const GURL&, AllowServiceWorkerResult));
  MOCK_METHOD3(OnServiceWorkerAccessed,
               void(RenderFrameHost*, const GURL&, AllowServiceWorkerResult));
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       OnServiceWorkerAccessed) {
  GURL service_worker_scope =
      embedded_test_server()->GetURL("/service_worker/");
  {
    // 1) Navigate to a page and register a ServiceWorker. Expect a notification
    // to be called when the service worker is accessed from a frame.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(
        observer,
        OnServiceWorkerAccessed(
            testing::Matcher<RenderFrameHost*>(NotNull()), service_worker_scope,
            AllowServiceWorkerResult::FromPolicy(false, false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(), embedded_test_server()->GetURL(
                            "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(top_frame_host(),
                     "register('fetch_event.js', '/service_worker/');"));
    run_loop.Run();
  }

  {
    // 2) Navigate to a page in scope of the previously registered ServiceWorker
    // and expect to get a notification about ServiceWorker being accessed for
    // a navigation.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(observer,
                OnServiceWorkerAccessed(
                    testing::Matcher<NavigationHandle*>(NotNull()),
                    service_worker_scope,
                    AllowServiceWorkerResult::FromPolicy(false, false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/service_worker/empty.html")));
    run_loop.Run();
  }
}

namespace {

class ServiceWorkerAccessContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  ServiceWorkerAccessContentBrowserClient() = default;

  void SetJavascriptAllowed(bool allowed) { javascript_allowed_ = allowed; }

  void SetCookiesAllowed(bool allowed) { cookies_allowed_ = allowed; }

  AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      BrowserContext* context) override {
    return AllowServiceWorkerResult::FromPolicy(!javascript_allowed_,
                                                !cookies_allowed_);
  }

 private:
  bool cookies_allowed_ = true;
  bool javascript_allowed_ = true;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       OnServiceWorkerAccessed_ContentClientBlocked) {
  GURL service_worker_scope =
      embedded_test_server()->GetURL("/service_worker/");
  {
    // 1) Navigate to a page and register a ServiceWorker. Expect a notification
    // to be called when the service worker is accessed from a frame.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(
        observer,
        OnServiceWorkerAccessed(
            testing::Matcher<RenderFrameHost*>(NotNull()), service_worker_scope,
            AllowServiceWorkerResult::FromPolicy(false, false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(), embedded_test_server()->GetURL(
                            "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE",
              EvalJs(top_frame_host(),
                     "register('fetch_event.js', '/service_worker/');"));
    run_loop.Run();
  }

  // 2) Set content client and disallow javascript.
  ServiceWorkerAccessContentBrowserClient content_browser_client;
  content_browser_client.SetJavascriptAllowed(false);

  {
    // 2) Navigate to a page in scope of the previously registered ServiceWorker
    // and expect to get a notification about ServiceWorker being accessed for
    // a navigation. Javascript should be blocked according to the policy.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnServiceWorkerAccessed(
                              testing::Matcher<NavigationHandle*>(NotNull()),
                              service_worker_scope,
                              AllowServiceWorkerResult::FromPolicy(
                                  /* javascript_blocked_by_policy=*/true,
                                  /* cookies_blocked_by_policy=*/false)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/service_worker/empty.html")));
    run_loop.Run();
  }

  content_browser_client.SetJavascriptAllowed(true);
  content_browser_client.SetCookiesAllowed(false);

  {
    // 3) Navigate to a page in scope of the previously registered ServiceWorker
    // and expect to get a notification about ServiceWorker being accessed for
    // a navigation. Cookies should be blocked according to the policy.
    ServiceWorkerAccessObserver observer(web_contents());
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnServiceWorkerAccessed(
                              testing::Matcher<NavigationHandle*>(NotNull()),
                              service_worker_scope,
                              AllowServiceWorkerResult::FromPolicy(
                                  /* javascript_blocked_by_policy=*/false,
                                  /* cookies_blocked_by_policy=*/true)))
        .WillOnce([&]() { run_loop.Quit(); });
    EXPECT_TRUE(NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/service_worker/empty.html")));
    run_loop.Run();
  }
}

namespace {

enum class ContextType {
  kNavigation,
  kFrame,
};

class CookieTracker : public WebContentsObserver {
 public:
  explicit CookieTracker(WebContentsImpl* web_contents)
      : WebContentsObserver(web_contents) {}

  struct CookieAccessDescription {
    CookieAccessDetails::Type type;

    ContextType context_type;
    GlobalRoutingID frame_id;
    int64_t navigation_id = -1;

    GURL url;
    GURL first_party_url;
    std::string cookie_name;
    std::string cookie_value;

    friend std::ostream& operator<<(std::ostream& o,
                                    const CookieAccessDescription& d) {
      o << (d.type == CookieAccessDetails::Type::kRead ? "read" : "change");
      o << " url=" << d.url;
      o << " first_party_url=" << d.first_party_url;
      o << " name=" << d.cookie_name;
      o << " value=" << d.cookie_value;
      switch (d.context_type) {
        case ContextType::kNavigation:
          o << " context=navigation(";
          o << "id=" << d.navigation_id;
          o << ")";
          break;
        case ContextType::kFrame:
          o << " context=frame(";
          o << "process_id=" << d.frame_id.child_id;
          o << " frame_id=" << d.frame_id.route_id;
          o << ")";
          break;
      }
      return o;
    }

   private:
    auto comparison_key() const {
      return std::tie(type, url, first_party_url, cookie_name, cookie_value,
                      frame_id, navigation_id);
    }

   public:
    bool operator==(const CookieAccessDescription& other) const {
      return comparison_key() == other.comparison_key();
    }
  };

  void OnCookiesAccessed(NavigationHandle* navigation,
                         const CookieAccessDetails& details) override {
    for (const auto& cookie : details.cookie_list) {
      cookie_accesses_.push_back({details.type,
                                  ContextType::kNavigation,
                                  {},
                                  navigation->GetNavigationId(),
                                  details.url,
                                  details.first_party_url,
                                  cookie.Name(),
                                  cookie.Value()});
    }

    QuitIfReady();
  }

  void OnCookiesAccessed(RenderFrameHost* rfh,
                         const CookieAccessDetails& details) override {
    for (const auto& cookie : details.cookie_list) {
      cookie_accesses_.push_back(
          {details.type,
           ContextType::kFrame,
           {rfh->GetProcess()->GetID(), rfh->GetRoutingID()},
           -1,
           details.url,
           details.first_party_url,
           cookie.Name(),
           cookie.Value()});
    }

    QuitIfReady();
  }

  void WaitForCookies(size_t count) {
    waiting_for_cookies_count_ = count;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    QuitIfReady();
    run_loop.Run();
  }

  std::vector<CookieAccessDescription>& cookie_accesses() {
    return cookie_accesses_;
  }

  GlobalRoutingID frame_id(size_t index) {
    if (index < frame_ids_.size())
      return frame_ids_[index];
    // Return bogus values which will never be returned by the code we are
    // testing. This ensures that if we return this value, the subsequent
    // comparison will fail.
    return {-42, -42};
  }

  int64_t navigation_id(size_t index) {
    if (index < navigation_ids_.size())
      return navigation_ids_[index];
    // Return bogus values which will never be returned by the code we are
    // testing. This ensures that if we return this value, the subsequent
    // comparison will fail.
    return -42;
  }

  void DidFinishNavigation(NavigationHandle* navigation) override {
    navigation_ids_.push_back(navigation->GetNavigationId());
  }

  void RenderFrameCreated(RenderFrameHost* rfh) override {
    frame_ids_.emplace_back(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  }

 private:
  void QuitIfReady() {
    if (quit_closure_.is_null())
      return;
    if (cookie_accesses_.size() < waiting_for_cookies_count_)
      return;
    std::move(quit_closure_).Run();
  }

  std::vector<CookieAccessDescription> cookie_accesses_;

  // List of observed navigation and frame ids to be used in testing.
  std::vector<GlobalRoutingID> frame_ids_;
  std::vector<int64_t> navigation_ids_;

  size_t waiting_for_cookies_count_ = 0;
  base::OnceClosure quit_closure_;
};

using CookieAccess = CookieTracker::CookieAccessDescription;

}  // namespace

// TODO(https://crbug.com/1288573): Flaky on Mac and Android.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_CookieCallbacks_MainFrame DISABLED_CookieCallbacks_MainFrame
#else
#define MAYBE_CookieCallbacks_MainFrame CookieCallbacks_MainFrame
#endif
IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       MAYBE_CookieCallbacks_MainFrame) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(
      embedded_test_server()->GetURL("a.com", "/cookies/set_cookie.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to |url1|. This navigation should set a cookie, which we should
  // be notified about.
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));
  cookie_tracker.WaitForCookies(1);

  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kChange,
                                        ContextType::kNavigation,
                                        {},
                                        cookie_tracker.navigation_id(0),
                                        url1,
                                        first_party_url,
                                        "foo",
                                        "bar"}));
  cookie_tracker.cookie_accesses().clear();

  // 2) Navigate to |url2| on the same site. Given that we have set a cookie
  // before, this should sent a previously set cookie with the request and we
  // should be notified about this.
  EXPECT_TRUE(NavigateToURL(web_contents(), url2));
  cookie_tracker.WaitForCookies(1);

  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kRead,
                                        ContextType::kNavigation,
                                        {},
                                        cookie_tracker.navigation_id(1),
                                        url2,
                                        first_party_url,
                                        "foo",
                                        "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

// TODO(https://crbug.com/1288573): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CookieCallbacks_MainFrameRedirect \
  DISABLED_CookieCallbacks_MainFrameRedirect
#else
#define MAYBE_CookieCallbacks_MainFrameRedirect \
  CookieCallbacks_MainFrameRedirect
#endif
IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       MAYBE_CookieCallbacks_MainFrameRedirect) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cookies/redirect_and_set_cookie.html"));
  GURL url1_after_redirect(
      embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url2(embedded_test_server()->GetURL("a.com", "/title2.html"));

  // 1) Navigate to |url1|. The initial URL redirects and sets a cookie (we
  // should be notified about this) and as the redirect points to the same site,
  // cookie should be sent for the second request as well (we should be notified
  // about this as well).
  EXPECT_TRUE(NavigateToURL(web_contents(), url1, url1_after_redirect));

  cookie_tracker.WaitForCookies(2);
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::UnorderedElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kChange,
                               ContextType::kNavigation,
                               {},
                               cookie_tracker.navigation_id(0),
                               url1,
                               first_party_url,
                               "foo",
                               "bar"},
                  CookieAccess{CookieAccessDetails::Type::kRead,
                               ContextType::kNavigation,
                               {},
                               cookie_tracker.navigation_id(0),
                               url1_after_redirect,
                               first_party_url,
                               "foo",
                               "bar"}));
  cookie_tracker.cookie_accesses().clear();

  // 2) Navigate to another url on the same site and expect a notification about
  // a read cookie.
  EXPECT_TRUE(NavigateToURL(web_contents(), url2));

  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kRead,
                                        ContextType::kNavigation,
                                        {},
                                        cookie_tracker.navigation_id(1),
                                        url2,
                                        first_party_url,
                                        "foo",
                                        "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

// TODO(https://crbug.com/1288573): Flaky on Mac, Android and Windows.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_CookieCallbacks_Subframe DISABLED_CookieCallbacks_Subframe
#else
#define MAYBE_CookieCallbacks_Subframe CookieCallbacks_Subframe
#endif
IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       MAYBE_CookieCallbacks_Subframe) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cookies/set_cookie_from_subframe.html"));
  GURL url1_subframe(
      embedded_test_server()->GetURL("a.com", "/cookies/set_cookie.html"));
  GURL url2(embedded_test_server()->GetURL("a.com",
                                           "/cookies/page_with_subframe.html"));
  GURL url2_subframe(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Load a page with a subframe. The main resource of the the subframe
  // triggers setting a cookie. We should get a cookie change for the
  // subresource and no cookie read for the main resource.
  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  cookie_tracker.WaitForCookies(1);
  // Navigations are: main frame (0), subframe (1).
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kChange,
                                        ContextType::kNavigation,
                                        {},
                                        cookie_tracker.navigation_id(1),
                                        url1_subframe,
                                        first_party_url,
                                        "foo",
                                        "bar"}));
  cookie_tracker.cookie_accesses().clear();

  EXPECT_TRUE(NavigateToURL(web_contents(), url2));

  // 2) Load a page with a subframe. Both main frame and subframe should get a
  // cookie read.
  cookie_tracker.WaitForCookies(2);
  // Navigations are: main frame (2), subframe (3).
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(CookieAccess{CookieAccessDetails::Type::kRead,
                                        ContextType::kNavigation,
                                        {},
                                        cookie_tracker.navigation_id(2),
                                        url2,
                                        first_party_url,
                                        "foo",
                                        "bar"},
                           CookieAccess{CookieAccessDetails::Type::kRead,
                                        ContextType::kNavigation,
                                        {},
                                        cookie_tracker.navigation_id(3),
                                        url2_subframe,
                                        first_party_url,
                                        "foo",
                                        "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

// TODO(https://crbug.com/1288573): Flaky on Mac.
// TODO(https://crbug.com/1426973): Fix on android and enable it.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_CookieCallbacks_Subresource DISABLED_CookieCallbacks_Subresource
#else
#define MAYBE_CookieCallbacks_Subresource CookieCallbacks_Subresource
#endif
IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       MAYBE_CookieCallbacks_Subresource) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL(
      "a.com", "/cookies/set_cookie_from_subresource.html"));
  GURL url1_image(embedded_test_server()->GetURL(
      "a.com", "/cookies/image_with_set_cookie.jpg"));
  GURL url2(embedded_test_server()->GetURL(
      "a.com", "/cookies/page_with_subresource.html"));
  GURL url2_image(embedded_test_server()->GetURL(
      "a.com", "/cookies/image_without_set_cookie.jpg"));

  EXPECT_TRUE(NavigateToURL(web_contents(), url1));

  // 1) Load a page with a subresource (image), which sets a cookie when
  // fetched.
  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kChange,
                               ContextType::kFrame, cookie_tracker.frame_id(0),
                               -1, url1_image, first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();

  // 2) Load a page with subresource. Both the page and the resource should get
  // a cookie.
  EXPECT_TRUE(NavigateToURL(web_contents(), url2));
  // If the RFH changes after navigation, the cookie will be attributed to a
  // different frame.
  int frame_id_index =
      CanSameSiteMainFrameNavigationsChangeRenderFrameHosts() ? 1 : 0;
  cookie_tracker.WaitForCookies(2);
  EXPECT_THAT(
      cookie_tracker.cookie_accesses(),
      testing::ElementsAre(
          CookieAccess{CookieAccessDetails::Type::kRead,
                       ContextType::kNavigation,
                       {},
                       cookie_tracker.navigation_id(1),
                       url2,
                       first_party_url,
                       "foo",
                       "bar"},
          CookieAccess{CookieAccessDetails::Type::kRead, ContextType::kFrame,
                       cookie_tracker.frame_id(frame_id_index), -1, url2_image,
                       first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       CookieCallbacks_DocumentCookie) {
  CookieTracker cookie_tracker(web_contents());

  GURL first_party_url("http://a.com/");
  GURL url1(embedded_test_server()->GetURL("a.com", "/title1.html"));

  EXPECT_TRUE(NavigateToURL(web_contents(), url1));
  EXPECT_TRUE(ExecJs(web_contents(), "document.cookie='foo=bar'"));

  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kChange,
                               ContextType::kFrame, cookie_tracker.frame_id(0),
                               -1, url1, first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();

  EXPECT_EQ("foo=bar", EvalJs(web_contents(), "document.cookie"));

  cookie_tracker.WaitForCookies(1);
  EXPECT_THAT(cookie_tracker.cookie_accesses(),
              testing::ElementsAre(
                  CookieAccess{CookieAccessDetails::Type::kRead,
                               ContextType::kFrame, cookie_tracker.frame_id(0),
                               -1, url1, first_party_url, "foo", "bar"}));
  cookie_tracker.cookie_accesses().clear();
}

namespace {

class FocusedNodeObserver : public WebContentsObserver {
 public:
  explicit FocusedNodeObserver(WebContentsImpl* web_contents)
      : WebContentsObserver(web_contents) {}

  blink::mojom::FocusType last_focus_type() const { return last_focus_type_; }

  void WaitForFocusChangedInPage() { run_loop_.Run(); }

  // WebContentsObserver:
  void OnFocusChangedInPage(FocusedNodeDetails* details) override {
    last_focus_type_ = details->focus_type;
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  blink::mojom::FocusType last_focus_type_;
};

// Tests that the focus type is reported correctly in FocusedNodeDetails when
// WebContentsObserver::OnFocusChangedInPage() is called.
IN_PROC_BROWSER_TEST_F(WebContentsObserverBrowserTest,
                       OnFocusChangedInPageFocusType) {
  FocusedNodeObserver observer(web_contents());
  GURL url(embedded_test_server()->GetURL("/form_that_posts_cross_site.html"));

  EXPECT_TRUE(NavigateToURL(web_contents(), url));
  SimulateMouseClickOrTapElementWithId(web_contents(), "text");
  observer.WaitForFocusChangedInPage();
  EXPECT_EQ(blink::mojom::FocusType::kMouse, observer.last_focus_type());
}

}  // namespace

namespace {

class ColorSchemeObserver : public WebContentsObserver {
 public:
  explicit ColorSchemeObserver(WebContentsImpl* web_contents)
      : WebContentsObserver(web_contents) {}

  MOCK_METHOD1(InferredColorSchemeUpdated,
               void(absl::optional<blink::mojom::PreferredColorScheme>));
};

class WebContentsObserverColorSchemeBrowserTest
    : public WebContentsObserverBrowserTest,
      public testing::WithParamInterface<blink::mojom::PreferredColorScheme> {
 public:
  WebContentsObserverColorSchemeBrowserTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebContentsObserverBrowserTest::SetUpCommandLine(command_line);
    // ShellContentBrowserClient::OverrideWebkitPrefs() overrides the
    // prefers-color-scheme according to switches::kForceDarkMode command line.
    if (GetParam() == blink::mojom::PreferredColorScheme::kDark)
      command_line->AppendSwitch(switches::kForceDarkMode);
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    WebContentsObserverColorSchemeBrowserTest,
    ::testing::Values(blink::mojom::PreferredColorScheme::kDark,
                      blink::mojom::PreferredColorScheme::kLight));

IN_PROC_BROWSER_TEST_P(WebContentsObserverColorSchemeBrowserTest,
                       ColorSchemeInferred) {
  ColorSchemeObserver observer(web_contents());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer,
                InferredColorSchemeUpdated(
                    absl::optional<blink::mojom::PreferredColorScheme>()));
    EXPECT_CALL(
        observer,
        InferredColorSchemeUpdated(
            absl::optional<blink::mojom::PreferredColorScheme>(GetParam())))
        .WillOnce([&]() { run_loop.Quit(); });
    GURL url(embedded_test_server()->GetURL("/color-scheme.html"));
    EXPECT_TRUE(NavigateToURL(web_contents(), url));
    run_loop.Run();
  }

  // Navigate to another URL to verify the callback invoked for each navigation
  // even if the color scheme isn't changed.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer,
                InferredColorSchemeUpdated(
                    absl::optional<blink::mojom::PreferredColorScheme>()));
    EXPECT_CALL(
        observer,
        InferredColorSchemeUpdated(
            absl::optional<blink::mojom::PreferredColorScheme>(GetParam())))
        .WillOnce([&]() { run_loop.Quit(); });
    GURL url2(embedded_test_server()->GetURL("/color-scheme-2.html"));
    EXPECT_TRUE(NavigateToURL(web_contents(), url2));
    run_loop.Run();
  }
}

}  // namespace

}  // namespace content
