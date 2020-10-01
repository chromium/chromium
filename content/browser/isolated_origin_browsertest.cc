// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom-test-utils.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/dom_storage.mojom-test-utils.h"
#include "url/gurl.h"

namespace content {

using IsolatedOriginSource = ChildProcessSecurityPolicy::IsolatedOriginSource;

// This is a base class for all tests in this class.  It does not isolate any
// origins and only provides common helper functions to the other test classes.
class IsolatedOriginTestBase : public ContentBrowserTest {
 public:
  IsolatedOriginTestBase() {}
  ~IsolatedOriginTestBase() override {}

  // Check if |origin| is an isolated origin.  This helper is used in tests
  // that care only about globally applicable isolated origins (not restricted
  // to a particular BrowsingInstance or profile).
  bool IsIsolatedOrigin(const url::Origin& origin) {
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    IsolationContext isolation_context(
        shell()->web_contents()->GetBrowserContext());
    return policy->IsIsolatedOrigin(isolation_context, origin,
                                    false /* origin_requests_isolation */);
  }

  bool IsIsolatedOrigin(const GURL& url) {
    return IsIsolatedOrigin(url::Origin::Create(url));
  }

  ProcessLock ProcessLockFromUrl(const std::string& url) {
    return ProcessLock(
        SiteInfo(GURL(url), GURL(url), false /* is_origin_keyed */,
                 false /* is_coop_coep_cross_origin_isolated */,
                 base::nullopt /* coop_coep_cross_origin_isolated_origin */));
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  // Helper function that computes an appropriate process lock that corresponds
  // to |url|'s origin (without converting to sites, handling effective URLs,
  // etc). This must be equivalent to what
  // SiteInstanceImpl::DetermineProcessLockURL() would return
  // for strict origin isolation.
  // Note: do not use this for opt-in origin isolation, as it won't set
  // is_origin_keyed to true.
  ProcessLock GetStrictProcessLock(const GURL& url) {
    GURL origin_url = url::Origin::Create(url).GetURL();
    return ProcessLock(
        SiteInfo(origin_url, origin_url, false /* is_origin_keyed */,
                 false /* is_coop_coep_cross_origin_isolated */,
                 base::nullopt /* coop_coep_cross_origin_isolated_origin */));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginTestBase);
};

class IsolatedOriginTest : public IsolatedOriginTestBase {
 public:
  IsolatedOriginTest() {}
  ~IsolatedOriginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::string origin_list =
        embedded_test_server()->GetURL("isolated.foo.com", "/").spec() + "," +
        embedded_test_server()->GetURL("isolated.bar.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void InjectAndClickLinkTo(GURL url) {
    EXPECT_TRUE(ExecuteScript(web_contents(),
                              "var link = document.createElement('a');"
                              "link.href = '" +
                                  url.spec() +
                                  "';"
                                  "document.body.appendChild(link);"
                                  "link.click();"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginTest);
};

// Base class used for server-based origin isolation opt-in tests to handle the
// server responses and other common infrastructure.
class OriginIsolationOptInServerTest : public IsolatedOriginTestBase {
 public:
  OriginIsolationOptInServerTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~OriginIsolationOptInServerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTestBase::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    IsolateAllSitesForTesting(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    feature_list_.InitAndEnableFeature(feature_switch());

    // Start the HTTPS server here so derived tests can use it if they override
    // SetUpCommandLine().
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&OriginIsolationOptInServerTest::HandleResponse,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpOnMainThread() override {
    IsolatedOriginTestBase::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    IsolatedOriginTestBase::TearDownOnMainThread();
  }

  // Need an https server because
  // OriginPolicyThrottle::ShouldRequestOriginPolicy() will return false
  // otherwise.
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  bool ShouldOriginGetOptInIsolation(const url::Origin& origin) {
    auto* site_instance = static_cast<SiteInstanceImpl*>(
        shell()->web_contents()->GetMainFrame()->GetSiteInstance());

    return ChildProcessSecurityPolicyImpl::GetInstance()
        ->ShouldOriginGetOptInIsolation(site_instance->GetIsolationContext(),
                                        origin,
                                        false /* origin_requests_isolation */);
  }

 protected:
  virtual const base::Feature& feature_switch() = 0;
  virtual std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) = 0;

 private:
  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OriginIsolationOptInServerTest);
};

// Tests of opt-in origin isolation which use origin policy as the opt-in
// mechanism. Most tests for the overall feature are in this class, but see also
// OriginIsolationOptInHeaderTest for tests that verify headers can be used as
// an opt-in mechanism as well.
class OriginIsolationOptInOriginPolicyTest
    : public OriginIsolationOptInServerTest {
 public:
  OriginIsolationOptInOriginPolicyTest() = default;

  void SetOriginPolicyManifest(const std::string& manifest) {
    origin_policy_manifest_ = manifest;
  }

 protected:
  const base::Feature& feature_switch() override {
    return features::kOriginPolicy;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) override {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    // Ensures requests to /isolate_origin request that the origin policy be
    // applied.
    if (request.relative_url == "/isolate_origin") {
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");
      response->AddCustomHeader("Origin-Policy", "allowed=(latest)");
      response->set_content("isolate me!");
      return std::move(response);
    }

    // Intercepts the request to get the origin policy, and injects the policy.
    // Note: this will only be activated for requests that load "isolate_origin"
    // above, since only it sets the Origin-Policy header.
    if (request.relative_url == "/.well-known/origin-policy") {
      response->set_code(net::HTTP_OK);
      response->set_content(origin_policy_manifest_);
      return std::move(response);
    }

    // If we return nullptr, then the server will go ahead and actually serve
    // the file.
    return nullptr;
  }

  std::string origin_policy_manifest_;

  DISALLOW_COPY_AND_ASSIGN(OriginIsolationOptInOriginPolicyTest);
};

// Tests that verify headers can be used to opt-in to origin isolation.  See
// OriginIsolationOptInOriginPolicyTest for most tests of the feature.
class OriginIsolationOptInHeaderTest : public OriginIsolationOptInServerTest {
 public:
  OriginIsolationOptInHeaderTest() = default;

  void SetHeaderValue(const std::string& header_value) {
    header_ = header_value;
  }

 protected:
  const base::Feature& feature_switch() override {
    return features::kOriginIsolationHeader;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) override {
    if (request.relative_url == "/isolate_origin") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");

      if (header_) {
        response->AddCustomHeader("Origin-Isolation", *header_);
      }

      response->set_content("isolate me!");
      return std::move(response);
    }

    // If we return nullptr, then the server will go ahead and actually serve
    // the file.
    return nullptr;
  }

  base::Optional<std::string> header_;

  DISALLOW_COPY_AND_ASSIGN(OriginIsolationOptInHeaderTest);
};

// This class allows testing the interaction of OptIn isolation and command-line
// isolation for origins. Tests using this class will isolate foo.com and
// bar.com by default using command-line isolation, but any opt-in isolation
// will override this.
class OriginIsolationOptInOriginPolicyCommandLineTest
    : public OriginIsolationOptInOriginPolicyTest {
 public:
  OriginIsolationOptInOriginPolicyCommandLineTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OriginIsolationOptInOriginPolicyTest::SetUpCommandLine(command_line);
    // The base class should already have started the HTTPS server so we can use
    // it here to generate origins to specify on the command line.
    ASSERT_TRUE(https_server()->Started());

    std::string origin_list = https_server()->GetURL("foo.com", "/").spec() +
                              "," +
                              https_server()->GetURL("bar.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OriginIsolationOptInOriginPolicyCommandLineTest);
};

// This test verifies that opt-in isolation takes precedence over command-line
// isolation. It loads an opt-in isolated base origin (which would have
// otherwise been isolated via command-line isolation), and then loads a child
// frame sub-origin which should-not be isolated (but would have been if the
// base origin was command-line isolated).
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyCommandLineTest,
                       OptInOverridesCommandLine) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  // Start off with an isolated base-origin in an a(a) configuration, then
  // navigate the subframe to a sub-origin not requesting isolation.
  // Note: this works because we serve mock headers with the base origin's html
  // file, requesting loading the origin policy.
  GURL isolated_base_origin_url(https_server()->GetURL(
      "foo.com", "/isolated_base_origin_with_subframe.html"));
  GURL non_isolated_sub_origin(
      https_server()->GetURL("non_isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_base_origin_url));
  // The .html main frame has two iframes, this test only uses the first one.
  EXPECT_EQ(3u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  NavigateFrameToURL(child_frame_node, non_isolated_sub_origin);

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->ShouldOriginGetOptInIsolation(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      url::Origin::Create(isolated_base_origin_url),
      false /* origin_requests_isolation */));
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      url::Origin::Create(non_isolated_sub_origin),
      false /* origin_requests_isolation */));
  // Make sure the child (i.e. sub-origin) is not isolated.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(
      GURL("https://foo.com"),
      child_frame_node->current_frame_host()->GetSiteInstance()->GetSiteURL());
  // The following test passes because IsIsolatedOrigin doesn't distinguish
  // between command-line isolation and opt-in isolation.
  EXPECT_TRUE(policy->IsIsolatedOrigin(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      url::Origin::Create(non_isolated_sub_origin),
      false /* origin_requests_isolation */));

  // Make sure the opt-in isolated origin is origin-keyed, and the non-opt-in
  // origin is site-keyed.
  EXPECT_TRUE(root->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_origin_keyed());
  EXPECT_FALSE(child_frame_node->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_origin_keyed());

  // Make sure the master opt-in list has the base origin isolated and the sub
  // origin not isolated.
  EXPECT_TRUE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(isolated_base_origin_url)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(non_isolated_sub_origin)));
}

// This tests that origin policy opt-in causes the origin to end up in the
// isolated origins list.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest, Basic) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");

  GURL url(https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  url::Origin origin(url::Origin::Create(url));

  EXPECT_FALSE(ShouldOriginGetOptInIsolation(origin));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ShouldOriginGetOptInIsolation(origin));
}

// This tests that header-based opt-in causes the origin to end up in the
// isolated origins list.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest, Basic) {
  SetHeaderValue("?1");

  GURL url(https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  url::Origin origin(url::Origin::Create(url));

  EXPECT_FALSE(ShouldOriginGetOptInIsolation(origin));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ShouldOriginGetOptInIsolation(origin));
}

// Further tests deep-dive into various scenarios for the isolation opt-ins.
// They use the origin policy mechanism, under the assumption that it will be
// the same for the header mechanism since they both trigger the same behavior
// in ChildProcessSecurityPolicyImpl.

// In this test the sub-origin is isolated because the origin policy requests
// "isolation". It will have a different site instance than the main frame.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       SimpleSubOriginIsolationTest) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  // Start off with an a(a) page, then navigate the subframe to an isolated sub
  // origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  GURL origin_url = url::Origin::Create(isolated_suborigin_url).GetURL();
  auto expected_isolated_suborigin_lock = ProcessLock(
      SiteInfo(origin_url, origin_url, true /* is_origin_keyed */,
               false /* is_coop_coep_cross_origin_isolated */,
               base::nullopt /* coop_coep_cross_origin_isolated_origin */));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  NavigateFrameToURL(child_frame_node, isolated_suborigin_url);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child_frame_node->current_frame_host()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  GURL expected_isolated_sub_origin =
      url::Origin::Create(isolated_suborigin_url).GetURL();
  EXPECT_EQ(
      expected_isolated_sub_origin,
      child_frame_node->current_frame_host()->GetSiteInstance()->GetSiteURL());
  EXPECT_EQ(expected_isolated_suborigin_lock,
            child_frame_node->current_frame_host()
                ->GetSiteInstance()
                ->GetProcessLock());
  EXPECT_EQ(child_frame_node->current_frame_host()
                ->GetSiteInstance()
                ->GetProcessLock(),
            ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
                child_frame_node->current_frame_host()->GetProcess()->GetID()));
}

// In this test the sub-origin isn't isolated because the origin policy doesn't
// request "isolation". It will have the same site instance as the main frame.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       SimpleSubOriginNonIsolationTest) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"] })");
  // Start off with an a(a) page, then navigate the subframe to an isolated sub
  // origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  NavigateFrameToURL(child_frame_node, isolated_suborigin_url);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());
}

// This test verifies that renderer-initiated navigations to/from isolated
// sub-origins works as expected.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       RendererInitiatedNavigations) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  GURL isolated_sub_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  {
    // Navigate the child to an isolated origin.
    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecuteScript(
        child, "location.href = '" + isolated_sub_origin_url.spec() + "';"));
    observer.Wait();
  }
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  GURL non_isolated_sub_origin_url(
      https_server()->GetURL("bar.foo.com", "/title1.html"));
  {
    // Navigate the child to a non-isolated origin.
    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecuteScript(
        child,
        "location.href = '" + non_isolated_sub_origin_url.spec() + "';"));
    observer.Wait();
  }
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
}

// Check that navigating a main frame from an non-isolated origin to an
// isolated origin and vice versa swaps processes and uses a new SiteInstance,
// both for renderer-initiated and browser-initiated navigations.
// Note: this test is essentially identical to
// IsolatedOriginTest.MainFrameNavigation.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       MainFrameNavigation) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  GURL unisolated_url(https_server()->GetURL("www.foo.com", "/title1.html"));
  GURL isolated_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));

  EXPECT_TRUE(NavigateToURL(shell(), unisolated_url));

  // Open a same-site popup to keep the www.foo.com process alive.
  Shell* popup = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  SiteInstance* unisolated_instance =
      popup->web_contents()->GetMainFrame()->GetSiteInstance();
  RenderProcessHost* unisolated_process =
      popup->web_contents()->GetMainFrame()->GetProcess();

  // Go to isolated.foo.com with a renderer-initiated navigation.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), isolated_url));
  scoped_refptr<SiteInstance> isolated_instance =
      web_contents()->GetSiteInstance();
  RenderProcessHost* isolated_process =
      web_contents()->GetMainFrame()->GetProcess();

  EXPECT_NE(unisolated_instance, isolated_instance);
  EXPECT_NE(unisolated_process, isolated_process);

  // The site URL for isolated.foo.com should be the full origin rather than
  // scheme and eTLD+1.
  EXPECT_EQ(https_server()->GetURL("isolated.foo.com", "/"),
            isolated_instance->GetSiteURL());

  // Now use a renderer-initiated navigation to go to an unisolated origin,
  // www.foo.com. This should end up back in the |popup|'s process.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), unisolated_url));
  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Now, perform a browser-initiated navigation to an isolated origin and
  // ensure that this ends up in a new process and SiteInstance for
  // isolated.foo.com.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  scoped_refptr<SiteInstance> isolated_instance2 =
      web_contents()->GetSiteInstance();
  RenderProcessHost* isolated_process2 =
      web_contents()->GetMainFrame()->GetProcess();
  EXPECT_NE(unisolated_instance, isolated_instance2);
  EXPECT_NE(isolated_instance, isolated_instance2);
  EXPECT_NE(unisolated_process, isolated_process2);

  // Go back to www.foo.com: this should end up in the unisolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Go back again.  This should go to isolated.foo.com in an isolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Do a renderer-initiated navigation from isolated.foo.com to another
  // isolated origin and ensure there is a different isolated process.
  GURL second_isolated_url(
      https_server()->GetURL("isolated.bar.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), second_isolated_url));
  EXPECT_EQ(https_server()->GetURL("isolated.bar.com", "/"),
            web_contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_instance, web_contents()->GetSiteInstance());
}

// This test ensures that if an origin starts off being isolated in a
// BrowsingInstance, it continues that way within the BrowsingInstance, even
// if a new policy is received that removes the opt-in request.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       OriginIsolationStateRetainedForBrowsingInstance) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  // Start off with an a(a,a) page, then navigate the subframe to an isolated
  // sub origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com, foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(3u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);
  FrameTreeNode* child_frame_node1 = root->child_at(1);

  NavigateFrameToURL(child_frame_node0, isolated_suborigin_url);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change OriginPolicy manifest to stop isolating the sub-origin. It should
  // still be isolated, to remain consistent with the other frame.
  SetOriginPolicyManifest(R"({ })");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "The page did not request origin isolation, but was isolated anyway*");

  NavigateFrameToURL(child_frame_node1, isolated_suborigin_url);

  console_observer.Wait();

  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());

  // The two sub-frames should be in the same site instance.
  EXPECT_EQ(child_frame_node0->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());

  // Make sure the master opt-in list still has the origin tracked.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(isolated_suborigin_url)));
}

// This test ensures that if an origin starts off not being isolated in a
// BrowsingInstance, it continues that way within the BrowsingInstance, even
// if a new opt-in policy is received.
// Case #1 where the non-opted-in origin is currently in the frame tree.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       OriginNonIsolationStateRetainedForBrowsingInstance1) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"] })");
  // Start off with an a(a,a) page, then navigate the subframe to an isolated
  // sub origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com, foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(3u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);
  FrameTreeNode* child_frame_node1 = root->child_at(1);

  NavigateFrameToURL(child_frame_node0, isolated_suborigin_url);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change OriginPolicy manifest to start isolating the sub-origin. It should
  // still be not-isolated, to remain consistent with the other frame.
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "The page requested origin isolation, but could not be isolated*");

  NavigateFrameToURL(child_frame_node1, isolated_suborigin_url);

  console_observer.Wait();

  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());

  // Make sure the master opt-in list has the origin listed.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(isolated_suborigin_url)));
}

// This test ensures that if an origin starts off not being isolated in a
// BrowsingInstance, it continues that way within the BrowsingInstance, even
// if a new opt-in policy is received.
// Case #2 where the non-opted-in origin is currently not in the frame tree.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       OriginNonIsolationStateRetainedForBrowsingInstance2) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"] })");
  // Start off with an a(a) page, then navigate the subframe to an isolated sub
  // origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);

  // Even though we're navigating to isolated.foo.com, there's no manifest
  // requesting opt-in, so it should end up in the same SiteInstance as the
  // main frame.
  NavigateFrameToURL(child_frame_node0, isolated_suborigin_url);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // This navigation removes isolated_suborigin_url from the frame tree, but it
  // should still be in the session history.
  NavigateFrameToURL(child_frame_node0,
                     https_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change OriginPolicy manifest to start isolating the sub-origin. It should
  // still be not isolated, to remain consistent with the other frame.
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  NavigateFrameToURL(child_frame_node0, isolated_suborigin_url);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Make sure the master opt-in list has the origin listed.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(isolated_suborigin_url)));

  // Make sure the current browsing instance does *not* isolate the origin.
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      url::Origin::Create(isolated_suborigin_url),
      false /* origin_requests_isolation */));
}

// This test makes sure that a different tab in the same BrowsingInstance where
// an origin originally did not opt-in respects that state even if the
// OriginPolicy changes.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       OriginNonIsolationStateRetainedForPopup) {
  SetOriginPolicyManifest(R"({ })");
  // Start off with an a(a,a) page, then navigate the subframe to an isolated
  // sub origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);

  NavigateFrameToURL(child_frame_node0, isolated_suborigin_url);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change OriginPolicy manifest to start isolating the sub-origin. It should
  // still be isolated, to remain consistent with the other frame.
  SetOriginPolicyManifest(R"({ "isolation": true })");

  // Open a popup in the same browsing instance, and navigate it to the
  // not-opted-in origin. Even though the manifest now requests isolation, it
  // should not opt-in since it's in the same BrowsingInstance where it
  // originally wasn't opted in.
  Shell* popup = OpenPopup(shell(), isolated_suborigin_url, "foo");
  auto* popup_web_contents = popup->web_contents();
  EXPECT_TRUE(
      NavigateToURLFromRenderer(popup_web_contents, isolated_suborigin_url));

  EXPECT_EQ(shell()->web_contents()->GetSiteInstance()->GetBrowsingInstanceId(),
            popup_web_contents->GetSiteInstance()->GetBrowsingInstanceId());

  // Make sure the current browsing instance does *not* isolate the origin.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      url::Origin::Create(isolated_suborigin_url),
      false /* origin_requests_isolation */));
}

// This test handles the case where the base origin is isolated, but a
// sub-origin isn't. In this case we need to place the sub-origin in a site-
// keyed SiteInstance with the same site URL as the origin-keyed SiteInstance
// used for the isolated base origin. Note: only the isolated base origin will
// have a port in this test, as the non-isolated sub-origin will have its port
// value stripped. The test IsolatedBaseOriginNoPorts tests the case where
// neither the isolated base origin nor the non-isolated sub-origin has a port
// value.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       IsolatedBaseOrigin) {
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  // Start off with an isolated base-origin in an a(a) configuration, then
  // navigate the subframe to a sub-origin no requesting isolation.
  GURL test_url(https_server()->GetURL(
      "foo.com", "/isolated_base_origin_with_subframe.html"));
  GURL non_isolated_sub_origin1(
      https_server()->GetURL("non_isolated1.foo.com", "/title1.html"));
  GURL non_isolated_sub_origin2(
      https_server()->GetURL("non_isolated2.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(3u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node1 = root->child_at(0);
  FrameTreeNode* child_frame_node2 = root->child_at(1);
  NavigateFrameToURL(child_frame_node1, non_isolated_sub_origin1);
  NavigateFrameToURL(child_frame_node2, non_isolated_sub_origin2);

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->ShouldOriginGetOptInIsolation(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      url::Origin::Create(test_url), false /* origin_requests_isolation */));
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      child_frame_node1->current_frame_host()
          ->GetSiteInstance()
          ->GetIsolationContext(),
      url::Origin::Create(non_isolated_sub_origin1),
      false /* origin_requests_isolation */));
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      child_frame_node2->current_frame_host()
          ->GetSiteInstance()
          ->GetIsolationContext(),
      url::Origin::Create(non_isolated_sub_origin2),
      false /* origin_requests_isolation */));

  // Base origin and subdomains should have different SiteInstances.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(root->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_origin_keyed());
  EXPECT_FALSE(child_frame_node1->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_origin_keyed());

  // Both non-isolated subdomains are in the same SiteInstance.
  EXPECT_EQ(child_frame_node1->current_frame_host()->GetSiteInstance(),
            child_frame_node2->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(
      GURL("https://foo.com"),
      child_frame_node1->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // The base-origin and the children are in different processes.
  EXPECT_NE(
      root->current_frame_host()->GetSiteInstance()->GetProcess(),
      child_frame_node1->current_frame_host()->GetSiteInstance()->GetProcess());

  // Make sure the master opt-in list has the base origin as isolated, but not
  // the sub-origins.
  EXPECT_TRUE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(test_url)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(non_isolated_sub_origin1)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(non_isolated_sub_origin2)));
}

// This test is the same as OriginIsolationOptInOriginPolicyTest
// .IsolatedBaseOrigin except it uses port-free URLs. This is critical since we
// can have two SiteInstances with the same SiteURL as long as one is
// origin-keyed and the other isn't. Site URLs used to be used as map-keys but
// with opt-in origin isolation we need to also consider the keying flag.
// When the URLs all have non-default ports, we will never have duplicate
// site URLs since the site-keyed one will have the port stripped.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       IsolatedBaseOriginNoPorts) {
  GURL isolated_base_origin_url("https://foo.com");
  GURL non_isolated_sub_origin_url_a("https://a.foo.com");
  GURL non_isolated_sub_origin_url_b("https://b.foo.com");

  // Since the embedded test server only works for URLs with non-default ports,
  // use a URLLoaderInterceptor to mimic port-free operation. This allows the
  // rest of the test to operate as if all URLs are using the default ports.
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "foo.com") {
          if (params->url_request.url.path() != "/")
            return false;

          const std::string headers =
              "HTTP/1.1 200 OK\n"
              "Content-Type: text/html\n"
              "Origin-Isolation: ?1\n";
          // Note: this call would normally get the headers from
          // isolated_base_origin_with_subframe.html.mock-http-headers,
          // but those are meant for use with a
          // OriginIsolationOptInOriginPolicyTest. and won't work here, so we
          // override them.
          URLLoaderInterceptor::WriteResponse(
              "content/test/data/isolated_base_origin_with_subframe.html",
              params->client.get(), &headers, base::Optional<net::SSLInfo>());
          return true;
        }
        if (params->url_request.url.host() == "a.foo.com" ||
            params->url_request.url.host() == "b.foo.com") {
          URLLoaderInterceptor::WriteResponse("content/test/data/title1.html",
                                              params->client.get());
          return true;
        }
        // Not handled by us.
        return false;
      }));

  // Load the isolated base url.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_base_origin_url));
  EXPECT_EQ(3u, shell()->web_contents()->GetAllFrames().size());

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child_frame_node1 = root->child_at(0);
  FrameTreeNode* child_frame_node2 = root->child_at(1);
  NavigateFrameToURL(child_frame_node1, non_isolated_sub_origin_url_a);
  NavigateFrameToURL(child_frame_node2, non_isolated_sub_origin_url_b);

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->ShouldOriginGetOptInIsolation(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      url::Origin::Create(isolated_base_origin_url),
      false /* origin_requests_isolation */));
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      child_frame_node1->current_frame_host()
          ->GetSiteInstance()
          ->GetIsolationContext(),
      url::Origin::Create(non_isolated_sub_origin_url_a),
      false /* origin_requests_isolation */));
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      child_frame_node2->current_frame_host()
          ->GetSiteInstance()
          ->GetIsolationContext(),
      url::Origin::Create(non_isolated_sub_origin_url_b),
      false /* origin_requests_isolation */));
  // Base origin and subdomains should have different SiteInstances.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(root->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_origin_keyed());
  EXPECT_FALSE(child_frame_node1->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .is_origin_keyed());

  // Both SiteInstances should have the same site URL, because they have no
  // port.
  EXPECT_EQ(
      root->current_frame_host()->GetSiteInstance()->GetSiteURL(),
      child_frame_node1->current_frame_host()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance()->GetSiteInfo(),
            child_frame_node1->current_frame_host()
                ->GetSiteInstance()
                ->GetSiteInfo());

  // Both non-isolated subdomains are in the same SiteInstance.
  EXPECT_EQ(child_frame_node1->current_frame_host()->GetSiteInstance(),
            child_frame_node2->current_frame_host()->GetSiteInstance());

  // The base-origin and the children are in different processes.
  EXPECT_NE(
      root->current_frame_host()->GetSiteInstance()->GetProcess(),
      child_frame_node1->current_frame_host()->GetSiteInstance()->GetProcess());

  // Make sure the master opt-in list has the base origin isolated and the sub
  // origins both not isolated.
  EXPECT_TRUE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(isolated_base_origin_url)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(non_isolated_sub_origin_url_a)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOptInIsolation(
      url::Origin::Create(non_isolated_sub_origin_url_b)));
}

// This test creates a scenario where we have a frame without a
// FrameNavigationEntry, and then we created another frame with the same origin
// that opts-in to isolation. The opt-in triggers a walk of the session history
// and the frame tree ... the session history won't pick up the first frame, but
// the frame-tree walk should.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest, FrameTreeTest) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("bar.com", "/title1.html")));
  // Have tab1 call window.open() to create blank tab2.
  FrameTreeNode* tab1_root = web_contents()->GetFrameTree()->root();
  ShellAddedObserver new_shell_observer;
  ASSERT_TRUE(ExecuteScript(tab1_root->current_frame_host(),
                            "window.w = window.open()"));
  Shell* tab2_shell = new_shell_observer.GetShell();

  // Create iframe in tab2.
  FrameTreeNode* tab2_root =
      static_cast<WebContentsImpl*>(tab2_shell->web_contents())
          ->GetFrameTree()
          ->root();
  ASSERT_TRUE(ExecuteScript(tab2_root->current_frame_host(),
                            "var iframe = document.createElement('iframe');"
                            "document.body.appendChild(iframe);"));
  EXPECT_EQ(1U, tab2_root->child_count());
  FrameTreeNode* tab2_child = tab2_root->child_at(0);
  GURL isolated_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  // The subframe won't be isolated.
  EXPECT_TRUE(NavigateFrameToURL(tab2_child, isolated_origin_url));

  // Do a browser-initiated navigation of tab1 to the same origin, but isolate
  // it this time. This should place the two frames with |isolated_origin_url|
  // into different BrowsingInstances.
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  EXPECT_TRUE(NavigateToURL(shell(), isolated_origin_url));

  // Since the same origin exists in two tabs, but one is isolated and the other
  // isn't, we expect them to be in different BrowsingInstances.
  EXPECT_NE(tab1_root->current_frame_host()->GetSiteInstance(),
            tab2_child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(tab1_root->current_frame_host()
                ->GetSiteInstance()
                ->GetIsolationContext()
                .browsing_instance_id(),
            tab2_child->current_frame_host()
                ->GetSiteInstance()
                ->GetIsolationContext()
                .browsing_instance_id());

  url::Origin isolated_origin = url::Origin::Create(isolated_origin_url);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  // Verify that |isolated origin| is in the non-opt-in list for tab2's
  // child's BrowsingInstance. We do this by requesting opt-in for the origin,
  // then verifying that it is denied by DoesOriginRequestOptInIsolation.
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      tab2_child->current_frame_host()
          ->GetSiteInstance()
          ->GetIsolationContext(),
      isolated_origin, true /* origin_requests_isolation */));
  // Verify that |isolated_origin| in tab1 is indeed isolated.
  EXPECT_TRUE(policy->ShouldOriginGetOptInIsolation(
      tab1_root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      isolated_origin, false /* origin_requests_isolation */));
  // Verify that the tab2 child frame has no FrameNavigationEntry.
  // TODO(wjmaclean): when https://crbug.com/524208 is fixed, this next check
  // will fail, and it should be removed with the CL that fixes 524208.
  EXPECT_EQ(
      nullptr,
      tab2_shell->web_contents()->GetController().GetLastCommittedEntry());

  // Now, create a second frame in tab2 and navigate it to
  // |isolated_origin_url|. Even though isolation is requested, it should not
  // be isolated.
  ASSERT_TRUE(ExecuteScript(tab2_root->current_frame_host(),
                            "var iframe = document.createElement('iframe');"
                            "document.body.appendChild(iframe);"));
  EXPECT_EQ(2U, tab2_root->child_count());
  FrameTreeNode* tab2_child2 = tab2_root->child_at(1);
  NavigateFrameToURL(tab2_child2, isolated_origin_url);
  EXPECT_EQ(tab2_child->current_frame_host()->GetSiteInstance(),
            tab2_child2->current_frame_host()->GetSiteInstance());

  // Check that the two child frames can script each other.
  EXPECT_TRUE(ExecuteScript(tab2_child2, R"(
      parent.frames[0].cross_frame_property_test = 'hello from t2c2'; )"));
  std::string message;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      tab2_child,
      "domAutomationController.send(window.cross_frame_property_test);",
      &message));
  EXPECT_EQ("hello from t2c2", message);
}

// Similar to FrameTreeTest, but we stop the navigation that's not requesting
// isolation at the pending commit state in tab2, then verify that the FrameTree
// walk has correctly registered the origin as non-isolated in tab2, but
// isolated in tab1.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       FrameTreeTestPendingCommit) {
  GURL isolated_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  TestNavigationManager non_isolated_delayer(shell()->web_contents(),
                                             isolated_origin_url);
  shell()->web_contents()->GetController().LoadURL(
      isolated_origin_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(non_isolated_delayer.WaitForResponse());

  Shell* tab2 = CreateBrowser();
  // Do a browser-initiated navigation of tab2 to the same origin, but isolate
  // it this time. This should place the two frames with |isolated_origin_url|
  // into different BrowsingInstances.
  SetOriginPolicyManifest(R"({ "ids": ["my-policy"], "isolation": true })");
  EXPECT_TRUE(NavigateToURL(tab2, isolated_origin_url));

  // Now commit the non-isolated navigation.
  non_isolated_delayer.WaitForNavigationFinished();

  FrameTreeNode* tab1_root = web_contents()->GetFrameTree()->root();
  SiteInstanceImpl* tab1_site_instance =
      tab1_root->current_frame_host()->GetSiteInstance();
  FrameTreeNode* tab2_root = static_cast<WebContentsImpl*>(tab2->web_contents())
                                 ->GetFrameTree()
                                 ->root();
  SiteInstanceImpl* tab2_site_instance =
      tab2_root->current_frame_host()->GetSiteInstance();
  EXPECT_NE(tab1_site_instance, tab2_site_instance);
  EXPECT_NE(tab1_site_instance->GetIsolationContext().browsing_instance_id(),
            tab2_site_instance->GetIsolationContext().browsing_instance_id());

  // Despite the non-isolated navigation only being at pending-commit when we
  // got the response for the isolated navigation, it should be properly
  // registered as non-isolated in its browsing instance.

  url::Origin isolated_origin = url::Origin::Create(isolated_origin_url);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  // Verify that |isolated origin| is in the non-opt-in list for tab1's
  // BrowsingInstance. We do this by requesting opt-in for the origin, then
  // verifying that it is denied by ShouldOriginGetOptInIsolation.
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      tab1_site_instance->GetIsolationContext(), isolated_origin,
      true /* origin_requests_isolation */));

  // Verify that |isolated_origin| in tab2 is indeed isolated.
  EXPECT_TRUE(policy->ShouldOriginGetOptInIsolation(
      tab2_site_instance->GetIsolationContext(), isolated_origin,
      false /* origin_requests_isolation */));
}

// Helper class to navigate a second tab to a specified URL that requests opt-in
// origin isolation just before the first tab processes the next
// DidCommitProvisionalLoad message.
class InjectIsolationRequestingNavigation
    : public DidCommitNavigationInterceptor {
 public:
  InjectIsolationRequestingNavigation(
      OriginIsolationOptInOriginPolicyTest* test_framework,
      WebContents* tab1_web_contents,
      Shell* tab2,
      const GURL& url)
      : DidCommitNavigationInterceptor(tab1_web_contents),
        test_framework_(test_framework),
        tab2_(tab2),
        url_(url) {}

  bool was_called() { return was_called_; }

 private:
  // DidCommitNavigationInterceptor implementation.
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    was_called_ = true;

    // Performa a navigation of |tab2_| to |url_|. |url_| should request
    // isolation.
    test_framework_->SetOriginPolicyManifest(
        R"({ "ids": ["my-policy"], "isolation": true })");
    EXPECT_TRUE(NavigateToURL(tab2_, url_));

    return true;
  }

  OriginIsolationOptInOriginPolicyTest* test_framework_;
  Shell* tab2_;
  const GURL& url_;
  bool was_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(InjectIsolationRequestingNavigation);
};

// TODO(crbug.com/1110767): flaky on Android builders since 2020-07-28.
#if defined(OS_ANDROID)
#define MAYBE_FrameTreeTestBeforeDidCommit DISABLED_FrameTreeTestBeforeDidCommit
#else
#define MAYBE_FrameTreeTestBeforeDidCommit FrameTreeTestBeforeDidCommit
#endif
// This test is similar to the one above, but exercises the pending navigation
// when it's at a different stage, namely between the CommitNavigation and
// DidCommitProvisionalLoad, rather than at WillProcessResponse.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInOriginPolicyTest,
                       MAYBE_FrameTreeTestBeforeDidCommit) {
  GURL isolated_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));

  FrameTreeNode* tab1_root = web_contents()->GetFrameTree()->root();
  // We use the following, slightly more verbose, code instead of
  // CreateBrowser() in order to avoid issues with NavigateToURL() in
  // InjectIsolationRequestingNavigation::WillProcessDidCommitNavigation()
  // getting stuck when it calls for WaitForLoadStop internally.
  Shell* tab2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size());

  InjectIsolationRequestingNavigation injector(this, web_contents(), tab2,
                                               isolated_origin_url);
  EXPECT_TRUE(NavigateToURL(shell(), isolated_origin_url));
  EXPECT_TRUE(injector.was_called());

  SiteInstanceImpl* tab1_site_instance =
      tab1_root->current_frame_host()->GetSiteInstance();
  FrameTreeNode* tab2_root = static_cast<WebContentsImpl*>(tab2->web_contents())
                                 ->GetFrameTree()
                                 ->root();
  SiteInstanceImpl* tab2_site_instance =
      tab2_root->current_frame_host()->GetSiteInstance();
  EXPECT_NE(tab1_site_instance, tab2_site_instance);
  EXPECT_NE(tab1_site_instance->GetIsolationContext().browsing_instance_id(),
            tab2_site_instance->GetIsolationContext().browsing_instance_id());

  // Despite the non-isolated navigation only being at pending-commit when we
  // got the response for the isolated navigation, it should be properly
  // registered as non-isolated in its browsing instance.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin isolated_origin = url::Origin::Create(isolated_origin_url);
  // Verify that |isolated origin| is in the non-opt-in list for tab1's
  // BrowsingInstance. We do this by requesting opt-in for the origin, then
  // verifying that it is denied by DoesOriginRequestOptInIsolation.
  EXPECT_FALSE(policy->ShouldOriginGetOptInIsolation(
      tab1_site_instance->GetIsolationContext(), isolated_origin,
      true /* origin_requests_isolation*/));

  // Verify that |isolated_origin| in tab2 is indeed isolated.
  EXPECT_TRUE(policy->ShouldOriginGetOptInIsolation(
      tab2_site_instance->GetIsolationContext(), isolated_origin,
      false /* origin_requests_isolation */));
}

class StrictOriginIsolationTest : public IsolatedOriginTestBase {
 public:
  StrictOriginIsolationTest() {}
  ~StrictOriginIsolationTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTestBase::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    IsolateAllSitesForTesting(command_line);
    feature_list_.InitAndEnableFeature(features::kStrictOriginIsolation);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  // Helper function that creates an http URL for |host| that includes the test
  // server's port and returns the strict ProcessLock for that URL.
  ProcessLock GetStrictProcessLockForHost(const std::string& host) {
    return GetStrictProcessLock(embedded_test_server()->GetURL(host, "/"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(StrictOriginIsolationTest);
};

IN_PROC_BROWSER_TEST_F(StrictOriginIsolationTest, SubframesAreIsolated) {
  GURL test_url(embedded_test_server()->GetURL(
      "foo.com",
      "/cross_site_iframe_factory.html?"
      "foo.com(mail.foo.com,bar.foo.com(foo.com),foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(5u, shell()->web_contents()->GetAllFrames().size());

  // Make sure we have three separate processes.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHost* main_frame = root->current_frame_host();
  int main_frame_id = main_frame->GetProcess()->GetID();
  RenderFrameHost* child_frame0 = root->child_at(0)->current_frame_host();
  int child_frame0_id = child_frame0->GetProcess()->GetID();
  RenderFrameHost* child_frame1 = root->child_at(1)->current_frame_host();
  int child_frame1_id = child_frame1->GetProcess()->GetID();
  RenderFrameHost* child_frame2 = root->child_at(2)->current_frame_host();
  int child_frame2_id = child_frame2->GetProcess()->GetID();
  RenderFrameHost* grandchild_frame0 =
      root->child_at(1)->child_at(0)->current_frame_host();
  int grandchild_frame0_id = grandchild_frame0->GetProcess()->GetID();
  EXPECT_NE(main_frame_id, child_frame0_id);
  EXPECT_NE(main_frame_id, child_frame1_id);
  EXPECT_EQ(main_frame_id, child_frame2_id);
  EXPECT_EQ(main_frame_id, grandchild_frame0_id);

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_EQ(GetStrictProcessLockForHost("foo.com"),
            policy->GetProcessLock(main_frame_id));
  EXPECT_EQ(GetStrictProcessLockForHost("mail.foo.com"),
            policy->GetProcessLock(child_frame0_id));
  EXPECT_EQ(GetStrictProcessLockForHost("bar.foo.com"),
            policy->GetProcessLock(child_frame1_id));
  EXPECT_EQ(GetStrictProcessLockForHost("foo.com"),
            policy->GetProcessLock(child_frame2_id));
  EXPECT_EQ(GetStrictProcessLockForHost("foo.com"),
            policy->GetProcessLock(grandchild_frame0_id));

  // Navigate child_frame1 to a new origin ... it should get its own process.
  FrameTreeNode* child_frame2_node = root->child_at(2);
  GURL foo_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  const auto expected_foo_lock = GetStrictProcessLock(foo_url);
  NavigateFrameToURL(child_frame2_node, foo_url);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame2_node->current_frame_host()->GetSiteInstance());
  // The old RenderFrameHost for subframe3 will no longer be valid, so get the
  // new one.
  child_frame2 = root->child_at(2)->current_frame_host();
  EXPECT_NE(main_frame->GetProcess()->GetID(),
            child_frame2->GetProcess()->GetID());
  EXPECT_EQ(expected_foo_lock,
            policy->GetProcessLock(child_frame2->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(StrictOriginIsolationTest, MainframesAreIsolated) {
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  const auto expected_foo_lock = GetStrictProcessLock(foo_url);
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  EXPECT_EQ(1u, web_contents()->GetAllFrames().size());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  auto foo_process_id = web_contents()->GetMainFrame()->GetProcess()->GetID();
  SiteInstanceImpl* foo_site_instance = web_contents()->GetSiteInstance();
  EXPECT_EQ(expected_foo_lock, foo_site_instance->GetProcessLock());
  EXPECT_EQ(foo_site_instance->GetProcessLock(),
            policy->GetProcessLock(foo_process_id));

  GURL sub_foo_url =
      embedded_test_server()->GetURL("sub.foo.com", "/title1.html");
  const auto expected_sub_foo_lock = GetStrictProcessLock(sub_foo_url);
  EXPECT_TRUE(NavigateToURL(shell(), sub_foo_url));
  auto sub_foo_process_id =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  SiteInstanceImpl* sub_foo_site_instance = web_contents()->GetSiteInstance();
  EXPECT_EQ(expected_sub_foo_lock, sub_foo_site_instance->GetProcessLock());
  EXPECT_EQ(sub_foo_site_instance->GetProcessLock(),
            policy->GetProcessLock(sub_foo_process_id));

  EXPECT_NE(foo_process_id, sub_foo_process_id);
  EXPECT_NE(foo_site_instance->GetSiteURL(),
            sub_foo_site_instance->GetSiteURL());

  // Now verify with a renderer-initiated navigation.
  GURL another_foo_url(
      embedded_test_server()->GetURL("another.foo.com", "/title2.html"));
  const auto expected_another_foo_lock = GetStrictProcessLock(another_foo_url);
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), another_foo_url));
  auto another_foo_process_id =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  SiteInstanceImpl* another_foo_site_instance =
      web_contents()->GetSiteInstance();
  EXPECT_NE(another_foo_process_id, sub_foo_process_id);
  EXPECT_NE(another_foo_process_id, foo_process_id);
  EXPECT_EQ(expected_another_foo_lock,
            another_foo_site_instance->GetProcessLock());
  EXPECT_EQ(another_foo_site_instance->GetProcessLock(),
            policy->GetProcessLock(another_foo_process_id));
  EXPECT_NE(another_foo_site_instance, foo_site_instance);

  EXPECT_NE(expected_foo_lock, expected_sub_foo_lock);
  EXPECT_NE(expected_sub_foo_lock, expected_another_foo_lock);
  EXPECT_NE(expected_another_foo_lock, expected_foo_lock);
}

// Ensure that navigations across two URLs that resolve to the same effective
// URL won't result in a renderer kill with strict origin isolation. See
// https://crbug.com/961386.
IN_PROC_BROWSER_TEST_F(StrictOriginIsolationTest,
                       NavigateToURLsWithSameEffectiveURL) {
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  GURL app_url(GetWebUIURL("translated"));

  // Set up effective URL translation that maps both |foo_url| and |bar_url| to
  // |app_url|.
  EffectiveURLContentBrowserClient modified_client(
      false /* requires_dedicated_process */);
  modified_client.AddTranslation(foo_url, app_url);
  modified_client.AddTranslation(bar_url, app_url);
  ContentBrowserClient* regular_client =
      SetBrowserClientForTesting(&modified_client);

  // Calculate the expected SiteInfo for each URL.  Both |foo_url| and
  // |bar_url| should have a site URL of |app_url|, but the process locks
  // should be foo.com and bar.com.
  SiteInfo foo_site_info = SiteInstanceImpl::ComputeSiteInfoForTesting(
      web_contents()->GetSiteInstance()->GetIsolationContext(), foo_url);
  EXPECT_EQ(app_url, foo_site_info.site_url());
  EXPECT_EQ(foo_url.GetOrigin(), foo_site_info.process_lock_url());
  SiteInfo bar_site_info = SiteInstanceImpl::ComputeSiteInfoForTesting(
      web_contents()->GetSiteInstance()->GetIsolationContext(), bar_url);
  EXPECT_EQ(app_url, bar_site_info.site_url());
  EXPECT_EQ(bar_url.GetOrigin(), bar_site_info.process_lock_url());
  EXPECT_EQ(foo_site_info.site_url(), bar_site_info.site_url());

  // Navigate to foo_url and then to bar_url.  Verify that we end up with
  // correct SiteInfo in each case.
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  scoped_refptr<SiteInstanceImpl> foo_site_instance =
      web_contents()->GetSiteInstance();
  EXPECT_EQ(foo_site_info, foo_site_instance->GetSiteInfo());

  EXPECT_TRUE(NavigateToURL(shell(), bar_url));
  scoped_refptr<SiteInstanceImpl> bar_site_instance =
      web_contents()->GetSiteInstance();
  EXPECT_EQ(bar_site_info, bar_site_instance->GetSiteInfo());

  // Verify that the SiteInstances and processes are different.  In
  // https://crbug.com/961386, we didn't swap processes for the second
  // navigation, leading to renderer kills.
  EXPECT_NE(foo_site_instance.get(), bar_site_instance.get());
  EXPECT_NE(foo_site_instance->GetProcess(), bar_site_instance->GetProcess());

  // Navigate to another site, then repeat this test with a redirect from
  // foo.com to bar.com.  The navigation should throw away the speculative RFH
  // created for foo.com and should commit in a process locked to bar.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  GURL redirect_url(embedded_test_server()->GetURL(
      "foo.com", "/server-redirect?" + bar_url.spec()));
  modified_client.AddTranslation(redirect_url, app_url);
  EXPECT_TRUE(NavigateToURL(shell(), redirect_url, bar_url));
  EXPECT_EQ(bar_site_info, web_contents()->GetSiteInstance()->GetSiteInfo());

  SetBrowserClientForTesting(regular_client);
}

// Check that navigating a main frame from an non-isolated origin to an
// isolated origin and vice versa swaps processes and uses a new SiteInstance,
// both for renderer-initiated and browser-initiated navigations.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, MainFrameNavigation) {
  GURL unisolated_url(
      embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), unisolated_url));

  // Open a same-site popup to keep the www.foo.com process alive.
  Shell* popup = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  SiteInstance* unisolated_instance =
      popup->web_contents()->GetMainFrame()->GetSiteInstance();
  RenderProcessHost* unisolated_process =
      popup->web_contents()->GetMainFrame()->GetProcess();

  // Go to isolated.foo.com with a renderer-initiated navigation.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), isolated_url));
  scoped_refptr<SiteInstance> isolated_instance =
      web_contents()->GetSiteInstance();
  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // The site URL for isolated.foo.com should be the full origin rather than
  // scheme and eTLD+1.
  EXPECT_EQ(GURL("http://isolated.foo.com/"), isolated_instance->GetSiteURL());

  // Now use a renderer-initiated navigation to go to an unisolated origin,
  // www.foo.com. This should end up back in the |popup|'s process.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), unisolated_url));
  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Now, perform a browser-initiated navigation to an isolated origin and
  // ensure that this ends up in a new process and SiteInstance for
  // isolated.foo.com.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_NE(web_contents()->GetSiteInstance(), unisolated_instance);
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(), unisolated_process);

  // Go back to www.foo.com: this should end up in the unisolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Go back again.  This should go to isolated.foo.com in an isolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process, web_contents()->GetMainFrame()->GetProcess());

  // Do a renderer-initiated navigation from isolated.foo.com to another
  // isolated origin and ensure there is a different isolated process.
  GURL second_isolated_url(
      embedded_test_server()->GetURL("isolated.bar.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), second_isolated_url));
  EXPECT_EQ(GURL("http://isolated.bar.com/"),
            web_contents()->GetSiteInstance()->GetSiteURL());
  EXPECT_NE(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_instance, web_contents()->GetSiteInstance());
}

// Check that opening a popup for an isolated origin puts it into a new process
// and its own SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, Popup) {
  GURL unisolated_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), unisolated_url));

  // Open a popup to a URL with an isolated origin and ensure that there was a
  // process swap.
  Shell* popup = OpenPopup(shell(), isolated_url, "foo");

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            popup->web_contents()->GetSiteInstance());

  // The popup's site URL should match the full isolated origin.
  EXPECT_EQ(GURL("http://isolated.foo.com/"),
            popup->web_contents()->GetSiteInstance()->GetSiteURL());

  // Now open a second popup from an isolated origin to a URL with an
  // unisolated origin and ensure that there was another process swap.
  Shell* popup2 = OpenPopup(popup, unisolated_url, "bar");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            popup2->web_contents()->GetSiteInstance());
  EXPECT_NE(popup->web_contents()->GetSiteInstance(),
            popup2->web_contents()->GetSiteInstance());
}

// Check that navigating a subframe to an isolated origin puts the subframe
// into an OOPIF and its own SiteInstance.  Also check that the isolated
// frame's subframes also end up in correct SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, Subframe) {
  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(child->current_url(), isolated_url);

  // Verify that the child frame is an OOPIF with a different SiteInstance.
  EXPECT_NE(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(GURL("http://isolated.foo.com/"),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // Verify that the isolated frame's subframe (which starts out at a relative
  // path) is kept in the isolated parent's SiteInstance.
  FrameTreeNode* grandchild = child->child_at(0);
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());

  // Navigating the grandchild to www.foo.com should put it into the top
  // frame's SiteInstance.
  GURL non_isolated_url(
      embedded_test_server()->GetURL("www.foo.com", "/title3.html"));
  TestFrameNavigationObserver observer(grandchild);
  EXPECT_TRUE(ExecuteScript(
      grandchild, "location.href = '" + non_isolated_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(non_isolated_url, grandchild->current_url());

  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
}

// Check that when an non-isolated origin foo.com embeds a subframe from an
// isolated origin, which then navigates to a non-isolated origin bar.com,
// bar.com goes back to the main frame's SiteInstance.  See
// https://crbug.com/711006.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       NoOOPIFWhenIsolatedOriginNavigatesToNonIsolatedOrigin) {
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));

  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(isolated_url, child->current_url());

  // Verify that the child frame is an OOPIF with a different SiteInstance.
  EXPECT_NE(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(GURL("http://isolated.foo.com/"),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // Navigate the child frame cross-site, but to a non-isolated origin. When
  // not in --site-per-process, this should bring the subframe back into the
  // main frame's SiteInstance.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_FALSE(IsIsolatedOrigin(bar_url));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()->IsCrossProcessSubframe());
}

// Check that a new isolated origin subframe will attempt to reuse an existing
// process for that isolated origin, even across BrowsingInstances.  Also check
// that main frame navigations to an isolated origin keep using the default
// process model and do not reuse existing processes.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, SubframeReusesExistingProcess) {
  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Open an unrelated tab in a separate BrowsingInstance, and navigate it to
  // to an isolated origin.  This SiteInstance should have a default process
  // reuse policy - only subframes attempt process reuse.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, isolated_url));
  scoped_refptr<SiteInstanceImpl> second_shell_instance =
      static_cast<SiteInstanceImpl*>(
          second_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      root->current_frame_host()->GetSiteInstance()));
  RenderProcessHost* isolated_process = second_shell_instance->GetProcess();
  EXPECT_EQ(SiteInstanceImpl::ProcessReusePolicy::DEFAULT,
            second_shell_instance->process_reuse_policy());

  // Now navigate the first tab's subframe to an isolated origin.  See that it
  // reuses the existing |isolated_process|.
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(isolated_url, child->current_url());
  EXPECT_EQ(isolated_process, child->current_frame_host()->GetProcess());
  EXPECT_EQ(
      SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE,
      child->current_frame_host()->GetSiteInstance()->process_reuse_policy());

  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(GURL("http://isolated.foo.com/"),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // The subframe's SiteInstance should still be different from second_shell's
  // SiteInstance, and they should be in separate BrowsingInstances.
  EXPECT_NE(second_shell_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      child->current_frame_host()->GetSiteInstance()));

  // Navigate the second tab to a normal URL with a same-site subframe.  This
  // leaves only the first tab's subframe in the isolated origin process.
  EXPECT_TRUE(NavigateToURL(second_shell, top_url));
  EXPECT_NE(isolated_process,
            second_shell->web_contents()->GetMainFrame()->GetProcess());

  // Navigate the second tab's subframe to an isolated origin, and check that
  // this new subframe reuses the isolated process of the subframe in the first
  // tab, even though the two are in separate BrowsingInstances.
  NavigateIframeToURL(second_shell->web_contents(), "test_iframe",
                      isolated_url);
  FrameTreeNode* second_subframe =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetFrameTree()
          ->root()
          ->child_at(0);
  EXPECT_EQ(isolated_process,
            second_subframe->current_frame_host()->GetProcess());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            second_subframe->current_frame_host()->GetSiteInstance());

  // Open a third, unrelated tab, navigate it to an isolated origin, and check
  // that its main frame doesn't share a process with the existing isolated
  // subframes.
  Shell* third_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(third_shell, isolated_url));
  SiteInstanceImpl* third_shell_instance = static_cast<SiteInstanceImpl*>(
      third_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            second_subframe->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(third_shell_instance->GetProcess(), isolated_process);
}

// Check that when a cross-site, non-isolated-origin iframe opens a popup,
// navigates it to an isolated origin, and then the popup navigates back to its
// opener iframe's site, the popup and the opener iframe end up in the same
// process and can script each other.  See https://crbug.com/796912.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       PopupNavigatesToIsolatedOriginAndBack) {
  // Start on a page with same-site iframe.
  GURL foo_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe cross-site, but not to an isolated origin.  This should
  // stay in the main frame's SiteInstance, unless we're in --site-per-process
  // mode.  (Note that the bug for which this test is written is exclusive to
  // --isolate-origins and does not happen with --site-per-process.)
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  }

  // Open a blank popup from the iframe.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(child, "window.w = window.open();"));
  Shell* new_shell = new_shell_observer.GetShell();

  // Have the opener iframe navigate the popup to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), isolated_url);
    EXPECT_TRUE(ExecuteScript(
        child, "window.w.location.href = '" + isolated_url.spec() + "';"));
    manager.WaitForNavigationFinished();
  }

  // Simulate the isolated origin in the popup navigating back to bar.com.
  GURL bar_url2(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), bar_url2);
    EXPECT_TRUE(
        ExecuteScript(new_shell, "location.href = '" + bar_url2.spec() + "';"));
    manager.WaitForNavigationFinished();
  }

  // Check that the popup ended up in the same SiteInstance as its same-site
  // opener iframe.
  EXPECT_EQ(new_shell->web_contents()->GetMainFrame()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Check that the opener iframe can script the popup.
  std::string popup_location;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child, "domAutomationController.send(window.w.location.href);",
      &popup_location));
  EXPECT_EQ(bar_url2.spec(), popup_location);
}

// Check that when a non-isolated-origin page opens a popup, navigates it
// to an isolated origin, and then the popup navigates to a third non-isolated
// origin and finally back to its opener's origin, the popup and the opener
// iframe end up in the same process and can script each other:
//
//   foo.com
//      |
//  window.open()
//      |
//      V
//  about:blank -> isolated.foo.com -> bar.com -> foo.com
//
// This is a variant of PopupNavigatesToIsolatedOriginAndBack where the popup
// navigates to a third site before coming back to the opener's site. See
// https://crbug.com/807184.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       PopupNavigatesToIsolatedOriginThenToAnotherSiteAndBack) {
  // Start on www.foo.com.
  GURL foo_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a blank popup.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root, "window.w = window.open();"));
  Shell* new_shell = new_shell_observer.GetShell();

  // Have the opener navigate the popup to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), isolated_url);
    EXPECT_TRUE(ExecuteScript(
        root, "window.w.location.href = '" + isolated_url.spec() + "';"));
    manager.WaitForNavigationFinished();
  }

  // Simulate the isolated origin in the popup navigating to bar.com.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), bar_url);
    EXPECT_TRUE(
        ExecuteScript(new_shell, "location.href = '" + bar_url.spec() + "';"));
    manager.WaitForNavigationFinished();
  }

  const SiteInstanceImpl* const root_site_instance_impl =
      static_cast<SiteInstanceImpl*>(
          root->current_frame_host()->GetSiteInstance());
  const SiteInstanceImpl* const newshell_site_instance_impl =
      static_cast<SiteInstanceImpl*>(
          new_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  if (AreDefaultSiteInstancesEnabled()) {
    // When default SiteInstances are enabled, all sites that do not
    // require a dedicated process all end up in the same default SiteInstance.
    EXPECT_EQ(newshell_site_instance_impl, root_site_instance_impl);
    EXPECT_TRUE(newshell_site_instance_impl->IsDefaultSiteInstance());
  } else {
    // At this point, the popup and the opener should still be in separate
    // SiteInstances.
    EXPECT_NE(newshell_site_instance_impl, root_site_instance_impl);
    EXPECT_NE(AreAllSitesIsolatedForTesting(),
              newshell_site_instance_impl->IsDefaultSiteInstance());
    EXPECT_FALSE(root_site_instance_impl->IsDefaultSiteInstance());
  }

  // Simulate the isolated origin in the popup navigating to www.foo.com.
  {
    TestNavigationManager manager(new_shell->web_contents(), foo_url);
    EXPECT_TRUE(
        ExecuteScript(new_shell, "location.href = '" + foo_url.spec() + "';"));
    manager.WaitForNavigationFinished();
  }

  // The popup should now be in the same SiteInstance as its same-site opener.
  EXPECT_EQ(new_shell->web_contents()->GetMainFrame()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());

  // Check that the popup can script the opener.
  std::string opener_location;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_shell, "domAutomationController.send(window.opener.location.href);",
      &opener_location));
  EXPECT_EQ(foo_url.spec(), opener_location);
}

// Check that with an ABA hierarchy, where B is an isolated origin, the root
// and grandchild frames end up in the same process and can script each other.
// See https://crbug.com/796912.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       IsolatedOriginSubframeCreatesGrandchildInRootSite) {
  // Start at foo.com and do a cross-site, renderer-initiated navigation to
  // bar.com, which should stay in the same SiteInstance (outside of
  // --site-per-process mode).  This sets up the main frame such that its
  // SiteInstance's site URL does not match its actual origin - a prerequisite
  // for https://crbug.com/796912 to happen.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/page_with_iframe.html"));
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecuteScript(shell(), "location.href = '" + bar_url.spec() + "';"));
  observer.Wait();

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate bar.com's subframe to an isolated origin with its own subframe.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(isolated_url, child->current_url());
  FrameTreeNode* grandchild = child->child_at(0);

  // Navigate the isolated origin's subframe back to bar.com, completing the
  // ABA hierarchy.
  NavigateFrameToURL(grandchild, bar_url);

  // The root and grandchild should be in the same SiteInstance, and the
  // middle child should be in a different SiteInstance.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());

  // Check that the root frame can script the same-site grandchild frame.
  std::string location;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root, "domAutomationController.send(frames[0][0].location.href);",
      &location));
  EXPECT_EQ(bar_url.spec(), location);
}

// Check that isolated origins can access cookies.  This requires cookie checks
// on the IO thread to be aware of isolated origins.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, Cookies) {
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  EXPECT_TRUE(ExecuteScript(web_contents(), "document.cookie = 'foo=bar';"));

  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents(), "window.domAutomationController.send(document.cookie);",
      &cookie));
  EXPECT_EQ("foo=bar", cookie);
}

// Check that isolated origins won't be placed into processes for other sites
// when over the process limit.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, ProcessLimit) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Navigate to an unisolated foo.com URL with an iframe.
  GURL foo_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderProcessHost* foo_process = root->current_frame_host()->GetProcess();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe to an isolated origin.
  GURL isolated_foo_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_foo_url);

  // Ensure that the subframe was rendered in a new process.
  EXPECT_NE(child->current_frame_host()->GetProcess(), foo_process);

  // Sanity-check IsSuitableHost values for the current processes.
  const IsolationContext& isolation_context =
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext();
  auto is_suitable_host = [&isolation_context](RenderProcessHost* process,
                                               const GURL& url) {
    return RenderProcessHostImpl::IsSuitableHost(
        process, isolation_context,
        SiteInstanceImpl::ComputeSiteInfoForTesting(isolation_context, url),
        /* is_guest= */ false);
  };
  EXPECT_TRUE(is_suitable_host(foo_process, foo_url));
  EXPECT_FALSE(is_suitable_host(foo_process, isolated_foo_url));
  EXPECT_TRUE(is_suitable_host(child->current_frame_host()->GetProcess(),
                               isolated_foo_url));
  EXPECT_FALSE(
      is_suitable_host(child->current_frame_host()->GetProcess(), foo_url));

  // Open a new, unrelated tab and navigate it to isolated.foo.com.  This
  // should use a new, unrelated SiteInstance that reuses the existing isolated
  // origin process from first tab's subframe.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, isolated_foo_url));
  scoped_refptr<SiteInstance> isolated_foo_instance(
      new_shell->web_contents()->GetMainFrame()->GetSiteInstance());
  RenderProcessHost* isolated_foo_process = isolated_foo_instance->GetProcess();
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            isolated_foo_instance);
  EXPECT_FALSE(isolated_foo_instance->IsRelatedSiteInstance(
      child->current_frame_host()->GetSiteInstance()));
  // TODO(alexmos): with --site-per-process, this won't currently reuse the
  // subframe process, because the new SiteInstance will initialize its
  // process while it still has no site (during CreateBrowser()), and since
  // dedicated processes can't currently be reused for a SiteInstance with no
  // site, this creates a new process.  The subsequent navigation to
  // |isolated_foo_url| stays in that new process without consulting whether it
  // can now reuse a different process.  This should be fixed; see
  // https://crbug.com/513036.   Without --site-per-process, this works because
  // the site-less SiteInstance is allowed to reuse the first tab's foo.com
  // process (which isn't dedicated), and then it swaps to the isolated.foo.com
  // process during navigation.
  if (!AreAllSitesIsolatedForTesting())
    EXPECT_EQ(child->current_frame_host()->GetProcess(), isolated_foo_process);

  // Navigate iframe on the first tab to a non-isolated site.  This should swap
  // processes so that it does not reuse the isolated origin's process.
  RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
  NavigateIframeToURL(
      web_contents(), "test_iframe",
      embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  EXPECT_EQ(foo_process, child->current_frame_host()->GetProcess());
  EXPECT_NE(isolated_foo_process, child->current_frame_host()->GetProcess());
  deleted_observer.WaitUntilDeleted();

  // Navigate iframe back to isolated origin.  See that it reuses the
  // |new_shell| process.
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_foo_url);
  EXPECT_NE(foo_process, child->current_frame_host()->GetProcess());
  EXPECT_EQ(isolated_foo_process, child->current_frame_host()->GetProcess());

  // Navigate iframe to a different isolated origin.  Ensure that this creates
  // a third process.
  GURL isolated_bar_url(
      embedded_test_server()->GetURL("isolated.bar.com", "/title3.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_bar_url);
  RenderProcessHost* isolated_bar_process =
      child->current_frame_host()->GetProcess();
  EXPECT_NE(foo_process, isolated_bar_process);
  EXPECT_NE(isolated_foo_process, isolated_bar_process);

  // The new process should only be suitable to host isolated.bar.com, not
  // regular web URLs or other isolated origins.
  EXPECT_TRUE(is_suitable_host(isolated_bar_process, isolated_bar_url));
  EXPECT_FALSE(is_suitable_host(isolated_bar_process, foo_url));
  EXPECT_FALSE(is_suitable_host(isolated_bar_process, isolated_foo_url));

  // Navigate second tab (currently at isolated.foo.com) to the
  // second isolated origin, and see that it switches processes.
  EXPECT_TRUE(NavigateToURL(new_shell, isolated_bar_url));
  EXPECT_NE(foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_NE(isolated_foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_EQ(isolated_bar_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());

  // Navigate second tab to a non-isolated URL and see that it goes back into
  // the www.foo.com process, and that it does not share processes with any
  // isolated origins.
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  EXPECT_EQ(foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_NE(isolated_foo_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_NE(isolated_bar_process,
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// Verify that a navigation to an non-isolated origin does not reuse a process
// from a pending navigation to an isolated origin.  See
// https://crbug.com/738634.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       ProcessReuseWithResponseStartedFromIsolatedOrigin) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start, but don't commit a navigation to an unisolated foo.com URL.
  GURL slow_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  NavigationController::LoadURLParams load_params(slow_url);
  TestNavigationManager foo_delayer(shell()->web_contents(), slow_url);
  shell()->web_contents()->GetController().LoadURL(
      slow_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(foo_delayer.WaitForRequestStart());

  // Open a new, unrelated tab and navigate it to isolated.foo.com.
  Shell* new_shell = CreateBrowser();
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  TestNavigationManager isolated_delayer(new_shell->web_contents(),
                                         isolated_url);
  new_shell->web_contents()->GetController().LoadURL(
      isolated_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // Wait for the response from the isolated origin. After this returns, we made
  // the final pick for the process to use for this navigation as part of
  // NavigationRequest::OnResponseStarted.
  EXPECT_TRUE(isolated_delayer.WaitForResponse());

  // Now, proceed with the response and commit the non-isolated URL.  This
  // should notice that the process that was picked for this navigation is not
  // suitable anymore, as it should have been locked to isolated.foo.com.
  foo_delayer.WaitForNavigationFinished();

  // Commit the isolated origin.
  isolated_delayer.WaitForNavigationFinished();

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// When a navigation uses a siteless SiteInstance, and a second navigation
// commits an isolated origin which reuses the siteless SiteInstance's process
// before the first navigation's response is received, ensure that the first
// navigation can still finish properly and transfer to a new process, without
// an origin lock mismatch. See https://crbug.com/773809.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       ProcessReuseWithLazilyAssignedSiteInstance) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start from an about:blank page, where the SiteInstance will not have a
  // site assigned, but will have an associated process.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  SiteInstanceImpl* starting_site_instance = static_cast<SiteInstanceImpl*>(
      shell()->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(starting_site_instance->HasSite());
  EXPECT_TRUE(starting_site_instance->HasProcess());

  // Inject and click a link to a non-isolated origin www.foo.com.  Note that
  // setting location.href won't work here, as that goes through OpenURL
  // instead of OnBeginNavigation when starting from an about:blank page, and
  // that doesn't trigger this bug.
  GURL foo_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  TestNavigationManager manager(shell()->web_contents(), foo_url);
  InjectAndClickLinkTo(foo_url);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Before response is received, open a new, unrelated tab and navigate it to
  // isolated.foo.com. This reuses the first process, which is still considered
  // unused at this point, and locks it to isolated.foo.com.
  Shell* new_shell = CreateBrowser();
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, isolated_url));
  EXPECT_EQ(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());

  // Wait for response from the first tab.  This should notice that the first
  // process is no longer suitable for the final destination (which is an
  // unisolated URL) and transfer to another process.  In
  // https://crbug.com/773809, this led to a CHECK due to origin lock mismatch.
  manager.WaitForNavigationFinished();

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// Same as ProcessReuseWithLazilyAssignedSiteInstance above, but here the
// navigation with a siteless SiteInstance is for an isolated origin, and the
// unrelated tab loads an unisolated URL which reuses the siteless
// SiteInstance's process.  Although the unisolated URL won't lock that process
// to an origin (except when running with --site-per-process), it should still
// mark it as used and cause the isolated origin to transfer when it receives a
// response. See https://crbug.com/773809.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       ProcessReuseWithLazilyAssignedIsolatedSiteInstance) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start from an about:blank page, where the SiteInstance will not have a
  // site assigned, but will have an associated process.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  SiteInstanceImpl* starting_site_instance = static_cast<SiteInstanceImpl*>(
      shell()->web_contents()->GetMainFrame()->GetSiteInstance());
  EXPECT_FALSE(starting_site_instance->HasSite());
  EXPECT_TRUE(starting_site_instance->HasProcess());
  EXPECT_TRUE(web_contents()->GetMainFrame()->GetProcess()->IsUnused());

  // Inject and click a link to an isolated origin.  Note that
  // setting location.href won't work here, as that goes through OpenURL
  // instead of OnBeginNavigation when starting from an about:blank page, and
  // that doesn't trigger this bug.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  TestNavigationManager manager(shell()->web_contents(), isolated_url);
  InjectAndClickLinkTo(isolated_url);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // Before response is received, open a new, unrelated tab and navigate it to
  // an unisolated URL. This should reuse the first process, which is still
  // considered unused at this point, and marks it as used.
  Shell* new_shell = CreateBrowser();
  GURL foo_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  EXPECT_EQ(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_FALSE(web_contents()->GetMainFrame()->GetProcess()->IsUnused());

  // Wait for response in the first tab.  This should notice that the first
  // process is no longer suitable for the isolated origin because it should
  // already be marked as used, and transfer to another process.
  manager.WaitForNavigationFinished();

  // Ensure that the isolated origin did not share a process with the second
  // tab.
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// Verify that a navigation to an unisolated origin cannot reuse a process from
// a pending navigation to an isolated origin.  Similar to
// ProcessReuseWithResponseStartedFromIsolatedOrigin, but here the non-isolated
// URL is the first to reach OnResponseStarted, which should mark the process
// as "used", so that the isolated origin can't reuse it. See
// https://crbug.com/738634.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       ProcessReuseWithResponseStartedFromUnisolatedOrigin) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start a navigation to an unisolated foo.com URL.
  GURL slow_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  NavigationController::LoadURLParams load_params(slow_url);
  TestNavigationManager foo_delayer(shell()->web_contents(), slow_url);
  shell()->web_contents()->GetController().LoadURL(
      slow_url, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  // Wait for the response for foo.com.  After this returns, we should have made
  // the final pick for the process to use for foo.com, so this should mark the
  // process as "used" and ineligible for reuse by isolated.foo.com below.
  EXPECT_TRUE(foo_delayer.WaitForResponse());

  // Open a new, unrelated tab, navigate it to isolated.foo.com, and wait for
  // the navigation to fully load.
  Shell* new_shell = CreateBrowser();
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, isolated_url));

  // Finish loading the foo.com URL.
  foo_delayer.WaitForNavigationFinished();

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
}

// Verify that when a process has a pending SiteProcessCountTracker entry for
// an isolated origin, and a navigation to a non-isolated origin reuses that
// process, future isolated origin subframe navigations do not reuse that
// process. See https://crbug.com/780661.
IN_PROC_BROWSER_TEST_F(
    IsolatedOriginTest,
    IsolatedSubframeDoesNotReuseUnsuitableProcessWithPendingSiteEntry) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start from an about:blank page, where the SiteInstance will not have a
  // site assigned, but will have an associated process.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(web_contents()->GetMainFrame()->GetProcess()->IsUnused());

  // Inject and click a link to an isolated origin URL which never sends back a
  // response.
  GURL hung_isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/hung"));
  TestNavigationManager manager(web_contents(), hung_isolated_url);
  InjectAndClickLinkTo(hung_isolated_url);

  // Wait for the request and send it.  This will place
  // isolated.foo.com on the list of pending sites for this tab's process.
  EXPECT_TRUE(manager.WaitForRequestStart());
  manager.ResumeNavigation();

  // Open a new, unrelated tab and navigate it to an unisolated URL. This
  // should reuse the first process, which is still considered unused at this
  // point, and mark it as used.
  Shell* new_shell = CreateBrowser();
  GURL foo_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));

  // Navigate iframe on second tab to isolated.foo.com.  This should *not*
  // reuse the first process, even though isolated.foo.com is still in its list
  // of pending sites (from the hung navigation in the first tab).  That
  // process is unsuitable because it now contains www.foo.com.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  NavigateIframeToURL(new_shell->web_contents(), "test_iframe", isolated_url);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(new_shell->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(child->current_frame_host()->GetProcess(),
            root->current_frame_host()->GetProcess());

  // Manipulating cookies from the main frame should not result in a renderer
  // kill.
  EXPECT_TRUE(ExecuteScript(root->current_frame_host(),
                            "document.cookie = 'foo=bar';"));
  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->current_frame_host(),
      "window.domAutomationController.send(document.cookie);", &cookie));
  EXPECT_EQ("foo=bar", cookie);
}

// Similar to the test above, but for a ServiceWorker.  When a process has a
// pending SiteProcessCountTracker entry for an isolated origin, and a
// navigation to a non-isolated origin reuses that process, a ServiceWorker
// subsequently created for that isolated origin shouldn't reuse that process.
// See https://crbug.com/780661 and https://crbug.com/780089.
IN_PROC_BROWSER_TEST_F(
    IsolatedOriginTest,
    IsolatedServiceWorkerDoesNotReuseUnsuitableProcessWithPendingSiteEntry) {
  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start from an about:blank page, where the SiteInstance will not have a
  // site assigned, but will have an associated process.
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_TRUE(web_contents()->GetMainFrame()->GetProcess()->IsUnused());

  // Inject and click a link to an isolated origin URL which never sends back a
  // response.
  GURL hung_isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/hung"));
  TestNavigationManager manager(shell()->web_contents(), hung_isolated_url);
  InjectAndClickLinkTo(hung_isolated_url);

  // Wait for the request and send it.  This will place
  // isolated.foo.com on the list of pending sites for this tab's process.
  EXPECT_TRUE(manager.WaitForRequestStart());
  manager.ResumeNavigation();

  // Open a new, unrelated tab and navigate it to an unisolated URL. This
  // should reuse the first process, which is still considered unused at this
  // point, and mark it as used.
  Shell* new_shell = CreateBrowser();
  GURL foo_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));

  // A SiteInstance created for an isolated origin ServiceWorker should
  // not reuse the unsuitable first process.
  scoped_refptr<SiteInstanceImpl> sw_site_instance =
      SiteInstanceImpl::CreateForServiceWorker(
          web_contents()->GetBrowserContext(), hung_isolated_url,
          /* can_reuse_process= */ true);
  RenderProcessHost* sw_host = sw_site_instance->GetProcess();
  EXPECT_NE(new_shell->web_contents()->GetMainFrame()->GetProcess(), sw_host);

  // Cancel the hung request and commit a real navigation to an isolated
  // origin. This should now end up in the ServiceWorker's process.
  web_contents()->GetFrameTree()->root()->ResetNavigationRequest(false);
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_EQ(web_contents()->GetMainFrame()->GetProcess(), sw_host);
}

// Check that subdomains on an isolated origin (e.g., bar.isolated.foo.com)
// also end up in the isolated origin's SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, IsolatedOriginWithSubdomain) {
  // Start on a page with an isolated origin with a same-site iframe.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstance> isolated_instance =
      web_contents()->GetSiteInstance();

  // Navigate iframe to the isolated origin's subdomain.
  GURL isolated_subdomain_url(
      embedded_test_server()->GetURL("bar.isolated.foo.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_subdomain_url);
  EXPECT_EQ(child->current_url(), isolated_subdomain_url);

  EXPECT_EQ(isolated_instance, child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(GURL("http://isolated.foo.com/"),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // Now try navigating the main frame (renderer-initiated) to the isolated
  // origin's subdomain.  This should not swap processes.
  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecuteScript(web_contents(),
                    "location.href = '" + isolated_subdomain_url.spec() + "'"));
  observer.Wait();
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    // If same-site ProactivelySwapBrowsingInstance is enabled, they should be
    // in different site instances but in the same process.
    EXPECT_NE(isolated_instance, web_contents()->GetSiteInstance());
    EXPECT_EQ(isolated_instance->GetProcess(),
              web_contents()->GetSiteInstance()->GetProcess());
  } else {
    EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  }
}

// This class allows intercepting the OpenLocalStorage method and changing
// the parameters to the real implementation of it.
class StoragePartitonInterceptor
    : public blink::mojom::DomStorageInterceptorForTesting,
      public RenderProcessHostObserver {
 public:
  StoragePartitonInterceptor(
      RenderProcessHostImpl* rph,
      mojo::PendingReceiver<blink::mojom::DomStorage> receiver,
      const url::Origin& origin_to_inject)
      : origin_to_inject_(origin_to_inject) {
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(rph->GetStoragePartition());

    // Bind the real DomStorage implementation.
    mojo::PendingRemote<blink::mojom::DomStorageClient> unused_client;
    ignore_result(unused_client.InitWithNewPipeAndPassReceiver());
    mojo::ReceiverId receiver_id = storage_partition->BindDomStorage(
        rph->GetID(), std::move(receiver), std::move(unused_client));

    // Now replace it with this object and keep a pointer to the real
    // implementation.
    dom_storage_ = storage_partition->dom_storage_receivers_for_testing()
                       .SwapImplForTesting(receiver_id, this);

    // Register the |this| as a RenderProcessHostObserver, so it can be
    // correctly cleaned up when the process exits.
    rph->AddObserver(this);
  }

  // Ensure this object is cleaned up when the process goes away, since it
  // is not owned by anyone else.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    host->RemoveObserver(this);
    delete this;
  }

  // Allow all methods that aren't explicitly overridden to pass through
  // unmodified.
  blink::mojom::DomStorage* GetForwardingInterface() override {
    return dom_storage_;
  }

  // Override this method to allow changing the origin. It simulates a
  // renderer process sending incorrect data to the browser process, so
  // security checks can be tested.
  void OpenLocalStorage(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override {
    GetForwardingInterface()->OpenLocalStorage(origin_to_inject_,
                                               std::move(receiver));
  }

 private:
  // Keep a pointer to the original implementation of the service, so all
  // calls can be forwarded to it.
  blink::mojom::DomStorage* dom_storage_;

  url::Origin origin_to_inject_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitonInterceptor);
};

void CreateTestDomStorageBackend(
    const url::Origin& origin_to_inject,
    RenderProcessHostImpl* rph,
    mojo::PendingReceiver<blink::mojom::DomStorage> receiver) {
  // This object will register as RenderProcessHostObserver, so it will
  // clean itself automatically on process exit.
  new StoragePartitonInterceptor(rph, std::move(receiver), origin_to_inject);
}

// Verify that an isolated renderer process cannot read localStorage of an
// origin outside of its isolated site.
IN_PROC_BROWSER_TEST_F(
    IsolatedOriginTest,
    LocalStorageOriginEnforcement_IsolatedAccessingNonIsolated) {
  auto mismatched_origin = url::Origin::Create(GURL("http://abc.foo.com"));
  EXPECT_FALSE(IsIsolatedOrigin(mismatched_origin));
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackend, mismatched_origin));

  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));

  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  // Use ignore_result here, since on Android the renderer process is
  // terminated, but ExecuteScript still returns true. It properly returns
  // false on all other platforms.
  ignore_result(ExecuteScript(shell()->web_contents()->GetMainFrame(),
                              "localStorage.length;"));
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

#if defined(OS_ANDROID)
#define MAYBE_LocalStorageOriginEnforcement_NonIsolatedAccessingIsolated \
  LocalStorageOriginEnforcement_NonIsolatedAccessingIsolated
#else
// TODO(lukasza): https://crbug.com/566091: Once remote NTP is capable of
// embedding OOPIFs, start enforcing citadel-style checks on desktop
// platforms.
#define MAYBE_LocalStorageOriginEnforcement_NonIsolatedAccessingIsolated \
  DISABLED_LocalStorageOriginEnforcement_NonIsolatedAccessingIsolated
#endif
// Verify that a non-isolated renderer process cannot read localStorage of an
// isolated origin.
//
// TODO(alexmos, lukasza): https://crbug.com/764958: Replicate this test for
// the IO-thread case.
IN_PROC_BROWSER_TEST_F(
    IsolatedOriginTest,
    MAYBE_LocalStorageOriginEnforcement_NonIsolatedAccessingIsolated) {
  auto isolated_origin = url::Origin::Create(GURL("http://isolated.foo.com"));
  EXPECT_TRUE(IsIsolatedOrigin(isolated_origin));

  GURL nonisolated_url(
      embedded_test_server()->GetURL("non-isolated.com", "/title1.html"));
  EXPECT_FALSE(IsIsolatedOrigin(url::Origin::Create(nonisolated_url)));

  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackend, isolated_origin));
  EXPECT_TRUE(NavigateToURL(shell(), nonisolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  // Use ignore_result here, since on Android the renderer process is
  // terminated, but ExecuteScript still returns true. It properly returns
  // false on all other platforms.
  ignore_result(ExecuteScript(shell()->web_contents()->GetMainFrame(),
                              "localStorage.length;"));
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Verify that an IPC request for reading localStorage of an *opaque* origin
// will be rejected.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       LocalStorageOriginEnforcement_OpaqueOrigin) {
  url::Origin precursor_origin =
      url::Origin::Create(GURL("https://non-isolated.com"));
  url::Origin opaque_origin = precursor_origin.DeriveNewOpaqueOrigin();
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackend, opaque_origin));

  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  // Use ignore_result here, since on Android the renderer process is
  // terminated, but ExecuteScript still returns true. It properly returns
  // false on all other platforms.
  ignore_result(ExecuteScript(shell()->web_contents()->GetMainFrame(),
                              "localStorage.length;"));
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

class IsolatedOriginFieldTrialTest : public IsolatedOriginTestBase {
 public:
  IsolatedOriginFieldTrialTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIsolateOrigins,
        {{features::kIsolateOriginsFieldTrialParamName,
          "https://field.trial.com/,https://bar.com/"}});
  }
  ~IsolatedOriginFieldTrialTest() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginFieldTrialTest);
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginFieldTrialTest, Test) {
  bool expected_to_isolate = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolation);

  EXPECT_EQ(expected_to_isolate,
            IsIsolatedOrigin(GURL("https://field.trial.com/")));
  EXPECT_EQ(expected_to_isolate, IsIsolatedOrigin(GURL("https://bar.com/")));
}

class IsolatedOriginCommandLineAndFieldTrialTest
    : public IsolatedOriginFieldTrialTest {
 public:
  IsolatedOriginCommandLineAndFieldTrialTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kIsolateOrigins,
        "https://cmd.line.com/,https://cmdline.com/");
  }

  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginCommandLineAndFieldTrialTest);
};

// Verify that the lists of isolated origins specified via --isolate-origins
// and via field trials are merged.  See https://crbug.com/894535.
IN_PROC_BROWSER_TEST_F(IsolatedOriginCommandLineAndFieldTrialTest, Test) {
  // --isolate-origins should take effect regardless of the
  //   kDisableSiteIsolation opt-out flag.
  EXPECT_TRUE(IsIsolatedOrigin(GURL("https://cmd.line.com/")));
  EXPECT_TRUE(IsIsolatedOrigin(GURL("https://cmdline.com/")));

  // Field trial origins should also take effect, but only if the opt-out flag
  // is not present.
  bool expected_to_isolate = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSiteIsolation);
  EXPECT_EQ(expected_to_isolate,
            IsIsolatedOrigin(GURL("https://field.trial.com/")));
  EXPECT_EQ(expected_to_isolate, IsIsolatedOrigin(GURL("https://bar.com/")));
}

// This is a regression test for https://crbug.com/793350 - the long list of
// origins to isolate used to be unnecessarily propagated to the renderer
// process, trigerring a crash due to exceeding kZygoteMaxMessageLength.
class IsolatedOriginLongListTest : public IsolatedOriginTestBase {
 public:
  IsolatedOriginLongListTest() {}
  ~IsolatedOriginLongListTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::ostringstream origin_list;
    origin_list
        << embedded_test_server()->GetURL("isolated.foo.com", "/").spec();
    for (int i = 0; i < 1000; i++) {
      std::ostringstream hostname;
      hostname << "foo" << i << ".com";

      origin_list << ","
                  << embedded_test_server()->GetURL(hostname.str(), "/").spec();
    }
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    origin_list.str());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginLongListTest, Test) {
  GURL test_url(embedded_test_server()->GetURL(
      "bar1.com",
      "/cross_site_iframe_factory.html?"
      "bar1.com(isolated.foo.com,foo999.com,bar2.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_EQ(4u, shell()->web_contents()->GetAllFrames().size());
  RenderFrameHost* main_frame = shell()->web_contents()->GetMainFrame();
  RenderFrameHost* subframe1 = shell()->web_contents()->GetAllFrames()[1];
  RenderFrameHost* subframe2 = shell()->web_contents()->GetAllFrames()[2];
  RenderFrameHost* subframe3 = shell()->web_contents()->GetAllFrames()[3];
  EXPECT_EQ("bar1.com", main_frame->GetLastCommittedOrigin().GetURL().host());
  EXPECT_EQ("isolated.foo.com",
            subframe1->GetLastCommittedOrigin().GetURL().host());
  EXPECT_EQ("foo999.com", subframe2->GetLastCommittedOrigin().GetURL().host());
  EXPECT_EQ("bar2.com", subframe3->GetLastCommittedOrigin().GetURL().host());

  // bar1.com and bar2.com are not on the list of origins to isolate - they
  // should stay in the same process, unless --site-per-process has also been
  // specified.
  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(main_frame->GetProcess()->GetID(),
              subframe3->GetProcess()->GetID());
    EXPECT_EQ(main_frame->GetSiteInstance(), subframe3->GetSiteInstance());
  }

  // isolated.foo.com and foo999.com are on the list of origins to isolate -
  // they should be isolated from everything else.
  EXPECT_NE(main_frame->GetProcess()->GetID(),
            subframe1->GetProcess()->GetID());
  EXPECT_NE(main_frame->GetSiteInstance(), subframe1->GetSiteInstance());
  EXPECT_NE(main_frame->GetProcess()->GetID(),
            subframe2->GetProcess()->GetID());
  EXPECT_NE(main_frame->GetSiteInstance(), subframe2->GetSiteInstance());
  EXPECT_NE(subframe1->GetProcess()->GetID(), subframe2->GetProcess()->GetID());
  EXPECT_NE(subframe1->GetSiteInstance(), subframe2->GetSiteInstance());
}

// Check that navigating a subframe to an isolated origin error page puts the
// subframe into an OOPIF and its own SiteInstance.  Also check that a
// non-isolated error page in a subframe ends up in the correct SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, SubframeErrorPages) {
  GURL top_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/close-socket"));
  GURL regular_url(embedded_test_server()->GetURL("a.com", "/close-socket"));

  EXPECT_TRUE(NavigateToURL(shell(), top_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(2u, root->child_count());

  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  {
    TestFrameNavigationObserver observer(child1);
    NavigationHandleObserver handle_observer(web_contents(), isolated_url);
    EXPECT_TRUE(ExecuteScript(
        child1, "location.href = '" + isolated_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(child1->current_url(), isolated_url);
    EXPECT_TRUE(handle_observer.is_error());

    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child1->current_frame_host()->GetSiteInstance());
    EXPECT_EQ(GURL("http://isolated.foo.com/"),
              child1->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }

  {
    TestFrameNavigationObserver observer(child2);
    NavigationHandleObserver handle_observer(web_contents(), regular_url);
    EXPECT_TRUE(
        ExecuteScript(child2, "location.href = '" + regular_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(child2->current_url(), regular_url);
    EXPECT_TRUE(handle_observer.is_error());
    if (AreAllSitesIsolatedForTesting()) {
      EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
                child2->current_frame_host()->GetSiteInstance());
      EXPECT_EQ(SiteInstance::GetSiteForURL(web_contents()->GetBrowserContext(),
                                            regular_url),
                child2->current_frame_host()->GetSiteInstance()->GetSiteURL());
    } else {
      EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
                child2->current_frame_host()->GetSiteInstance());
    }
    EXPECT_NE(GURL(kUnreachableWebDataURL),
              child2->current_frame_host()->GetSiteInstance()->GetSiteURL());
  }
}

namespace {
bool HasDefaultSiteInstance(RenderFrameHost* rfh) {
  return static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance())
      ->IsDefaultSiteInstance();
}
}  // namespace

// Verify process assignment behavior for the case where a site that does not
// require isolation embeds a frame that does require isolation, which in turn
// embeds another site that does not require isolation.
// A  (Does not require isolation)
// +-> B (requires isolation)
//     +-> C (different site from A that does not require isolation.)
//         +-> A (same site as top-level which also does not require isolation.)
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, AIsolatedCA) {
  GURL main_url(embedded_test_server()->GetURL(
      "www.foo.com",
      "/cross_site_iframe_factory.html?a(isolated.foo.com(c(www.foo.com)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHost* a = root->current_frame_host();
  RenderFrameHost* b = root->child_at(0)->current_frame_host();
  RenderFrameHost* c = root->child_at(0)->child_at(0)->current_frame_host();
  RenderFrameHost* d =
      root->child_at(0)->child_at(0)->child_at(0)->current_frame_host();

  // Sanity check that the test works with the right frame tree.
  EXPECT_FALSE(IsIsolatedOrigin(a->GetLastCommittedOrigin()));
  EXPECT_TRUE(IsIsolatedOrigin(b->GetLastCommittedOrigin()));
  EXPECT_FALSE(IsIsolatedOrigin(c->GetLastCommittedOrigin()));
  EXPECT_FALSE(IsIsolatedOrigin(d->GetLastCommittedOrigin()));
  EXPECT_EQ("www.foo.com", a->GetLastCommittedURL().host());
  EXPECT_EQ("isolated.foo.com", b->GetLastCommittedURL().host());
  EXPECT_EQ("c.com", c->GetLastCommittedURL().host());
  EXPECT_EQ("www.foo.com", d->GetLastCommittedURL().host());

  // Verify that the isolated site is indeed isolated.
  EXPECT_NE(b->GetProcess()->GetID(), a->GetProcess()->GetID());
  EXPECT_NE(b->GetProcess()->GetID(), c->GetProcess()->GetID());
  EXPECT_NE(b->GetProcess()->GetID(), d->GetProcess()->GetID());

  // Verify that same-origin a and d frames share a process.  This is
  // necessary for correctness - otherwise a and d wouldn't be able to
  // synchronously script each other.
  EXPECT_EQ(a->GetProcess()->GetID(), d->GetProcess()->GetID());

  // Verify that same-origin a and d frames can script each other.
  EXPECT_TRUE(ExecuteScript(a, "window.name = 'a';"));
  EXPECT_TRUE(ExecuteScript(d, R"(
      a = window.open('', 'a');
      a.cross_frame_property_test = 'hello from d'; )"));
  EXPECT_EQ("hello from d",
            EvalJs(a, "window.cross_frame_property_test").ExtractString());

  // The test assertions below are not strictly necessary - they just document
  // the current behavior.  In particular, consolidating www.foo.com and c.com
  // sites into the same process is not necessary for correctness.
  if (AreAllSitesIsolatedForTesting()) {
    // All sites are isolated so we expect foo.com, isolated.foo.com and c.com
    // to all be in their own processes.
    EXPECT_NE(a->GetProcess()->GetID(), b->GetProcess()->GetID());
    EXPECT_NE(a->GetProcess()->GetID(), c->GetProcess()->GetID());
    EXPECT_NE(b->GetProcess()->GetID(), c->GetProcess()->GetID());

    EXPECT_NE(a->GetSiteInstance(), b->GetSiteInstance());
    EXPECT_NE(a->GetSiteInstance(), c->GetSiteInstance());
    EXPECT_EQ(a->GetSiteInstance(), d->GetSiteInstance());
    EXPECT_NE(b->GetSiteInstance(), c->GetSiteInstance());

    EXPECT_FALSE(HasDefaultSiteInstance(a));
    EXPECT_FALSE(HasDefaultSiteInstance(b));
    EXPECT_FALSE(HasDefaultSiteInstance(c));
  } else if (AreDefaultSiteInstancesEnabled()) {
    // All sites that are not isolated should be in the same default
    // SiteInstance process.
    EXPECT_NE(a->GetProcess()->GetID(), b->GetProcess()->GetID());
    EXPECT_EQ(a->GetProcess()->GetID(), c->GetProcess()->GetID());

    EXPECT_NE(a->GetSiteInstance(), b->GetSiteInstance());
    EXPECT_EQ(a->GetSiteInstance(), c->GetSiteInstance());
    EXPECT_EQ(a->GetSiteInstance(), d->GetSiteInstance());
    EXPECT_NE(b->GetSiteInstance(), c->GetSiteInstance());

    EXPECT_TRUE(HasDefaultSiteInstance(a));
    EXPECT_FALSE(HasDefaultSiteInstance(b));
  } else {
    // Documenting current behavior where the top level document doesn't end
    // up in a default SiteInstance even though it is not isolated and does not
    // require a dedicated process. c.com does get placed in a default
    // SiteInstance because we currently allow subframes that don't require
    // isolation to share a process. This behavior should go away once we
    // turn on default SiteInstances by default.
    EXPECT_NE(a->GetProcess()->GetID(), b->GetProcess()->GetID());
    EXPECT_NE(a->GetProcess()->GetID(), c->GetProcess()->GetID());

    EXPECT_NE(a->GetSiteInstance(), b->GetSiteInstance());
    EXPECT_NE(a->GetSiteInstance(), c->GetSiteInstance());
    EXPECT_EQ(a->GetSiteInstance(), d->GetSiteInstance());
    EXPECT_NE(b->GetSiteInstance(), c->GetSiteInstance());

    EXPECT_FALSE(HasDefaultSiteInstance(a));
    EXPECT_FALSE(HasDefaultSiteInstance(b));
    EXPECT_TRUE(HasDefaultSiteInstance(c));
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, NavigateToBlobURL) {
  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(child->current_url(), isolated_url);
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());

  // Now navigate the child frame to a Blob URL.
  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents()->GetMainFrame(),
                            "const b = new Blob(['foo']);\n"
                            "const u = URL.createObjectURL(b);\n"
                            "frames[0].location = u;\n"
                            "URL.revokeObjectURL(u);"));
  load_observer.Wait();
  EXPECT_TRUE(base::StartsWith(child->current_url().spec(),
                               "blob:http://www.foo.com",
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(load_observer.last_navigation_succeeded());
}

// Ensure that --disable-site-isolation-trials disables origin isolation.
class IsolatedOriginTrialOverrideTest : public IsolatedOriginFieldTrialTest {
 public:
  IsolatedOriginTrialOverrideTest() {}

  ~IsolatedOriginTrialOverrideTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginTrialOverrideTest);
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginTrialOverrideTest, Test) {
  if (AreAllSitesIsolatedForTesting())
    return;
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://field.trial.com/")));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://bar.com/")));
}

// Ensure that --disable-site-isolation-trials and/or
// --disable-site-isolation-for-policy do not override the flag.
class IsolatedOriginPolicyOverrideTest : public IsolatedOriginFieldTrialTest {
 public:
  IsolatedOriginPolicyOverrideTest() {}

  ~IsolatedOriginPolicyOverrideTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
#if defined(OS_ANDROID)
    command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginPolicyOverrideTest);
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginPolicyOverrideTest, Test) {
  if (AreAllSitesIsolatedForTesting())
    return;
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://field.trial.com/")));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://bar.com/")));
}

// Ensure that --disable-site-isolation-trials and/or
// --disable-site-isolation-for-policy do not override the flag.
class IsolatedOriginNoFlagOverrideTest : public IsolatedOriginTest {
 public:
  IsolatedOriginNoFlagOverrideTest() {}

  ~IsolatedOriginNoFlagOverrideTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
#if defined(OS_ANDROID)
    command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginNoFlagOverrideTest);
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginNoFlagOverrideTest, Test) {
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  EXPECT_TRUE(IsIsolatedOrigin(isolated_url));
}

// Verify that main frame's origin isolation still keeps all same-origin frames
// in the same process.  When allocating processes for a(b(c),d(c)), we should
// ensure that "c" frames are in the same process.
//
// This is a regression test for https://crbug.com/787576.
IN_PROC_BROWSER_TEST_F(IsolatedOriginNoFlagOverrideTest,
                       SameOriginSubframesProcessSharing) {
  GURL main_url(embedded_test_server()->GetURL(
      "isolated.foo.com", "/cross_site_iframe_factory.html?a(b(c),d(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHost* a = root->current_frame_host();
  RenderFrameHost* b = root->child_at(0)->current_frame_host();
  RenderFrameHost* c1 = root->child_at(0)->child_at(0)->current_frame_host();
  RenderFrameHost* d = root->child_at(1)->current_frame_host();
  RenderFrameHost* c2 = root->child_at(1)->child_at(0)->current_frame_host();

  // Sanity check that the test works with the right frame tree.
  EXPECT_TRUE(IsIsolatedOrigin(a->GetLastCommittedOrigin()));
  EXPECT_FALSE(IsIsolatedOrigin(b->GetLastCommittedOrigin()));
  EXPECT_FALSE(IsIsolatedOrigin(d->GetLastCommittedOrigin()));
  EXPECT_FALSE(IsIsolatedOrigin(c1->GetLastCommittedOrigin()));
  EXPECT_FALSE(IsIsolatedOrigin(c2->GetLastCommittedOrigin()));
  EXPECT_EQ("b.com", b->GetLastCommittedURL().host());
  EXPECT_EQ("d.com", d->GetLastCommittedURL().host());
  EXPECT_EQ("c.com", c1->GetLastCommittedURL().host());
  EXPECT_EQ("c.com", c2->GetLastCommittedURL().host());

  // Verify that the isolated site is indeed isolated.
  EXPECT_NE(a->GetProcess()->GetID(), c1->GetProcess()->GetID());
  EXPECT_NE(a->GetProcess()->GetID(), c2->GetProcess()->GetID());
  EXPECT_NE(a->GetProcess()->GetID(), b->GetProcess()->GetID());
  EXPECT_NE(a->GetProcess()->GetID(), d->GetProcess()->GetID());

  // Verify that same-origin c1 and c2 frames share a process.  This is
  // necessary for correctness - otherwise c1 and c2 wouldn't be able to
  // synchronously script each other.
  EXPECT_EQ(c1->GetProcess()->GetID(), c2->GetProcess()->GetID());

  // Verify that same-origin c1 and c2 frames can script each other.
  EXPECT_TRUE(ExecuteScript(c1, "window.name = 'c1';"));
  EXPECT_TRUE(ExecuteScript(c2, R"(
      c1 = window.open('', 'c1');
      c1.cross_frame_property_test = 'hello from c2'; )"));
  std::string actual_property_value;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      c1, "domAutomationController.send(window.cross_frame_property_test);",
      &actual_property_value));
  EXPECT_EQ("hello from c2", actual_property_value);

  // The test assertions below are not strictly necessary - they just document
  // the current behavior and might be tweaked if needed.  In particular,
  // consolidating b,c,d sites into the same process is not necessary for
  // correctness.  Consolidation might be desirable if we want to limit the
  // number of renderer processes.  OTOH, consolidation might be undesirable
  // if we desire smaller renderer processes (even if it means more processes).
  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(b->GetProcess()->GetID(), c1->GetProcess()->GetID());
    EXPECT_EQ(b->GetProcess()->GetID(), c2->GetProcess()->GetID());
    EXPECT_EQ(b->GetProcess()->GetID(), d->GetProcess()->GetID());
  } else {
    EXPECT_NE(b->GetProcess()->GetID(), c1->GetProcess()->GetID());
    EXPECT_NE(b->GetProcess()->GetID(), c2->GetProcess()->GetID());
    EXPECT_NE(b->GetProcess()->GetID(), d->GetProcess()->GetID());
    EXPECT_EQ(c1->GetProcess()->GetID(), c2->GetProcess()->GetID());
  }
}

// Helper class for testing dynamically-added isolated origins.  Tests that use
// this run without full --site-per-process, but with two isolated origins that
// are configured at startup (isolated.foo.com and isolated.bar.com).
class DynamicIsolatedOriginTest : public IsolatedOriginTest {
 public:
  DynamicIsolatedOriginTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~DynamicIsolatedOriginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
    // This is necessary to use https with arbitrary hostnames.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    if (AreAllSitesIsolatedForTesting()) {
      LOG(WARNING) << "This test should be run without strict site isolation. "
                   << "It does nothing when --site-per-process is specified.";
    }
  }

  void SetUpOnMainThread() override {
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server()->Start());
    IsolatedOriginTest::SetUpOnMainThread();
  }

  // Need an https server because third-party cookies are used, and
  // SameSite=None cookies must be Secure.
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  net::EmbeddedTestServer https_server_;
  DISALLOW_COPY_AND_ASSIGN(DynamicIsolatedOriginTest);
};

// Check that dynamically added isolated origins take effect for future
// BrowsingInstances only.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       IsolationAppliesToFutureBrowsingInstances) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Start on a non-isolated origin with same-site iframe.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe cross-site.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(child->current_url(), bar_url);

  // The two frames should be in the same process, since neither site is
  // isolated so far.
  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
    EXPECT_EQ(root->current_frame_host()->GetProcess(),
              child->current_frame_host()->GetProcess());
  }

  // Start isolating foo.com.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST);

  // The isolation shouldn't take effect in the current frame tree, so that it
  // doesn't break same-site scripting.  Navigate iframe to a foo.com URL and
  // ensure it stays in the same process.
  NavigateIframeToURL(web_contents(), "test_iframe", foo_url);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Also try a foo(bar(foo)) hierarchy and check that all frames are still in
  // the same SiteInstance/process.
  GURL bar_with_foo_url(embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar.com(foo.com)"));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_with_foo_url);
  FrameTreeNode* grandchild = child->child_at(0);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());

  // Create an unrelated window, which will be in a new BrowsingInstance.
  // Ensure that foo.com becomes an isolated origin in that window.  A
  // cross-site bar.com subframe on foo.com should now become an OOPIF.
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, foo_url));

  FrameTreeNode* second_root =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetFrameTree()
          ->root();
  FrameTreeNode* second_child = second_root->child_at(0);

  NavigateIframeToURL(second_shell->web_contents(), "test_iframe", bar_url);
  scoped_refptr<SiteInstance> foo_instance =
      second_root->current_frame_host()->GetSiteInstance();
  EXPECT_NE(foo_instance,
            second_child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(second_root->current_frame_host()->GetProcess(),
            second_child->current_frame_host()->GetProcess());

  // Now try the reverse: ensure that when bar.com embeds foo.com, foo.com
  // becomes an OOPIF.
  EXPECT_TRUE(NavigateToURL(second_shell, bar_with_foo_url));

  // We should've swapped processes in the main frame, since we navigated from
  // (isolated) foo.com to (non-isolated) bar.com.
  EXPECT_NE(foo_instance, second_root->current_frame_host()->GetSiteInstance());

  // Ensure the new foo.com subframe is cross-process.
  second_child = second_root->child_at(0);
  EXPECT_NE(second_root->current_frame_host()->GetSiteInstance(),
            second_child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(second_root->current_frame_host()->GetProcess(),
            second_child->current_frame_host()->GetProcess());
}

// Check that dynamically added isolated origins take effect for future
// BrowsingInstances only, focusing on various main frame navigations.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, MainFrameNavigations) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Create three windows on a non-isolated origin.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell2, foo_url));

  Shell* shell3 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell3, foo_url));

  // Create window.open popups in all three windows, which would prevent a
  // BrowsingInstance swap on renderer-initiated navigations to newly isolated
  // origins in these windows.
  OpenPopup(shell(), foo_url, "");
  OpenPopup(shell2, GURL(url::kAboutBlankURL), "");
  OpenPopup(shell3, embedded_test_server()->GetURL("baz.com", "/title1.html"),
            "");

  // Start isolating bar.com.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(bar_url)},
                             IsolatedOriginSource::TEST);

  // Do a renderer-initiated navigation in each of the existing three windows.
  // None of them should swap to a new process, since bar.com shouldn't be
  // isolated in those older BrowsingInstances.
  int old_process_id = web_contents()->GetMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), bar_url));
  EXPECT_EQ(old_process_id,
            web_contents()->GetMainFrame()->GetProcess()->GetID());

  old_process_id =
      shell2->web_contents()->GetMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell2, bar_url));
  EXPECT_EQ(old_process_id,
            shell2->web_contents()->GetMainFrame()->GetProcess()->GetID());

  old_process_id =
      shell3->web_contents()->GetMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell3, bar_url));
  EXPECT_EQ(old_process_id,
            shell3->web_contents()->GetMainFrame()->GetProcess()->GetID());

  // Now try the same in a new window and BrowsingInstance, and ensure that the
  // navigation to bar.com swaps processes in that case.
  Shell* shell4 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell4, foo_url));

  old_process_id =
      shell4->web_contents()->GetMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell4, bar_url));
  EXPECT_NE(old_process_id,
            shell4->web_contents()->GetMainFrame()->GetProcess()->GetID());

  // Go back to foo.com in window 1, ensuring this stays in the same process.
  {
    old_process_id = web_contents()->GetMainFrame()->GetProcess()->GetID();
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
    EXPECT_EQ(old_process_id,
              web_contents()->GetMainFrame()->GetProcess()->GetID());
  }

  // Go back to foo.com in window 4, ensuring this swaps processes.
  {
    old_process_id =
        shell4->web_contents()->GetMainFrame()->GetProcess()->GetID();
    TestNavigationObserver back_observer(shell4->web_contents());
    shell4->web_contents()->GetController().GoBack();
    back_observer.Wait();
    EXPECT_NE(old_process_id,
              shell4->web_contents()->GetMainFrame()->GetProcess()->GetID());
  }
}

// Check that dynamically added isolated origins do not prevent older processes
// for the same origin from accessing cookies.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, OldProcessCanAccessCookies) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Since foo.com isn't isolated yet, its process lock should allow any site.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(
      policy->GetProcessLock(root->current_frame_host()->GetProcess()->GetID())
          .allows_any_site());

  // Start isolating foo.com.
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST);

  // Create an unrelated window, which will be in a new BrowsingInstance.
  // foo.com will become an isolated origin in that window.
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, foo_url));
  FrameTreeNode* second_root =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetFrameTree()
          ->root();

  // The new window's process should be locked to "foo.com".
  int isolated_foo_com_process_id =
      second_root->current_frame_host()->GetProcess()->GetID();
  EXPECT_EQ(ProcessLockFromUrl("http://foo.com"),
            policy->GetProcessLock(isolated_foo_com_process_id));

  // Make sure both old and new foo.com processes can access cookies without
  // renderer kills.
  EXPECT_TRUE(ExecuteScript(root, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(root, "document.cookie"));
  EXPECT_TRUE(ExecuteScript(second_root, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(second_root, "document.cookie"));

  // Navigate to sub.foo.com in |second_shell|, staying in same
  // BrowsingInstance.  This should stay in the same process.
  GURL sub_foo_url(
      embedded_test_server()->GetURL("sub.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(second_shell, sub_foo_url));
  EXPECT_EQ(isolated_foo_com_process_id,
            second_root->current_frame_host()->GetProcess()->GetID());

  // Now, start isolating sub.foo.com.
  policy->AddIsolatedOrigins({url::Origin::Create(sub_foo_url)},
                             IsolatedOriginSource::TEST);

  // Make sure the process locked to foo.com, which currently has sub.foo.com
  // committed in it, can still access sub.foo.com cookies.
  EXPECT_TRUE(ExecuteScript(second_root, "document.cookie = 'foo=baz';"));
  EXPECT_EQ("foo=baz", EvalJs(second_root, "document.cookie"));

  // Now, navigate to sub.foo.com in a new BrowsingInstance.  This should go
  // into a new process, locked to sub.foo.com.
  // TODO(alexmos): navigating to bar.com prior to navigating to sub.foo.com is
  // currently needed since we only swap BrowsingInstances on cross-site
  // address bar navigations.  We should look into swapping BrowsingInstances
  // even on same-site browser-initiated navigations, in cases where the sites
  // change due to a dynamically isolated origin.
  EXPECT_TRUE(NavigateToURL(
      second_shell, embedded_test_server()->GetURL("bar.com", "/title2.html")));
  EXPECT_TRUE(NavigateToURL(second_shell, sub_foo_url));
  EXPECT_NE(isolated_foo_com_process_id,
            second_root->current_frame_host()->GetProcess()->GetID());
  EXPECT_EQ(ProcessLockFromUrl("http://sub.foo.com"),
            policy->GetProcessLock(
                second_root->current_frame_host()->GetProcess()->GetID()));

  // Make sure that process can also access sub.foo.com cookies.
  EXPECT_TRUE(ExecuteScript(second_root, "document.cookie = 'foo=qux';"));
  EXPECT_EQ("foo=qux", EvalJs(second_root, "document.cookie"));
}

// Verify that when isolating sub.foo.com dynamically, foo.com and sub.foo.com
// start to be treated as cross-site for process model decisions.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, IsolatedSubdomain) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Start isolating sub.foo.com.
  GURL sub_foo_url(
      embedded_test_server()->GetURL("sub.foo.com", "/title1.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(sub_foo_url)},
                             IsolatedOriginSource::TEST);

  // Navigate to foo.com and then to sub.foo.com in a new BrowsingInstance.
  // foo.com and sub.foo.com should now be considered cross-site for the
  // purposes of process assignment, and we should swap processes.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  int initial_process_id =
      new_shell->web_contents()->GetMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(new_shell, sub_foo_url));
  EXPECT_NE(initial_process_id,
            new_shell->web_contents()->GetMainFrame()->GetProcess()->GetID());

  // Repeat this, but now navigate a subframe on foo.com to sub.foo.com and
  // ensure that it is rendered in an OOPIF.
  new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  NavigateIframeToURL(new_shell->web_contents(), "test_iframe", sub_foo_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(new_shell->web_contents())
                            ->GetFrameTree()
                            ->root();
  FrameTreeNode* child = root->child_at(0);

  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

// Check that when an isolated origin takes effect in BrowsingInstance 1, a new
// BrowsingInstance 2, which reuses an old process from BrowsingInstance 1 for
// its main frame, still applies the isolated origin to its subframe.  This
// demonstrates that isolated origins can't be scoped purely based on process
// IDs.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       NewBrowsingInstanceInOldProcess) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Force process reuse for main frames in new BrowsingInstances.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start on a non-isolated origin with same-site iframe.
  GURL foo_url(https_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe cross-site.
  GURL bar_url(https_server()->GetURL("bar.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(child->current_url(), bar_url);

  // The iframe should not be in an OOPIF yet.
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Start isolating bar.com.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(bar_url)},
                             IsolatedOriginSource::TEST);

  // Open a new window in a new BrowsingInstance.  Navigate to foo.com and
  // check that the old foo.com process is reused.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  FrameTreeNode* new_child = new_root->child_at(0);

  EXPECT_EQ(new_root->current_frame_host()->GetProcess(),
            root->current_frame_host()->GetProcess());
  EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(
      new_root->current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
          root->current_frame_host()->GetSiteInstance()));

  // Navigate iframe in the second window to bar.com, and check that it becomes
  // an OOPIF in its own process.
  NavigateIframeToURL(new_shell->web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(new_child->current_url(), bar_url);

  EXPECT_NE(new_child->current_frame_host()->GetProcess(),
            new_root->current_frame_host()->GetProcess());
  EXPECT_NE(new_child->current_frame_host()->GetProcess(),
            root->current_frame_host()->GetProcess());
  EXPECT_NE(new_child->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  EXPECT_NE(new_child->current_frame_host()->GetSiteInstance(),
            new_root->current_frame_host()->GetSiteInstance());
  EXPECT_NE(new_child->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Make sure the bar.com iframe in the old foo.com process can still access
  // bar.com cookies.
  EXPECT_TRUE(ExecuteScript(
      child, "document.cookie = 'foo=bar;SameSite=None;Secure';"));
  EXPECT_EQ("foo=bar", EvalJs(child, "document.cookie"));
}

// Verify that a process locked to foo.com is not reused for a navigation to
// foo.com that does not require a dedicated process.  See
// https://crbug.com/950453.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       LockedProcessNotReusedForNonisolatedSameSiteNavigation) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start on a non-isolated foo.com URL.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Navigate to a different isolated origin and wait for the original foo.com
  // process to shut down.  Note that the foo.com SiteInstance will stick
  // around in session history.
  RenderProcessHostWatcher foo_process_observer(
      web_contents()->GetMainFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_ASSUMES_NO_CACHING);

  GURL isolated_bar_url(
      embedded_test_server()->GetURL("isolated.bar.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_bar_url));
  foo_process_observer.Wait();
  EXPECT_TRUE(foo_process_observer.did_exit_normally());

  // Start isolating foo.com.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST);

  // Create a new window, forcing a new BrowsingInstance, and navigate it to
  // foo.com, which will spin up a process locked to foo.com.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  RenderProcessHost* new_process =
      new_shell->web_contents()->GetMainFrame()->GetProcess();
  EXPECT_EQ(ProcessLockFromUrl("http://foo.com"),
            policy->GetProcessLock(new_process->GetID()));

  // Go to foo.com in the older first tab, where foo.com does not require a
  // dedicated process.  Ensure that the existing locked foo.com process is
  // *not* reused in that case (if that were the case, LockProcessIfNeeded
  // would trigger a CHECK here).  Using a history navigation here ensures that
  // the SiteInstance (from session history) will have a foo.com site URL,
  // rather than a default site URL, since this case isn't yet handled by the
  // default SiteInstance (see crbug.com/787576).
  TestNavigationObserver observer(web_contents());
  web_contents()->GetController().GoBack();
  observer.Wait();
  EXPECT_NE(web_contents()->GetMainFrame()->GetProcess(), new_process);
}

// Checks that isolated origins can be added only for a specific profile,
// and that they don't apply to other profiles.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, PerProfileIsolation) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Create a browser in a different profile.
  BrowserContext* main_context = shell()->web_contents()->GetBrowserContext();
  Shell* other_shell = CreateOffTheRecordBrowser();
  BrowserContext* other_context =
      other_shell->web_contents()->GetBrowserContext();
  ASSERT_NE(main_context, other_context);

  // Start on bar.com in both browsers.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), bar_url));
  EXPECT_TRUE(NavigateToURL(other_shell, bar_url));

  // Start isolating foo.com in |other_context| only.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST, other_context);

  // Verify that foo.com is indeed isolated in |other_shell|, by navigating to
  // it in a new BrowsingInstance and checking that a bar.com subframe becomes
  // an OOPIF.
  EXPECT_TRUE(NavigateToURL(other_shell, foo_url));
  WebContentsImpl* other_contents =
      static_cast<WebContentsImpl*>(other_shell->web_contents());
  NavigateIframeToURL(other_contents, "test_iframe", bar_url);
  FrameTreeNode* root = other_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(child->current_url(), bar_url);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Verify that foo.com is *not* isolated in the regular shell, due to a
  // different profile.
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  root = web_contents()->GetFrameTree()->root();
  child = root->child_at(0);
  EXPECT_EQ(child->current_url(), bar_url);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

// Check that a dynamically added isolated origin can take effect on the next
// main frame navigation by forcing a BrowsingInstance swap, in the case that
// there are no script references to the frame being navigated.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, ForceBrowsingInstanceSwap) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Navigate to a non-isolated page with a cross-site iframe.  The frame
  // shouldn't be in an OOPIF.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(bar.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(first_instance, child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->GetProcessLock(first_instance->GetProcess()->GetID())
                  .allows_any_site());

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST, context);

  // Try navigating to another foo URL.
  GURL foo2_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(baz.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), foo2_url));

  // Verify that this navigation ended up in a dedicated process, and that we
  // swapped BrowsingInstances in the process.
  scoped_refptr<SiteInstance> second_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_NE(first_instance, second_instance);
  EXPECT_FALSE(first_instance->IsRelatedSiteInstance(second_instance.get()));
  EXPECT_NE(first_instance->GetProcess(), second_instance->GetProcess());
  EXPECT_EQ(ProcessLockFromUrl("http://foo.com"),
            policy->GetProcessLock(second_instance->GetProcess()->GetID()));

  // The frame on that page should now be an OOPIF.
  child = root->child_at(0);
  EXPECT_NE(second_instance, child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

// Same as the test above, but using a renderer-initiated navigation.  Check
// that a dynamically added isolated origin can take effect on the next main
// frame navigation by forcing a BrowsingInstance swap, in the case that there
// are no script references to the frame being navigated.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       ForceBrowsingInstanceSwap_RendererInitiated) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Navigate to a foo.com page.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->GetProcessLock(first_instance->GetProcess()->GetID())
                  .allows_any_site());

  // Set a sessionStorage value, to sanity check that foo.com's session storage
  // will still be accessible after the BrowsingInstance swap.
  EXPECT_TRUE(ExecJs(root, "window.sessionStorage['foo'] = 'bar';"));

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST, context);

  // Do a renderer-initiated navigation to another foo URL.
  GURL foo2_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(baz.com)"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), foo2_url));

  // Verify that this navigation ended up in a dedicated process, and that we
  // swapped BrowsingInstances in the process.
  scoped_refptr<SiteInstance> second_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_NE(first_instance, second_instance);
  EXPECT_FALSE(first_instance->IsRelatedSiteInstance(second_instance.get()));
  EXPECT_NE(first_instance->GetProcess(), second_instance->GetProcess());
  EXPECT_EQ(ProcessLockFromUrl("http://foo.com"),
            policy->GetProcessLock(second_instance->GetProcess()->GetID()));

  // The frame on that page should be an OOPIF.
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(second_instance, child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Verify that the isolated foo.com page can still access session storage set
  // by the previous foo.com page.
  EXPECT_EQ("bar", EvalJs(root, "window.sessionStorage['foo']"));
}

IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       DontForceBrowsingInstanceSwapWhenScriptReferencesExist) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Navigate to a page that won't be in a dedicated process.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST, context);

  // Open a popup.
  GURL popup_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  OpenPopup(shell(), popup_url, "");

  // Try navigating the main frame to another foo URL.
  GURL foo2_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), foo2_url));

  // This navigation should not end up in a dedicated process.  The popup
  // should prevent the BrowsingInstance swap heuristic from applying, since it
  // should still be able to communicate with the opener after the navigation.
  EXPECT_EQ(first_instance, root->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(policy->GetProcessLock(first_instance->GetProcess()->GetID())
                  .allows_any_site());
}

// This test ensures that when a page becomes isolated in the middle of
// creating and navigating a new window, the new window prevents a
// BrowsingInstance swap.
IN_PROC_BROWSER_TEST_F(
    DynamicIsolatedOriginTest,
    DontForceBrowsingInstanceSwapWithPendingNavigationInNewWindow) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  // Navigate to a page that won't be in a dedicated process.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());

  // Open and start navigating a popup to a URL that never finishes loading.
  GURL popup_url(embedded_test_server()->GetURL("a.com", "/hung"));
  EXPECT_TRUE(ExecuteScript(root, JsReplace("window.open($1);", popup_url)));

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(foo_url)},
                             IsolatedOriginSource::TEST, context);

  // Navigate the main frame to another foo URL.
  GURL foo2_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), foo2_url));

  // This navigation should not end up in a dedicated process.  The pending
  // navigation in the popup should prevent the BrowsingInstance swap heuristic
  // from applying, since it should still be able to communicate with the
  // opener after the navigation.
  EXPECT_EQ(first_instance, root->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(policy->GetProcessLock(first_instance->GetProcess()->GetID())
                  .allows_any_site());
}

// Test that we're not tracking whether we did a proactive BrowsingInstance
// swap, bfcache eligibility, and whether unload runs after commit or not for
// same-site navigations where we did a BrowsingInstance swap due to dynamic
// isolation (instead of doing it proactively due to
// ProactivelySwapBrowsingInstance).
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       ProactiveSameSiteBISwapHistogramsNotModified) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;
  GURL url_a1(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_a2(embedded_test_server()->GetURL("a.com", "/title2.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  const char kSameSiteNavigationDidSwapHistogramName[] =
      "BackForwardCache.ProactiveSameSiteBISwap.SameSiteNavigationDidSwap";
  const char kEligibilityDuringCommitHistogramName[] =
      "BackForwardCache.ProactiveSameSiteBISwap.EligibilityDuringCommit";
  const char kUnloadRunsAfterCommitHistogramName[] =
      "BackForwardCache.ProactiveSameSiteBISwap.UnloadRunsAfterCommit";
  base::HistogramTester histogram_tester;

  // 1) Navigate to a.com/title1.html.
  EXPECT_TRUE(NavigateToURL(shell(), url_a1));
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  histogram_tester.ExpectTotalCount(kSameSiteNavigationDidSwapHistogramName, 0);
  histogram_tester.ExpectTotalCount(kEligibilityDuringCommitHistogramName, 0);
  histogram_tester.ExpectTotalCount(kUnloadRunsAfterCommitHistogramName, 0);

  // Add unload handler to A1.
  EXPECT_TRUE(
      ExecJs(web_contents->GetMainFrame(), "window.onunload = () => {} "));

  // Start isolating a.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins({url::Origin::Create(url_a1)},
                             IsolatedOriginSource::TEST, context);

  // 2) Navigate same-site from a.com/title1.html to a.com/title2.html, which
  // should trigger a BrowsingInstance swap due to dynamic isolation.
  EXPECT_TRUE(NavigateToURL(shell(), url_a2));
  scoped_refptr<SiteInstance> second_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_NE(first_instance, second_instance);
  EXPECT_FALSE(first_instance->IsRelatedSiteInstance(second_instance.get()));

  // We didn't do a same-site proactive BrowsingInstance swap. Since this
  histogram_tester.ExpectUniqueSample(kSameSiteNavigationDidSwapHistogramName,
                                      false, 1);
  // There's no unload handler in A1 but we should not save anything to
  // UnloadRunsAfterCommit (as it only cares about proactive BrowsingInstance
  // swap cases).
  histogram_tester.ExpectTotalCount(kUnloadRunsAfterCommitHistogramName, 0);
  // A1's eligibility/ineligibility for bfcache should not be counted in
  // EligibilityDuringCommitAfterBISwap histogram (as it only cares about
  // proactive BrowsingInstance swap cases).
  histogram_tester.ExpectTotalCount(kEligibilityDuringCommitHistogramName, 0);
}

// This class allows intercepting the BroadcastChannelProvider::ConnectToChannel
// method and changing the |origin| parameter before passing the call to the
// real implementation of BroadcastChannelProvider.
class BroadcastChannelProviderInterceptor
    : public blink::mojom::BroadcastChannelProviderInterceptorForTesting,
      public RenderProcessHostObserver {
 public:
  BroadcastChannelProviderInterceptor(
      RenderProcessHostImpl* rph,
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver,
      const url::Origin& origin_to_inject)
      : origin_to_inject_(origin_to_inject) {
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(rph->GetStoragePartition());

    // Bind the real BroadcastChannelProvider implementation.
    mojo::ReceiverId receiver_id =
        storage_partition->GetBroadcastChannelProvider()->Connect(
            ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
                rph->GetID()),
            std::move(receiver));

    // Now replace it with this object and keep a pointer to the real
    // implementation.
    original_broadcast_channel_provider_ =
        storage_partition->GetBroadcastChannelProvider()
            ->receivers_for_testing()
            .SwapImplForTesting(receiver_id, this);

    // Register the |this| as a RenderProcessHostObserver, so it can be
    // correctly cleaned up when the process exits.
    rph->AddObserver(this);
  }

  // Ensure this object is cleaned up when the process goes away, since it
  // is not owned by anyone else.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override {
    host->RemoveObserver(this);
    delete this;
  }

  // Allow all methods that aren't explicitly overridden to pass through
  // unmodified.
  blink::mojom::BroadcastChannelProvider* GetForwardingInterface() override {
    return original_broadcast_channel_provider_;
  }

  // Override this method to allow changing the origin. It simulates a
  // renderer process sending incorrect data to the browser process, so
  // security checks can be tested.
  void ConnectToChannel(
      const url::Origin& origin,
      const std::string& name,
      mojo::PendingAssociatedRemote<blink::mojom::BroadcastChannelClient>
          client,
      mojo::PendingAssociatedReceiver<blink::mojom::BroadcastChannelClient>
          connection) override {
    GetForwardingInterface()->ConnectToChannel(
        origin_to_inject_, name, std::move(client), std::move(connection));
  }

 private:
  // Keep a pointer to the original implementation of the service, so all
  // calls can be forwarded to it.
  blink::mojom::BroadcastChannelProvider* original_broadcast_channel_provider_;

  url::Origin origin_to_inject_;

  DISALLOW_COPY_AND_ASSIGN(BroadcastChannelProviderInterceptor);
};

void CreateTestBroadcastChannelProvider(
    const url::Origin& origin_to_inject,
    RenderProcessHostImpl* rph,
    mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver) {
  // This object will register as RenderProcessHostObserver, so it will
  // clean itself automatically on process exit.
  new BroadcastChannelProviderInterceptor(rph, std::move(receiver),
                                          origin_to_inject);
}

// Test verifying that a compromised renderer can't lie about |origin| argument
// passed in the BroadcastChannelProvider::ConnectToChannel IPC message.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, BroadcastChannelOriginEnforcement) {
  auto mismatched_origin = url::Origin::Create(GURL("http://abc.foo.com"));
  EXPECT_FALSE(IsIsolatedOrigin(mismatched_origin));
  RenderProcessHostImpl::SetBroadcastChannelProviderReceiverHandlerForTesting(
      base::BindRepeating(&CreateTestBroadcastChannelProvider,
                          mismatched_origin));

  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetMainFrame()->GetProcess());
  ExecuteScriptAsync(
      shell()->web_contents()->GetMainFrame(),
      "window.test_channel = new BroadcastChannel('test_channel');");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

class IsolatedOriginTestWithStrictSiteInstances : public IsolatedOriginTest {
 public:
  IsolatedOriginTestWithStrictSiteInstances() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kProcessSharingWithStrictSiteInstances);
  }
  ~IsolatedOriginTestWithStrictSiteInstances() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);

    if (AreAllSitesIsolatedForTesting()) {
      LOG(WARNING) << "This test should be run without strict site isolation. "
                   << "It does nothing when --site-per-process is specified.";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginTestWithStrictSiteInstances);
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       NonIsolatedFramesCanShareDefaultProcess) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL top_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(top_url)));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  GURL bar_url(embedded_test_server()->GetURL("www.bar.com", "/title3.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(bar_url)));
  {
    TestFrameNavigationObserver observer(child1);
    NavigationHandleObserver handle_observer(web_contents(), bar_url);
    EXPECT_TRUE(
        ExecuteScript(child1, "location.href = '" + bar_url.spec() + "';"));
    observer.Wait();
  }

  GURL baz_url(embedded_test_server()->GetURL("www.baz.com", "/title3.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(baz_url)));
  {
    TestFrameNavigationObserver observer(child2);
    NavigationHandleObserver handle_observer(web_contents(), baz_url);
    EXPECT_TRUE(
        ExecuteScript(child2, "location.href = '" + baz_url.spec() + "';"));
    observer.Wait();
  }

  // All 3 frames are from different sites, so each should have its own
  // SiteInstance.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child1->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child2->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child1->current_frame_host()->GetSiteInstance(),
            child2->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      FrameTreeVisualizer().DepictFrameTree(root));

  // But none are isolated, so all should share the default process for their
  // BrowsingInstance.
  RenderProcessHost* host = root->current_frame_host()->GetProcess();
  EXPECT_EQ(host, child1->current_frame_host()->GetProcess());
  EXPECT_EQ(host, child2->current_frame_host()->GetProcess());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()
                  ->GetProcessLock(host->GetID())
                  .allows_any_site());
}

// Creates a non-isolated main frame with an isolated child and non-isolated
// grandchild. With strict site isolation disabled and
// kProcessSharingWithStrictSiteInstances enabled, the main frame and the
// grandchild should be in the same process even though they have different
// SiteInstances.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       IsolatedChildWithNonIsolatedGrandchild) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(top_url)));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  ASSERT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(child->current_url(), isolated_url);

  // Verify that the child frame is an OOPIF with a different SiteInstance.
  EXPECT_NE(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());
  EXPECT_EQ(GURL("http://isolated.foo.com/"),
            child->current_frame_host()->GetSiteInstance()->GetSiteURL());

  // Verify that the isolated frame's subframe (which starts out at a relative
  // path) is kept in the isolated parent's SiteInstance.
  FrameTreeNode* grandchild = child->child_at(0);
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());

  // Navigating the grandchild to www.bar.com should put it into the top
  // frame's process, but not its SiteInstance.
  GURL non_isolated_url(
      embedded_test_server()->GetURL("www.bar.com", "/title3.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(non_isolated_url)));
  TestFrameNavigationObserver observer(grandchild);
  EXPECT_TRUE(ExecuteScript(
      grandchild, "location.href = '" + non_isolated_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(non_isolated_url, grandchild->current_url());

  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            grandchild->current_frame_host()->GetProcess());
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://foo.com/\n"
      "      B = http://isolated.foo.com/\n"
      "      C = http://bar.com/",
      FrameTreeVisualizer().DepictFrameTree(root));
}

// Navigate a frame into and out of an isolated origin. This should not
// confuse BrowsingInstance into holding onto a stale default_process_.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       SubframeNavigatesOutofIsolationThenToIsolation) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  ASSERT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child->current_frame_host()->IsCrossProcessSubframe());

  GURL non_isolated_url(
      embedded_test_server()->GetURL("www.foo.com", "/title3.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(non_isolated_url)));
  NavigateIframeToURL(web_contents(), "test_iframe", non_isolated_url);
  EXPECT_EQ(child->current_url(), non_isolated_url);

  // Verify that the child frame is an OOPIF with a different SiteInstance.
  EXPECT_NE(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Navigating the child to the isolated origin again.
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(child->current_url(), isolated_url);
  EXPECT_EQ(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // And navigate out of the isolated origin one last time.
  NavigateIframeToURL(web_contents(), "test_iframe", non_isolated_url);
  EXPECT_EQ(child->current_url(), non_isolated_url);
  EXPECT_NE(web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://isolated.foo.com/\n"
      "      B = http://foo.com/",
      FrameTreeVisualizer().DepictFrameTree(root));
}

// Ensure a popup and its opener can go in the same process, even though
// they have different SiteInstances with kProcessSharingWithStrictSiteInstances
// enabled.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       NonIsolatedPopup) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting())
    return;

  GURL foo_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a blank popup.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root, "window.w = window.open();"));
  Shell* new_shell = new_shell_observer.GetShell();

  // Have the opener navigate the popup to a non-isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("www.bar.com", "/title1.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), isolated_url);
    EXPECT_TRUE(ExecuteScript(
        root, "window.w.location.href = '" + isolated_url.spec() + "';"));
    manager.WaitForNavigationFinished();
  }

  // The popup and the opener should not share a SiteInstance, but should
  // end up in the same process.
  EXPECT_NE(new_shell->web_contents()->GetMainFrame()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            new_shell->web_contents()->GetMainFrame()->GetProcess());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      FrameTreeVisualizer().DepictFrameTree(root));
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = http://bar.com/\n"
      "      B = http://foo.com/",
      FrameTreeVisualizer().DepictFrameTree(
          static_cast<WebContentsImpl*>(new_shell->web_contents())
              ->GetFrameTree()
              ->root()));
}

class WildcardOriginIsolationTest : public IsolatedOriginTestBase {
 public:
  WildcardOriginIsolationTest() {}
  ~WildcardOriginIsolationTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::string origin_list =
        MakeWildcard(embedded_test_server()->GetURL("isolated.foo.com", "/")) +
        "," + embedded_test_server()->GetURL("foo.com", "/").spec();

    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);

    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  const char* kAllSubdomainWildcard = "[*.]";

  // Calling GetURL() on the embedded test server will escape any '*' characters
  // into '%2A', so to create a wildcard origin they must be post-processed to
  // have the string '[*.]' inserted at the correct point.
  std::string MakeWildcard(GURL url) {
    DCHECK(url.is_valid());
    return url.scheme() + url::kStandardSchemeSeparator +
           kAllSubdomainWildcard + url.GetContent();
  }

  DISALLOW_COPY_AND_ASSIGN(WildcardOriginIsolationTest);
};

IN_PROC_BROWSER_TEST_F(WildcardOriginIsolationTest, MainFrameNavigation) {
  GURL a_foo_url(embedded_test_server()->GetURL("a.foo.com", "/title1.html"));
  GURL b_foo_url(embedded_test_server()->GetURL("b.foo.com", "/title1.html"));
  GURL a_isolated_url(
      embedded_test_server()->GetURL("a.isolated.foo.com", "/title1.html"));
  GURL b_isolated_url(
      embedded_test_server()->GetURL("b.isolated.foo.com", "/title1.html"));

  EXPECT_TRUE(IsIsolatedOrigin(a_foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(b_foo_url));
  EXPECT_TRUE(IsIsolatedOrigin(a_isolated_url));
  EXPECT_TRUE(IsIsolatedOrigin(b_isolated_url));

  // Navigate in the following order, all within the same shell:
  // 1. a_foo_url
  // 2. b_foo_url      -- check (1) and (2) have the same pids / instances (*)
  // 3. a_isolated_url
  // 4. b_isolated_url -- check (2), (3) and (4) have distinct pids / instances
  // 5. a_foo_url      -- check (4) and (5) have distinct pids / instances
  // 6. b_foo_url      -- check (5) and (6) have the same pids / instances (*)
  // (*) SiteInstances will be the same unless ProactivelySwapBrowsingInstances
  // is enabled for same-site navigations.
  EXPECT_TRUE(NavigateToURL(shell(), a_foo_url));
  int a_foo_pid =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> a_foo_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  EXPECT_TRUE(NavigateToURL(shell(), b_foo_url));
  int b_foo_pid =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> b_foo_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  // Check that hosts in the wildcard subdomain (but not the wildcard subdomain
  // itself) have their processes reused between navigation events.
  EXPECT_EQ(a_foo_pid, b_foo_pid);
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    EXPECT_NE(a_foo_instance, b_foo_instance);
  } else {
    EXPECT_EQ(a_foo_instance, b_foo_instance);
  }

  EXPECT_TRUE(NavigateToURL(shell(), a_isolated_url));
  int a_isolated_pid =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> a_isolated_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  EXPECT_TRUE(NavigateToURL(shell(), b_isolated_url));
  int b_isolated_pid =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> b_isolated_instance =
      shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  // Navigating from a non-wildcard domain to a wildcard domain should result in
  // a new process.
  EXPECT_NE(b_foo_pid, b_isolated_pid);
  EXPECT_NE(b_foo_instance, b_isolated_instance);

  // Navigating to another URL within the wildcard domain should always result
  // in a new process.
  EXPECT_NE(a_isolated_pid, b_isolated_pid);
  EXPECT_NE(a_isolated_instance, b_isolated_instance);

  EXPECT_TRUE(NavigateToURL(shell(), a_foo_url));
  a_foo_pid = shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  a_foo_instance = shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  EXPECT_TRUE(NavigateToURL(shell(), b_foo_url));
  b_foo_pid = shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  b_foo_instance = shell()->web_contents()->GetMainFrame()->GetSiteInstance();

  // Navigating from the wildcard subdomain to the isolated subdomain should
  // produce a new pid.
  EXPECT_NE(a_foo_pid, b_isolated_pid);
  EXPECT_NE(a_foo_instance, b_isolated_instance);

  // Confirm that navigation events in the isolated domain behave the same as
  // before visiting the wildcard subdomain.
  EXPECT_EQ(a_foo_pid, b_foo_pid);
  if (CanSameSiteMainFrameNavigationsChangeSiteInstances()) {
    EXPECT_NE(a_foo_instance, b_foo_instance);
  } else {
    EXPECT_EQ(a_foo_instance, b_foo_instance);
  }
}

IN_PROC_BROWSER_TEST_F(WildcardOriginIsolationTest, SubFrameNavigation) {
  GURL url = embedded_test_server()->GetURL(
      "a.foo.com",
      "/cross_site_iframe_factory.html?a.foo.com("
      "isolated.foo.com,b.foo.com("
      "b.isolated.foo.com,a.foo.com,a.isolated.com))");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   +--Site A ------- proxies for B C D\n"
      "        |--Site C -- proxies for A B D\n"
      "        |--Site A -- proxies for B C D\n"
      "        +--Site D -- proxies for A B C\n"
      "Where A = http://foo.com/\n"
      "      B = http://isolated.foo.com/\n"
      "      C = http://b.isolated.foo.com/\n"
      "      D = http://isolated.com/",
      FrameTreeVisualizer().DepictFrameTree(root));
}

}  // namespace content
