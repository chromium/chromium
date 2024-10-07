// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/process_lock.h"
#include "content/browser/process_reuse_policy.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/prerender_test_util.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/dom_storage/dom_storage.mojom-test-utils.h"
#include "url/gurl.h"

namespace content {

using IsolatedOriginSource = ChildProcessSecurityPolicy::IsolatedOriginSource;

// This is a base class for all tests in this class.  It does not isolate any
// origins and only provides common helper functions to the other test classes.
class IsolatedOriginTestBase : public ContentBrowserTest {
 public:
  IsolatedOriginTestBase() = default;
  ~IsolatedOriginTestBase() override = default;

  IsolatedOriginTestBase(const IsolatedOriginTestBase&) = delete;
  IsolatedOriginTestBase& operator=(const IsolatedOriginTestBase&) = delete;

  // Check if `origin` is an isolated origin.  This helper is used in tests
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

  OriginAgentClusterIsolationState MakeOACIsolationState(
      bool requires_origin_keyed_process) {
    // Assume |requires_origin_keyed_process| is the same as
    // |is_origin_agent_cluster| here.
    if (!requires_origin_keyed_process) {
      return OriginAgentClusterIsolationState::CreateNonIsolated();
    }
    return OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
        requires_origin_keyed_process);
  }

  bool ShouldOriginGetOptInProcessIsolation(const url::Origin& origin) {
    auto* site_instance = static_cast<SiteInstanceImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());

    OriginAgentClusterIsolationState isolation_request =
        OriginAgentClusterIsolationState::CreateNonIsolated();

    return ChildProcessSecurityPolicyImpl::GetInstance()
        ->DetermineOriginAgentClusterIsolation(
            site_instance->GetIsolationContext(), origin, isolation_request)
        .requires_origin_keyed_process();
  }

  // Assuming no additional explicit opt-in or opt-out was requested, check what
  // isolation state would currently be used for a navigation to |url| in
  // |site_instance| in the test, based on the current state in the
  // BrowsingInstance.
  static OriginAgentClusterIsolationState DetermineOriginAgentClusterIsolation(
      SiteInstanceImpl* site_instance,
      const GURL& url) {
    OriginAgentClusterIsolationState isolation_request =
        site_instance->GetIsolationContext().default_isolation_state();

    return ChildProcessSecurityPolicyImpl::GetInstance()
        ->DetermineOriginAgentClusterIsolation(
            site_instance->GetIsolationContext(), url::Origin::Create(url),
            isolation_request);
  }

  ProcessLock ProcessLockFromUrl(const std::string& url) {
    BrowserContext* browser_context = web_contents()->GetBrowserContext();
    return ProcessLock::FromSiteInfo(SiteInfo(
        /*site_url=*/GURL(url),
        /*process_lock_url=*/GURL(url),
        /*requires_origin_keyed_process=*/false,
        /*requires_origin_keyed_process_by_default=*/false,
        /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
        StoragePartitionConfig::CreateDefault(browser_context),
        WebExposedIsolationInfo::CreateNonIsolated(),
        WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
        /*does_site_request_dedicated_process_for_coop=*/false,
        /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
        /*is_pdf=*/false, /*is_fenced=*/false,
        /*cross_origin_isolation_key=*/std::nullopt));
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  // Helper function that computes an appropriate process lock that corresponds
  // to `url`'s origin (without converting to sites, handling effective URLs,
  // etc). This must be equivalent to what
  // SiteInstanceImpl::DetermineProcessLockURL() would return
  // for strict origin isolation.
  // Note: do not use this for opt-in origin isolation, as it won't set
  // requires_origin_keyed_process to true.
  ProcessLock GetStrictProcessLock(const GURL& url) {
    BrowserContext* browser_context = web_contents()->GetBrowserContext();
    GURL origin_url = url::Origin::Create(url).GetURL();
    return ProcessLock::FromSiteInfo(SiteInfo(
        /*site_url=*/origin_url,
        /*process_lock_url=*/origin_url,
        /*requires_origin_keyed_process=*/false,
        /*requires_origin_keyed_process_by_default=*/false,
        /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
        StoragePartitionConfig::CreateDefault(browser_context),
        WebExposedIsolationInfo::CreateNonIsolated(),
        WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
        /*does_site_request_dedicated_process_for_coop=*/false,
        /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
        /*is_pdf=*/false, /*is_fenced=*/false,
        /*cross_origin_isolation_key=*/std::nullopt));
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
};

class IsolatedOriginTest : public IsolatedOriginTestBase {
 public:
  IsolatedOriginTest() = default;
  ~IsolatedOriginTest() override = default;

  IsolatedOriginTest(const IsolatedOriginTest&) = delete;
  IsolatedOriginTest& operator=(const IsolatedOriginTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTestBase::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    std::string origin_list =
        embedded_test_server()->GetURL("isolated.foo.com", "/").spec() + "," +
        embedded_test_server()->GetURL("isolated.bar.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    IsolatedOriginTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void InjectAndClickLinkTo(GURL url) {
    EXPECT_TRUE(ExecJs(web_contents(),
                       "var link = document.createElement('a');"
                       "link.href = '" +
                           url.spec() +
                           "';"
                           "document.body.appendChild(link);"
                           "link.click();"));
  }
};

// Tests that verify the header can be used to opt-in to origin isolation.
class OriginIsolationOptInHeaderTest : public IsolatedOriginTestBase {
 public:
  OriginIsolationOptInHeaderTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kOriginIsolationHeader},
        /*disabled_features=*/{
            // TODO(https://crbug.com/40259221): update this test to be
            // parameterized on kOriginKeyedProcessesByDefault, and then
            // make sure all the tests have correct expectations both with and
            // without. This will assist in removing the
            // kOriginAgentClusterDefaultEnabled flag.
            blink::features::kOriginAgentClusterDefaultEnabled,
            features::kOriginKeyedProcessesByDefault});
  }
  ~OriginIsolationOptInHeaderTest() override = default;

  OriginIsolationOptInHeaderTest(const OriginIsolationOptInHeaderTest&) =
      delete;
  OriginIsolationOptInHeaderTest& operator=(
      const OriginIsolationOptInHeaderTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTestBase::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    IsolateAllSitesForTesting(command_line);

    // Start the HTTPS server here so derived tests can use it if they override
    // SetUpCommandLine().
    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&OriginIsolationOptInHeaderTest::HandleResponse,
                            base::Unretained(this)));
    ASSERT_TRUE(https_server()->Start());
  }

  void SetHeaderValue(const std::string& header_value) {
    header_ = header_value;
  }

  void SetRedirectTarget(const std::string& redirect_target) {
    redirect_target_ = redirect_target;
  }

  // Allows specifying what content to return when an opt-in isolation header is
  // intercepted. Uses a queue so that multiple requests can be handled without
  // returning to the test body. If the queue is empty, the document content is
  // simply "isolate me!".
  void AddContentToQueue(const std::string& content_str) {
    content_.push(content_str);
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

  // Need an https server because the header requires HTTPS.
  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/isolate_origin") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html");

      if (header_) {
        response->AddCustomHeader("Origin-Agent-Cluster", *header_);
      }

      if (!content_.empty()) {
        response->set_content(content_.front());
        content_.pop();
      } else {
        response->set_content("isolate me!");
      }
      return std::move(response);
    } else if (request.relative_url == "/redirect_me") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->AddCustomHeader("Location", *redirect_target_);
      response->AddCustomHeader("Origin-Agent-Cluster", *header_);
      response->set_content("redirected");
      return std::move(response);
    }

    // If we return nullptr, then the server will go ahead and actually serve
    // the file.
    return nullptr;
  }

  net::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;

  std::optional<std::string> header_;
  std::optional<std::string> redirect_target_;
  std::queue<std::string> content_;
};

// A set of tests that enable OriginAgentCluster by default.
class OriginIsolationDefaultOACTest : public OriginIsolationOptInHeaderTest {
 public:
  OriginIsolationDefaultOACTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kOriginAgentClusterDefaultEnabled);
  }

  ~OriginIsolationDefaultOACTest() override = default;

  OriginIsolationDefaultOACTest(const OriginIsolationDefaultOACTest&) = delete;
  OriginIsolationDefaultOACTest& operator=(OriginIsolationDefaultOACTest&) =
      delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A set of tests that enable process-isolated OriginAgentCluster-by-default.
class OriginKeyedProcessByDefaultTest : public OriginIsolationOptInHeaderTest {
 public:
  OriginKeyedProcessByDefaultTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kOriginAgentClusterDefaultEnabled,
         features::kOriginKeyedProcessesByDefault},
        {});
  }

  ~OriginKeyedProcessByDefaultTest() override = default;

  OriginKeyedProcessByDefaultTest(const OriginKeyedProcessByDefaultTest&) =
      delete;
  OriginKeyedProcessByDefaultTest& operator=(OriginKeyedProcessByDefaultTest&) =
      delete;

  void SetUpOnMainThread() override {
    OriginIsolationOptInHeaderTest::SetUpOnMainThread();
    // Constructing a new BrowserClient also installs it; the old BrowserClient
    // is restored when the new one destructs.
    browser_client_ =
        std::make_unique<OriginAgentClusterByDefaultContentBrowserClient>();
  }

 protected:
  // A custom ContentBrowserClient to allow tests to simulate turning off
  // OriginAgentClusterByDefault.
  class OriginAgentClusterByDefaultContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldDisableOriginAgentClusterDefault(
        BrowserContext* browser_context) override {
      return should_disable_origin_agent_cluster_default_;
    }

    void SetShouldDisableOriginAgentClusterDefault(bool should_disable) {
      should_disable_origin_agent_cluster_default_ = should_disable;
    }

   private:
    bool should_disable_origin_agent_cluster_default_ = false;
  };

  std::unique_ptr<OriginAgentClusterByDefaultContentBrowserClient>
      browser_client_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class OriginIsolationPrerenderOptInHeaderTest
    : public OriginIsolationOptInHeaderTest {
 public:
  OriginIsolationPrerenderOptInHeaderTest()
      : prerender_helper_(base::BindRepeating(
            &OriginIsolationPrerenderOptInHeaderTest::prerender_web_contents,
            base::Unretained(this))) {}
  ~OriginIsolationPrerenderOptInHeaderTest() override = default;

  OriginIsolationPrerenderOptInHeaderTest(
      const OriginIsolationPrerenderOptInHeaderTest&) = delete;
  OriginIsolationPrerenderOptInHeaderTest& operator=(
      const OriginIsolationPrerenderOptInHeaderTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This must be called prior to starting the test server.
    prerender_helper_.RegisterServerRequestMonitor(https_server());
    OriginIsolationOptInHeaderTest::SetUpCommandLine(command_line);
  }

  void set_prerender_web_contents(WebContents* web_contents) {
    prerender_web_contents_ = web_contents->GetWeakPtr();
  }
  WebContents* prerender_web_contents() {
    CHECK(prerender_web_contents_);
    return prerender_web_contents_.get();
  }

 protected:
  test::PrerenderTestHelper prerender_helper_;

 private:
  base::WeakPtr<WebContents> prerender_web_contents_;
};  // class OriginIsolationPrerenderOptInHeaderTest

// As in OriginIsolationOptInHeaderTest, but with same-process origin
// isolation.
class SameProcessOriginIsolationOptInHeaderTest
    : public OriginIsolationOptInHeaderTest {
 public:
  SameProcessOriginIsolationOptInHeaderTest() = default;
  ~SameProcessOriginIsolationOptInHeaderTest() override = default;

  SameProcessOriginIsolationOptInHeaderTest(
      const SameProcessOriginIsolationOptInHeaderTest&) = delete;
  SameProcessOriginIsolationOptInHeaderTest& operator=(
      const SameProcessOriginIsolationOptInHeaderTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OriginIsolationOptInHeaderTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
    command_line->RemoveSwitch(switches::kSitePerProcess);
  }
};

// As in SameProcessOriginIsolationOptInHeaderTest, but command-line isolate
// foo.com.
class SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest
    : public SameProcessOriginIsolationOptInHeaderTest {
 public:
  SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest() = default;
  ~SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest() override =
      default;

  SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest(
      const SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest&) =
      delete;
  SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest& operator=(
      const SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SameProcessOriginIsolationOptInHeaderTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    "https://foo.com/");
  }
};

// Force WebSecurity off for tests.
class SameProcessNoWebSecurityOriginIsolationOptInHeaderTest
    : public SameProcessOriginIsolationOptInHeaderTest {
 public:
  SameProcessNoWebSecurityOriginIsolationOptInHeaderTest() = default;
  ~SameProcessNoWebSecurityOriginIsolationOptInHeaderTest() override = default;

  // Disallow copy & assign.
  SameProcessNoWebSecurityOriginIsolationOptInHeaderTest(
      const SameProcessNoWebSecurityOriginIsolationOptInHeaderTest&) = delete;
  SameProcessNoWebSecurityOriginIsolationOptInHeaderTest& operator=(
      const SameProcessNoWebSecurityOriginIsolationOptInHeaderTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SameProcessOriginIsolationOptInHeaderTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }
};

// Used for a few tests that check non-HTTPS secure context behavior.
class OriginIsolationOptInHttpServerHeaderTest : public IsolatedOriginTestBase {
 public:
  OriginIsolationOptInHttpServerHeaderTest() = default;

  OriginIsolationOptInHttpServerHeaderTest(
      const OriginIsolationOptInHttpServerHeaderTest&) = delete;
  OriginIsolationOptInHttpServerHeaderTest& operator=(
      const OriginIsolationOptInHttpServerHeaderTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTestBase::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    IsolateAllSitesForTesting(command_line);

    feature_list_.InitAndEnableFeature(features::kOriginIsolationHeader);

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &OriginIsolationOptInHttpServerHeaderTest::HandleResponse,
        base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->AddCustomHeader("Origin-Agent-Cluster", "?1");

    response->set_content("isolate me!");
    return std::move(response);
  }

  base::test::ScopedFeatureList feature_list_;
};

// This class allows testing the interaction of OptIn isolation and command-line
// isolation for origins. Tests using this class will isolate foo.com and
// bar.com by default using command-line isolation, but any opt-in isolation
// will override this.
class OriginIsolationOptInHeaderCommandLineTest
    : public OriginIsolationOptInHeaderTest {
 public:
  OriginIsolationOptInHeaderCommandLineTest() = default;

  OriginIsolationOptInHeaderCommandLineTest(
      const OriginIsolationOptInHeaderCommandLineTest&) = delete;
  OriginIsolationOptInHeaderCommandLineTest& operator=(
      const OriginIsolationOptInHeaderCommandLineTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OriginIsolationOptInHeaderTest::SetUpCommandLine(command_line);
    // The base class should already have started the HTTPS server so we can use
    // it here to generate origins to specify on the command line.
    ASSERT_TRUE(https_server()->Started());

    std::string origin_list = https_server()->GetURL("foo.com", "/").spec() +
                              "," +
                              https_server()->GetURL("bar.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }
};

// This test verifies that opt-in isolation takes precedence over command-line
// isolation. It loads an opt-in isolated base origin (which would have
// otherwise been isolated via command-line isolation), and then loads a child
// frame sub-origin which should-not be isolated (but would have been if the
// base origin was command-line isolated).
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderCommandLineTest,
                       OptInOverridesCommandLine) {
  SetHeaderValue("?1");
  // Start off with an isolated base-origin in an a(a) configuration, then
  // navigate the subframe to a sub-origin not requesting isolation.
  // Note: this works because we serve mock headers with the base origin's html
  // file, which set the header.
  GURL isolated_base_origin_url(https_server()->GetURL(
      "foo.com", "/isolated_base_origin_with_subframe.html"));
  GURL non_isolated_sub_origin(
      https_server()->GetURL("non_isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_base_origin_url));
  // The .html main frame has two iframes, this test only uses the first one.
  EXPECT_EQ(3u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, non_isolated_sub_origin));

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      root->current_frame_host()
                          ->GetSiteInstance()
                          ->GetIsolationContext(),
                      url::Origin::Create(isolated_base_origin_url),
                      MakeOACIsolationState(false))
                  .requires_origin_keyed_process());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       root->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(non_isolated_sub_origin),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
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
                  .requires_origin_keyed_process());
  EXPECT_FALSE(child_frame_node->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .requires_origin_keyed_process());

  // Make sure the master opt-in list has the base origin isolated and the sub
  // origin not isolated.
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(isolated_base_origin_url)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(non_isolated_sub_origin)));
}

// A test to verify that an origin with a trailing dot in the domain name
// doesn't crash when it opts-out of origin isolation when
// kOriginAgentClusterDefaultEnabled is enabled.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       TrailingDotDomainOptOutDoesNotCrash) {
  GURL dotted_nonisolated_url(
      https_server()->GetURL("a.com.", "/isolate_origin"));

  // Set header to opt this domain out of default OriginAgentCluster.
  SetHeaderValue("?0");
  EXPECT_TRUE(NavigateToURL(shell(), dotted_nonisolated_url));
  url::Origin origin(url::Origin::Create(dotted_nonisolated_url));
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
}

// A test to confirm that "a.com." is treated as a separate host (and hence
// a separate origin) from "a.com". See example at
// https://url.spec.whatwg.org/#concept-domain.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       TrailingDotDomainIsolatesSeparately1) {
  GURL main_frame_url(https_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(foo.com,foo.com)"));
  GURL isolated_url(https_server()->GetURL("a.com", "/isolate_origin"));
  GURL dotted_isolated_url(https_server()->GetURL("a.com.", "/isolate_origin"));
  SetHeaderValue("?1");

  // Create page with sibling iframes.
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2u, root->child_count());
  FrameTreeNode* child0_frame_node = root->child_at(0);
  FrameTreeNode* child1_frame_node = root->child_at(1);
  EXPECT_TRUE(NavigateToURLFromRenderer(child0_frame_node, isolated_url));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child1_frame_node, dotted_isolated_url));

  url::Origin child0_origin(url::Origin::Create(isolated_url));
  url::Origin child1_origin(url::Origin::Create(dotted_isolated_url));
  EXPECT_NE(isolated_url, dotted_isolated_url);
  EXPECT_NE(child0_origin, child1_origin);

  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(child0_origin));
  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(child1_origin));

  scoped_refptr<SiteInstanceImpl> child0_site_instance =
      child0_frame_node->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> child1_site_instance =
      child1_frame_node->current_frame_host()->GetSiteInstance();
  EXPECT_NE(child0_site_instance, child1_site_instance);
  EXPECT_NE(child0_site_instance->GetProcess(),
            child1_site_instance->GetProcess());
}

// A test similar to TrailingDotDomainIsolatesSeparately1, but this time the
// "a.com" domain does not opt-in via a header, and does not get an origin-
// keyed process. Thus, it ends up in a separate process from "a.com.".
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       TrailingDotDomainIsolatesSeparately2) {
  GURL main_frame_url(https_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(foo.com,foo.com)"));
  GURL non_isolated_url(https_server()->GetURL("a.com", "/title1.html"));
  GURL dotted_isolated_url(https_server()->GetURL("a.com.", "/isolate_origin"));

  // Create page with sibling iframes.
  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2u, root->child_count());
  FrameTreeNode* child0_frame_node = root->child_at(0);
  FrameTreeNode* child1_frame_node = root->child_at(1);
  SetHeaderValue("?1");
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child0_frame_node, dotted_isolated_url));
  SetHeaderValue("");
  EXPECT_TRUE(NavigateToURLFromRenderer(child1_frame_node, non_isolated_url));

  url::Origin child0_origin(url::Origin::Create(dotted_isolated_url));
  url::Origin child1_origin(url::Origin::Create(non_isolated_url));

  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(child0_origin));
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(child1_origin));

  scoped_refptr<SiteInstanceImpl> child0_site_instance =
      child0_frame_node->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> child1_site_instance =
      child1_frame_node->current_frame_host()->GetSiteInstance();
  EXPECT_NE(child0_site_instance, child1_site_instance);
  EXPECT_NE(child0_site_instance->GetProcess(),
            child1_site_instance->GetProcess());
}

// A test to confirm that if an Origin-Agent-Cluster header is encountered (but
// not committed) as part of a redirect, that it does not opt-in to
// OriginAgentCluster isolation. The setup in this test is subtle, since in
// order for the call to NavigationRequest::OnRequestRedirected() to attempt to
// create a new SiteInstance, we must load the same origin the redirect wants to
// use, and load it without OriginAgentCluster isolation. Prior to the fix for
// https://crbug.com/1329061 the redirect would result in opting the origin into
// OriginAgentCluster isolation since no global walk is present to detect that
// it has already been loaded without.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       RedirectSameSiteWithOACDoesntOptIn) {
  GURL main_frame_url(https_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(foo.com)"));
  GURL redirect_url(https_server()->GetURL("foo.com", "/redirect_me"));
  GURL expected_commit_url(https_server()->GetURL("foo.com", "/title1.html"));
  url::Origin origin(url::Origin::Create(main_frame_url));

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  SetRedirectTarget("/title1.html");
  SetHeaderValue("?1");
  EXPECT_TRUE(NavigateToURLFromRenderer(child_frame_node, redirect_url,
                                        expected_commit_url));
  // This next line verifies that the OriginAgentCluster header sent with the
  // 301 redirect failed to opt foo.com into OriginAgentCluster isolation, as
  // it should. The check will fail if the origin was opted-in to
  // OriginAgentCluster isolation.
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
}

// Same as the preceding test, but the redirect is cross-site.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       RedirectCrossSiteWithOACDoesntOptIn) {
  GURL main_frame_url(https_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(foo.com)"));
  GURL redirect_url(https_server()->GetURL("bar.com", "/redirect_me"));
  GURL expected_commit_url(https_server()->GetURL("foo.com", "/title1.html"));
  url::Origin origin(url::Origin::Create(main_frame_url));

  EXPECT_TRUE(NavigateToURL(shell(), main_frame_url));
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));

  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  SetRedirectTarget(expected_commit_url.spec());
  SetHeaderValue("?1");
  EXPECT_TRUE(NavigateToURLFromRenderer(child_frame_node, redirect_url,
                                        expected_commit_url));

  // This next line verifies that the OriginAgentCluster header sent with the
  // 301 redirect failed to opt foo.com into OriginAgentCluster isolation, as
  // it should. The check will fail if the origin was opted-in to
  // OriginAgentCluster isolation.
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
}

// This tests that header-based opt-in causes the origin to end up in the
// isolated origins list.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest, Basic) {
  base::HistogramTester histograms;
  SetHeaderValue("?1");

  GURL url(https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  url::Origin origin(url::Origin::Create(url));

  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(origin));

  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                               kRequestedAndOriginKeyed),
          1)));
}

// A test to ensure that origins whose host has a trailing dot pass the
// validation checks for explicit opt-ins and opt-outs. This is an
// `OriginKeyedProcessByDefaultTest` test in order that the explicit opt-out
// will be tracked. Note: failure for either part of this test will involve
// crashing on a CHECK in
// ChildProcessSecurityPolicyImpl::AddOriginIsolationStateForBrowsingInstance():
IN_PROC_BROWSER_TEST_F(OriginKeyedProcessByDefaultTest,
                       HostWithTrailingDotAllowed) {
  // Explicit opt-in with a trailing dot.
  SetHeaderValue("?1");
  GURL opt_in_url(https_server()->GetURL("opt-in.foo.com.", "/isolate_origin"));
  url::Origin opt_in_origin(url::Origin::Create(opt_in_url));

  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(opt_in_origin));
  EXPECT_TRUE(NavigateToURL(shell(), opt_in_url));
  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(opt_in_origin));

  // Explicit opt-out with a trailing dot.
  SetHeaderValue("?0");
  GURL opt_out_url(
      https_server()->GetURL("opt-out.foo.com.", "/isolate_origin"));
  url::Origin opt_out_origin(url::Origin::Create(opt_out_url));

  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(opt_out_origin));
  EXPECT_TRUE(NavigateToURL(shell(), opt_out_url));
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(opt_out_origin));
}

// A simple test that, when OAC-by-default is enabled with process-isolation, an
// origin that receives default OAC is put in an origin-keyed process.
IN_PROC_BROWSER_TEST_F(OriginKeyedProcessByDefaultTest,
                       DefaultIsOriginKeyedProcess) {
  GURL test_url(https_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  scoped_refptr<SiteInstanceImpl> site_instance =
      root->current_frame_host()->GetSiteInstance();
  OriginAgentClusterIsolationState isolation_state =
      DetermineOriginAgentClusterIsolation(site_instance.get(), test_url);
  // Even though this request has no OriginAgentCluster header, it should get
  // an origin-keyed process by default.
  EXPECT_TRUE(isolation_state.is_origin_agent_cluster());
  EXPECT_TRUE(isolation_state.requires_origin_keyed_process());
  EXPECT_TRUE(site_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_TRUE(
      site_instance->GetSiteInfo().requires_origin_keyed_process_by_default());
}

// A test to make sure that a renderer-initiated navigation from a default-
// isolated frame to about:blank doesn't crash on a ProcessLock mismatch.
IN_PROC_BROWSER_TEST_F(OriginKeyedProcessByDefaultTest,
                       RendererInitiatedNavigationToAboutBlankSucceeds) {
  GURL test_url(https_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // Verify the main frame got an origin-keyed process by default.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  scoped_refptr<SiteInstanceImpl> site_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(site_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_TRUE(
      site_instance->GetSiteInfo().requires_origin_keyed_process_by_default());

  // Record the origin of the isolated frame.
  std::string initial_origin = EvalJs(shell(), "origin").ExtractString();
  EXPECT_EQ(url::Origin::Create(test_url).GetURL(), GURL(initial_origin));

  // Renderer-initiated navigation to about:blank.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "location.href = 'about:blank';"));
  navigation_observer.Wait();

  // Expect that the about:blank frame inherits the origin of the initiator.
  // Also, this gives us additional verification that the navigation succeeded
  // without hitting the ProcessLock check.
  EXPECT_EQ(initial_origin, EvalJs(shell(), "origin").ExtractString());
  scoped_refptr<SiteInstanceImpl> new_site_instance =
      root->current_frame_host()->GetSiteInstance();
  // Note: the site_instance has changed, due to the proactive BrowsingInstance
  // swap done to make the previous page eligible for back-forward cache.
  // Note: some bots may run this test with BFCache disabled, so we need to
  // handle both cases here.
  if (base::FeatureList::IsEnabled(features::kBackForwardCache)) {
    EXPECT_NE(site_instance, new_site_instance);
  } else {
    EXPECT_EQ(site_instance, new_site_instance);
  }
  EXPECT_EQ(site_instance->GetSiteInfo(), site_instance->GetSiteInfo());
  EXPECT_TRUE(new_site_instance->GetSiteInfo().requires_origin_keyed_process());
}

// A test to make sure that a renderer-initiated navigation from a default-
// isolated frame to about:blank doesn't crash on a ProcessLock mismatch.
// This test is similar to RendererInitiatedNavigationToAboutBlankSucceeds
// but with BFCache disabled.
IN_PROC_BROWSER_TEST_F(
    OriginKeyedProcessByDefaultTest,
    RendererInitiatedNavigationToAboutBlankSucceeds_BFCacheDisabled) {
  DisableBackForwardCacheForTesting(
      web_contents(), BackForwardCacheImpl::TEST_REQUIRES_NO_CACHING);
  GURL test_url(https_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  // Verify the main frame got an origin-keyed process by default.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  scoped_refptr<SiteInstanceImpl> site_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_TRUE(site_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_TRUE(
      site_instance->GetSiteInfo().requires_origin_keyed_process_by_default());

  // Record the origin of the isolated frame.
  std::string initial_origin = EvalJs(shell(), "origin").ExtractString();
  EXPECT_EQ(url::Origin::Create(test_url).GetURL(), GURL(initial_origin));

  // Renderer-initiated navigation to about:blank.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "location.href = 'about:blank';"));
  navigation_observer.Wait();

  // Expect that the about:blank frame inherits the origin of the initiator.
  // Also, this gives us additional verification that the navigation succeeded
  // without hitting the ProcessLock check.
  EXPECT_EQ(initial_origin, EvalJs(shell(), "origin").ExtractString());
  scoped_refptr<SiteInstanceImpl> new_site_instance =
      root->current_frame_host()->GetSiteInstance();
  // Note: with BFCache disabled, the site_instance does not change.
  EXPECT_EQ(site_instance, new_site_instance);
  EXPECT_EQ(site_instance->GetSiteInfo(), site_instance->GetSiteInfo());
  EXPECT_TRUE(new_site_instance->GetSiteInfo().requires_origin_keyed_process());
}

// The same as for DefaultOptInIsIsolated, but testing on a subframe.
IN_PROC_BROWSER_TEST_F(OriginKeyedProcessByDefaultTest,
                       SubframeDefaultIsOriginKeyedProcess) {
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  // Even though this request has no OriginAgentCluster header, it should get
  // an origin-keyed process by default.
  SetHeaderValue("");
  GURL default_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, default_isolated_url));

  scoped_refptr<SiteInstanceImpl> root_site_instance =
      root->current_frame_host()->GetSiteInstance();
  scoped_refptr<SiteInstanceImpl> child_site_instance =
      child_frame_node->current_frame_host()->GetSiteInstance();
  OriginAgentClusterIsolationState child_isolation_state =
      DetermineOriginAgentClusterIsolation(child_site_instance.get(),
                                           default_isolated_url);
  EXPECT_NE(child_site_instance, root_site_instance);
  EXPECT_NE(child_site_instance->GetProcess(),
            root_site_instance->GetProcess());
  EXPECT_TRUE(child_isolation_state.is_origin_agent_cluster());
  EXPECT_TRUE(child_isolation_state.requires_origin_keyed_process());
  EXPECT_EQ(
      root_site_instance->GetIsolationContext().default_isolation_state(),
      child_site_instance->GetIsolationContext().default_isolation_state());
  EXPECT_TRUE(
      child_site_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_TRUE(child_site_instance->GetSiteInfo()
                  .requires_origin_keyed_process_by_default());

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  IsolationContext isolation_context =
      root_site_instance->GetIsolationContext();
  // Verify that we're not explicitly tracking the origin for
  // `default_isolated_url`.
  EXPECT_EQ(static_cast<OriginAgentClusterIsolationState*>(nullptr),
            policy->LookupOriginIsolationStateForTesting(
                isolation_context.browsing_instance_id(),
                url::Origin::Create(default_isolated_url)));

  // Now trigger a global walk by attempting to create a non-isolated version of
  // the same origin.
  GURL attempted_non_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  SetHeaderValue("?0");
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, attempted_non_isolated_url));
  // Now the origin should be explicitly tracked, even though it continues to
  // have the default isolation state as defined for the current
  // BrowsingInstance.
  OriginAgentClusterIsolationState* isolation_state2 =
      policy->LookupOriginIsolationStateForTesting(
          isolation_context.browsing_instance_id(),
          url::Origin::Create(default_isolated_url));
  ASSERT_NE(static_cast<OriginAgentClusterIsolationState*>(nullptr),
            isolation_state2);
  EXPECT_TRUE(isolation_state2->is_origin_agent_cluster());
  EXPECT_TRUE(isolation_state2->requires_origin_keyed_process());
}

// The test ExplicitOptInRespected tests the speculative RFH created before
// receiving the OAC headers. The delay must be set to zero so that the
// speculative RFH is always created before receiving the header.
class OriginKeyedProcessByDefaultTestWithoutSpeculativeRFHDelay
    : public OriginKeyedProcessByDefaultTest {
 public:
  OriginKeyedProcessByDefaultTestWithoutSpeculativeRFHDelay() {
    feature_list_for_defer_speculative_rfh_.InitAndEnableFeatureWithParameters(
        features::kDeferSpeculativeRFHCreation,
        {{"create_speculative_rfh_delay_ms", "0"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_for_defer_speculative_rfh_;
};

// Using origin-keyed processes by default faces a challenge for speculative
// RenderFrameHosts, which are created before any OAC headers arrive.
// Note: the origin is tracked even though the SiteInfo still says it is an
// origin-keyed process by default.
IN_PROC_BROWSER_TEST_F(
    OriginKeyedProcessByDefaultTestWithoutSpeculativeRFHDelay,
    ExplicitOptInRespected) {
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  // This request explicitly opts-in to OAC, and should get process isolation.
  // Note the use of the "isolate_origin" relative path below to force
  // processing of the (non-empty) OAC header.
  SetHeaderValue("?1");
  GURL explicit_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, explicit_isolated_url));

  auto* site_instance =
      child_frame_node->current_frame_host()->GetSiteInstance();
  OriginAgentClusterIsolationState isolation_state =
      DetermineOriginAgentClusterIsolation(site_instance,
                                           explicit_isolated_url);
  EXPECT_TRUE(isolation_state.is_origin_agent_cluster());
  EXPECT_TRUE(isolation_state.requires_origin_keyed_process());
  EXPECT_TRUE(site_instance->GetSiteInfo().requires_origin_keyed_process());
  // In this scenario, the explicit opt-in ends up using a SiteInstance that was
  // created for the speculative RFH, and with requires_origin_keyed_process on
  // by default. Since we don't want to alter the underlying SiteInfo after
  // it's been used to create a ProcessLock, we leave this case as "by_default"
  // in the SiteInfo since the isolation behavior is the same.
  //
  // Note that if the speculative RFH had been created after a previous instance
  // of the origin had been explicitly opted-in, then
  // `requires_origin_keyed_process_by_default()` would return false in that
  // case. This can happen in a cross-origin redirect from A to B, where B has
  // an opt-in header. We would create a speculative RFH for A, throw it away
  // when the redirect happens, and wait to create the RFH for B until headers
  // have arrived.
  EXPECT_TRUE(
      site_instance->GetSiteInfo().requires_origin_keyed_process_by_default());

  // Verify the explicit opt-in is being tracked.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  IsolationContext isolation_context =
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext();
  OriginAgentClusterIsolationState* isolation_state2 =
      policy->LookupOriginIsolationStateForTesting(
          isolation_context.browsing_instance_id(),
          url::Origin::Create(explicit_isolated_url));
  ASSERT_NE(static_cast<OriginAgentClusterIsolationState*>(nullptr),
            isolation_state2);
  EXPECT_TRUE(isolation_state2->is_origin_agent_cluster());
  EXPECT_TRUE(isolation_state2->requires_origin_keyed_process());
}

// Process-isolated OAC-by-default should not process isolate a navigation that
// explicitly opts-out. This test is important, in part, for making sure all the
// CanAccessDataForOrigin checks encountered during the navigation pass.
IN_PROC_BROWSER_TEST_F(OriginKeyedProcessByDefaultTest,
                       ExplicitOptOutRespected) {
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  // This request explicitly opts-out of OAC, and should not get process
  // isolation. Note the use of the "isolate_origin" relative path below to
  // force processing of the (non-empty) OAC header.
  SetHeaderValue("?0");
  GURL default_not_isolated_url(
      https_server()->GetURL("not-isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, default_not_isolated_url));

  auto* child_site_instance =
      child_frame_node->current_frame_host()->GetSiteInstance();
  // The child should be in a separate process from the main frame despite
  // opting-out, because the child is now in a site-keyed process while the
  // main frame is in an origin-keyed process (as verified below).
  EXPECT_NE(child_site_instance->GetProcess(),
            root->current_frame_host()->GetSiteInstance()->GetProcess());

  OriginAgentClusterIsolationState isolation_state =
      DetermineOriginAgentClusterIsolation(child_site_instance,
                                           default_not_isolated_url);
  EXPECT_FALSE(isolation_state.is_origin_agent_cluster());
  EXPECT_FALSE(isolation_state.requires_origin_keyed_process());
  EXPECT_FALSE(
      child_site_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_FALSE(child_site_instance->GetSiteInfo()
                   .requires_origin_keyed_process_by_default());

  // Verify the explicit opt-out is being tracked.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  IsolationContext isolation_context =
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext();
  OriginAgentClusterIsolationState* isolation_state2 =
      policy->LookupOriginIsolationStateForTesting(
          isolation_context.browsing_instance_id(),
          url::Origin::Create(default_not_isolated_url));
  ASSERT_NE(static_cast<OriginAgentClusterIsolationState*>(nullptr),
            isolation_state2);
  EXPECT_FALSE(isolation_state2->is_origin_agent_cluster());
  EXPECT_FALSE(isolation_state2->requires_origin_keyed_process());
}

namespace {

void TestDefaultIsolationForFrame(
    FrameTreeNode* ftn,
    const GURL& default_isolated_url,
    bool expect_origin_agent_cluster,
    bool expect_requires_origin_keyed_process,
    bool expect_default_requires_origin_keyed_process) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  auto* site_instance = ftn->current_frame_host()->GetSiteInstance();
  OriginAgentClusterIsolationState isolation_state =
      IsolatedOriginTestBase::DetermineOriginAgentClusterIsolation(
          site_instance, default_isolated_url);
  const SiteInfo& site_info = site_instance->GetSiteInfo();
  EXPECT_EQ(expect_origin_agent_cluster,
            isolation_state.is_origin_agent_cluster());
  EXPECT_EQ(expect_requires_origin_keyed_process,
            isolation_state.requires_origin_keyed_process());
  EXPECT_EQ(expect_requires_origin_keyed_process,
            site_info.requires_origin_keyed_process());
  EXPECT_EQ(expect_default_requires_origin_keyed_process,
            site_info.requires_origin_keyed_process_by_default());

  // Verify that we're not explicitly tracking the origin we isolated by
  // default.
  IsolationContext isolation_context = site_instance->GetIsolationContext();
  EXPECT_EQ(static_cast<OriginAgentClusterIsolationState*>(nullptr),
            policy->LookupOriginIsolationStateForTesting(
                isolation_context.browsing_instance_id(),
                url::Origin::Create(default_isolated_url)));
}

}  // namespace

// This test verifies that locking the definition of default isolation to
// individual BrowsingInstances works correctly when the underlying feature
// is changed dynamically.
IN_PROC_BROWSER_TEST_F(OriginKeyedProcessByDefaultTest,
                       DynamicEnterprisePolicyChange) {
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  SetHeaderValue("");
  GURL default_isolated_url(
      https_server()->GetURL("isolated.foo.com", "/title1.html"));

  // Setup first BrowsingInstance. This one will have default isolation with
  // origin_agent_cluster and requests_origin_keyed_process (by default) true.
  Shell* shell1 = shell();
  auto* web_contents1 = static_cast<WebContentsImpl*>(shell1->web_contents());

  EXPECT_TRUE(NavigateToURL(shell1, test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(web_contents1).size());
  FrameTreeNode* root1 = web_contents1->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node1 = root1->child_at(0);

  // Load a frame into the first BrowsingInstance.
  // Even though this request has no OriginAgentCluster header, it should get
  // process-isolated OAC by default.
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node1, default_isolated_url));
  EXPECT_NE(
      child_frame_node1->current_frame_host()->GetSiteInstance()->GetProcess(),
      root1->current_frame_host()->GetSiteInstance()->GetProcess());

  {
    SCOPED_TRACE("child_frame_node1");
    TestDefaultIsolationForFrame(
        child_frame_node1, default_isolated_url,
        /*expect_origin_agent_cluster=*/true,
        /*expect_requires_origin_keyed_process=*/true,
        /*expect_default_requires_origin_keyed_process=*/true);
  }

  // Dynamically disable the feature to simulate the enterprise policy being
  // dynamically changed.
  browser_client_->SetShouldDisableOriginAgentClusterDefault(true);

  // Create a second BrowsingInstance. This one will have default isolation with
  // origin_agent_cluster = false and requests_origin_keyed_process (by default)
  // false.
  Shell* shell2 = CreateBrowser();
  auto* web_contents2 = static_cast<WebContentsImpl*>(shell2->web_contents());

  // Load a frame into the second BrowsingInstance.
  // This request also has no OriginAgentCluster header, but it should not get
  // OAC by default, nor request process-isolation.
  EXPECT_TRUE(NavigateToURL(shell2, test_url));
  FrameTreeNode* root2 = web_contents2->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node2 = root2->child_at(0);
  // We navigate to `default_isolated_url` again so that we're using the same
  // urls in both parts of the test, but we don't expect it to be isolated in
  // this BrowsingInstance.
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node2, default_isolated_url));
  EXPECT_EQ(
      child_frame_node2->current_frame_host()->GetSiteInstance()->GetProcess(),
      root2->current_frame_host()->GetSiteInstance()->GetProcess());

  {
    SCOPED_TRACE("child_frame_node2");
    TestDefaultIsolationForFrame(
        child_frame_node2, default_isolated_url,
        /*expect_origin_agent_cluster=*/false,
        /*expect_requires_origin_keyed_process=*/false,
        /*expect_default_requires_origin_keyed_process=*/false);
  }

  // We expect the default isolation to be different in the two
  // BrowsingInstances.
  EXPECT_NE(root1->current_frame_host()
                ->GetSiteInstance()
                ->GetIsolationContext()
                .default_isolation_state(),
            root2->current_frame_host()
                ->GetSiteInstance()
                ->GetIsolationContext()
                .default_isolation_state());

  // Another navigation in root1 should respect the origin-keyed default used by
  // that BrowsingInstance and not the current site-keyed default.
  GURL default_isolated_url2(
      https_server()->GetURL("isolated.bar.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell1, test_url));
  {
    SCOPED_TRACE("root1");
    TestDefaultIsolationForFrame(
        root1, default_isolated_url2,
        /*expect_origin_agent_cluster=*/true,
        /*expect_requires_origin_keyed_process=*/true,
        /*expect_default_requires_origin_keyed_process=*/true);
  }
}

IN_PROC_BROWSER_TEST_F(OriginIsolationDefaultOACTest, Basic) {
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  // We must load the origins to be isolated (or not) into a child frame so that
  // they all stay in the same BrowsingInstance, since the test relies on
  // knowing isolation history for the OriginAgentClusterEndResult::*But* cases.
  // In this test, the convention is:
  //   foo.com is (implicitly) isolated,
  //   isolated.foo.com is (explicitly) isolated,
  //   isolated.bar.com is (implicitly) isolated, and
  //   bar.com is (explicitly) not isolated.
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  // The first three scenarios should all get the isolation status they request
  // (the "And" cases).
  {
    // Explicitly request OriginAgentCluster via the header.
    SetHeaderValue("?1");
    base::HistogramTester histograms;
    GURL isolated_suborigin_url(
        https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(child_frame_node, isolated_suborigin_url));
    auto* site_instance =
        child_frame_node->current_frame_host()->GetSiteInstance();
    EXPECT_TRUE(DetermineOriginAgentClusterIsolation(site_instance,
                                                     isolated_suborigin_url)
                    .requires_origin_keyed_process());

    EXPECT_THAT(
        histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                 kExplicitlyRequestedAndOriginKeyed),
            1)));
  }
  {
    // Even though this request has no OriginAgentCluster header, it should get
    // OAC by default.
    SetHeaderValue("");
    base::HistogramTester histograms;
    GURL default_isolated_url(
        https_server()->GetURL("isolated.bar.com", "/title1.html"));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(child_frame_node, default_isolated_url));

    auto* site_instance =
        child_frame_node->current_frame_host()->GetSiteInstance();
    OriginAgentClusterIsolationState isolation_state =
        DetermineOriginAgentClusterIsolation(site_instance,
                                             default_isolated_url);
    // TODO(wjmaclean): If OriginAgentCluster-by-default transitions to using
    // process-isolation at some future date, the second expectation below will
    // need to change to EXPECT_TRUE.
    EXPECT_TRUE(isolation_state.is_origin_agent_cluster());
    EXPECT_FALSE(isolation_state.requires_origin_keyed_process());

    EXPECT_THAT(
        histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                 kNotExplicitlyRequestedAndOriginKeyed),
            1)));
    // Ensure that the implicit case did not do a global walk (which would be
    // inefficient), by noticing that a hypothetical request for non-isolation
    // of that origin in the same SiteInstance would succeed. That can only
    // happen if the implicit case was not recorded in the BrowsingInstance.
    OriginAgentClusterIsolationState hypothetical_isolation_request =
        OriginAgentClusterIsolationState::CreateNonIsolated();
    OriginAgentClusterIsolationState hypothetical_isolation_state =
        ChildProcessSecurityPolicyImpl::GetInstance()
            ->DetermineOriginAgentClusterIsolation(
                site_instance->GetIsolationContext(),
                url::Origin::Create(default_isolated_url),
                hypothetical_isolation_request);
    EXPECT_FALSE(hypothetical_isolation_state.is_origin_agent_cluster());
  }
  {
    // The "isolate_origin" path in the url will force the test framework to
    // include the OriginAgentCluster header. Here we explicitly request not to
    // have OAC.
    SetHeaderValue("?0");
    base::HistogramTester histograms;
    GURL explicit_non_isolated_url(
        https_server()->GetURL("bar.com", "/isolate_origin"));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(child_frame_node, explicit_non_isolated_url));
    auto* site_instance =
        child_frame_node->current_frame_host()->GetSiteInstance();

    EXPECT_FALSE(DetermineOriginAgentClusterIsolation(site_instance,
                                                      explicit_non_isolated_url)
                     .is_origin_agent_cluster());

    EXPECT_THAT(
        histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                 kExplicitlyNotRequestedAndNotOriginKeyed),
            1)));
  }

  // The next three cases should all fail to get the isolation status they
  // request (the "But" cases). In these cases, URLs from origins we have
  // already visited in the BrowsingInstance return different OAC header values,
  // but are forced to stick with their earlier value rather than the newly
  // requested value.
  {
    // Even though the lack of a header would normally lead to default OAC
    // isolation, the previous explicitly non-isolated visit to this origin
    // means that this origin will remain not origin keyed.
    SetHeaderValue("");
    base::HistogramTester histograms;
    GURL url(https_server()->GetURL("bar.com", "/title1.html"));
    EXPECT_TRUE(NavigateToURLFromRenderer(child_frame_node, url));
    auto* site_instance =
        child_frame_node->current_frame_host()->GetSiteInstance();

    EXPECT_FALSE(DetermineOriginAgentClusterIsolation(site_instance, url)
                     .is_origin_agent_cluster());

    EXPECT_THAT(
        histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                 kNotExplicitlyRequestedButNotOriginKeyed),
            1)));
  }

  {
    // An explicit opt-out for isolated.bar.com should not be granted given the
    // previous default-opt-in above.
    SetHeaderValue("?0");
    base::HistogramTester histograms;
    GURL explicit_non_isolated_url(
        https_server()->GetURL("isolated.bar.com", "/isolate_origin"));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(child_frame_node, explicit_non_isolated_url));
    auto* site_instance =
        child_frame_node->current_frame_host()->GetSiteInstance();

    OriginAgentClusterIsolationState isolation_state =
        DetermineOriginAgentClusterIsolation(site_instance,
                                             explicit_non_isolated_url);
    EXPECT_TRUE(isolation_state.is_origin_agent_cluster());
    EXPECT_FALSE(isolation_state.requires_origin_keyed_process());

    EXPECT_THAT(
        histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                 kExplicitlyNotRequestedButOriginKeyed),
            1)));
  }
  {
    // Verify that we don't explicitly opt-in an origin that was explicitly
    // opted-out.
    SetHeaderValue("?1");
    base::HistogramTester histograms;
    GURL explicit_isolated_url(
        https_server()->GetURL("bar.com", "/isolate_origin"));
    EXPECT_TRUE(
        NavigateToURLFromRenderer(child_frame_node, explicit_isolated_url));
    auto* site_instance =
        child_frame_node->current_frame_host()->GetSiteInstance();

    EXPECT_FALSE(DetermineOriginAgentClusterIsolation(site_instance,
                                                      explicit_isolated_url)
                     .is_origin_agent_cluster());

    EXPECT_THAT(
        histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
        testing::ElementsAre(base::Bucket(
            static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                 kExplicitlyRequestedButNotOriginKeyed),
            1)));
  }
}

// These tests ensure that non-HTTPS secure contexts (see
// https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy) are
// able to use origin isolation.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHttpServerHeaderTest, Localhost) {
  GURL url(embedded_test_server()->GetURL("localhost", "/"));
  url::Origin origin(url::Origin::Create(url));

  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(origin));
}

IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHttpServerHeaderTest, DotLocalhost) {
  GURL url(embedded_test_server()->GetURL("test.localhost", "/"));
  url::Origin origin(url::Origin::Create(url));

  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(origin));
}

IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHttpServerHeaderTest,
                       OneTwentySeven) {
  GURL url(embedded_test_server()->GetURL("127.0.0.1", "/"));
  url::Origin origin(url::Origin::Create(url));

  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_TRUE(ShouldOriginGetOptInProcessIsolation(origin));
}

// Two tests for basic OAC operation w.r.t. prerendering FrameTrees.

// Basic test to make sure an origin opting-in in a primary FrameTree
// triggers registration of a non-opting-origin in an existing prerendering
// Frametree.
IN_PROC_BROWSER_TEST_F(OriginIsolationPrerenderOptInHeaderTest,
                       SimplePrerenderSubOriginIsolationTest) {
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  // Navigate primary tab to a non-isolated origin.
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);

  // Create prerender tab, load non-isolated "a.foo.com".
  Shell* prerender_tab = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(prerender_tab, GURL(https_server()->GetURL(
                                               "a.foo.com", "/title1.html"))));
  auto* prerender_web_contents =
      static_cast<WebContentsImpl*>(prerender_tab->web_contents());
  set_prerender_web_contents(prerender_web_contents);
  GURL non_isolated_origin_url(
      https_server()->GetURL("a.foo.com", "/title2.html"));

  FrameTreeNodeId host_id =
      prerender_helper_.AddPrerender(non_isolated_origin_url);

  // In primary tab, navigate to an isolated origin.
  SetHeaderValue("?1");
  GURL isolated_suborigin_url(
      https_server()->GetURL("a.foo.com", "/isolate_origin"));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, isolated_suborigin_url));
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child_frame_node->current_frame_host()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  EXPECT_TRUE(child_frame_node->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .requires_origin_keyed_process());

  // Verify in prerender tab that "a.foo.com" is registered as a non-isolated
  // origin. We must get the SiteInstance() to test from the
  // PrerenderedMainFrameHost() to make sure the opt-out registration has
  // propagated to the right place.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  auto* prerender_site_instance_impl = static_cast<SiteInstanceImpl*>(
      prerender_helper_.GetPrerenderedMainFrameHost(host_id)
          ->GetSiteInstance());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       prerender_site_instance_impl->GetIsolationContext(),
                       url::Origin::Create(non_isolated_origin_url),
                       MakeOACIsolationState(true))
                   .requires_origin_keyed_process());

  // Activate the prerendered page and confirm the non-isolated origin remains
  // non-isolated.
  prerender_helper_.NavigatePrimaryPage(non_isolated_origin_url);
  auto* new_prerender_site_instance_impl = static_cast<SiteInstanceImpl*>(
      prerender_tab->web_contents()->GetSiteInstance());
  EXPECT_EQ(prerender_site_instance_impl, new_prerender_site_instance_impl);
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       new_prerender_site_instance_impl->GetIsolationContext(),
                       url::Origin::Create(non_isolated_origin_url),
                       MakeOACIsolationState(true))
                   .requires_origin_keyed_process());
  EXPECT_FALSE(new_prerender_site_instance_impl->GetSiteInfo()
                   .requires_origin_keyed_process());
  EXPECT_TRUE(new_prerender_site_instance_impl->GetSiteURL() ==
                  GURL("https://foo.com") ||
              new_prerender_site_instance_impl->IsDefaultSiteInstance());
}

// Basic test to make sure an origin opting-in in a prerendering FrameTree
// triggers registration of a non-opting-origin in an existing primary
// Frametree.
IN_PROC_BROWSER_TEST_F(OriginIsolationPrerenderOptInHeaderTest,
                       SimplePrerenderSubOriginIsolationTest2) {
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  // Navigate child frame to a non-isolated origin "a.foo.com".
  GURL non_isolated_suborigin_url(
      https_server()->GetURL("a.foo.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, non_isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());

  // Create prerender tab, load isolated "a.foo.com".
  Shell* prerender_tab = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(prerender_tab, GURL(https_server()->GetURL(
                                               "a.foo.com", "/title1.html"))));
  auto* prerender_web_contents =
      static_cast<WebContentsImpl*>(prerender_tab->web_contents());
  set_prerender_web_contents(prerender_web_contents);
  SetHeaderValue("?1");
  GURL isolated_origin_url(
      https_server()->GetURL("a.foo.com", "/isolate_origin"));

  FrameTreeNodeId host_id = prerender_helper_.AddPrerender(isolated_origin_url);

  // Verify origin is isolated in the prerender IsolationContext.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  auto* prerender_site_instance_impl = static_cast<SiteInstanceImpl*>(
      prerender_helper_.GetPrerenderedMainFrameHost(host_id)
          ->GetSiteInstance());
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      prerender_site_instance_impl->GetIsolationContext(),
                      url::Origin::Create(isolated_origin_url),
                      MakeOACIsolationState(false))
                  .requires_origin_keyed_process());
  EXPECT_TRUE(prerender_site_instance_impl->RequiresDedicatedProcess());
  EXPECT_TRUE(prerender_site_instance_impl->GetSiteInfo()
                  .requires_origin_keyed_process());

  // Verify in original tab that "a.foo.com" is now registered as a non-isolated
  // origin.
  auto* primary_site_instance_impl = static_cast<SiteInstanceImpl*>(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       primary_site_instance_impl->GetIsolationContext(),
                       url::Origin::Create(isolated_origin_url),
                       MakeOACIsolationState(true))
                   .requires_origin_keyed_process());

  // Activate the prerendered page and confirm the isolated origin remains
  // isolated.
  prerender_helper_.NavigatePrimaryPage(isolated_origin_url);
  auto* new_prerender_site_instance_impl = static_cast<SiteInstanceImpl*>(
      prerender_tab->web_contents()->GetSiteInstance());
  EXPECT_EQ(prerender_site_instance_impl, new_prerender_site_instance_impl);
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      new_prerender_site_instance_impl->GetIsolationContext(),
                      url::Origin::Create(isolated_origin_url),
                      MakeOACIsolationState(false))
                  .requires_origin_keyed_process());
  EXPECT_TRUE(prerender_site_instance_impl->RequiresDedicatedProcess());
  EXPECT_TRUE(new_prerender_site_instance_impl->GetSiteInfo()
                  .requires_origin_keyed_process());
}

// Further tests deep-dive into various scenarios for the isolation opt-ins.

// In this test the sub-origin is isolated because the header requests it. It
// will have a different site instance than the main frame.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       SimpleSubOriginIsolationTest) {
  base::HistogramTester histograms;
  SetHeaderValue("?1");
  // Start off with an a(a) page, then navigate the subframe to an isolated sub
  // origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  GURL origin_url = url::Origin::Create(isolated_suborigin_url).GetURL();
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  auto expected_isolated_suborigin_lock = ProcessLock::FromSiteInfo(SiteInfo(
      /*site_url=*/origin_url,
      /*process_lock_url=*/origin_url,
      /*requires_origin_keyed_process=*/true,
      /*requires_origin_keyed_process_by_default=*/false,
      /*is_sandboxed=*/false, UrlInfo::kInvalidUniqueSandboxId,
      StoragePartitionConfig::CreateDefault(browser_context),
      WebExposedIsolationInfo::CreateNonIsolated(),
      WebExposedIsolationLevel::kNotIsolated, /*is_guest=*/false,
      /*does_site_request_dedicated_process_for_coop=*/false,
      /*is_jit_disabled=*/false, /*are_v8_optimizations_disabled=*/false,
      /*is_pdf=*/false, /*is_fenced=*/false,
      /*cross_origin_isolation_key=*/std::nullopt));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, isolated_suborigin_url));
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
            ProcessLock::FromSiteInfo(child_frame_node->current_frame_host()
                                          ->GetSiteInstance()
                                          ->GetSiteInfo()));
  EXPECT_EQ(
      ProcessLock::FromSiteInfo(child_frame_node->current_frame_host()
                                    ->GetSiteInstance()
                                    ->GetSiteInfo()),
      child_frame_node->current_frame_host()->GetProcess()->GetProcessLock());

  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kNotRequestedAndNotOriginKeyed),
              2),
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kRequestedAndOriginKeyed),
              1)));
}

// Check that two same-site Origin-Agent-Cluster subframes in unrelated windows
// obey the subframe process reuse policy.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       OriginAgentClusterProcessReuse) {
  SetHeaderValue("?1");
  // Start off with an a(a) page, then navigate the subframe to an isolated
  // suborigin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(child, isolated_suborigin_url));
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .requires_origin_keyed_process());

  // Open an unrelated window and set up the same frame hierarchy there.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, test_url));
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  FrameTreeNode* new_child = new_root->child_at(0);
  EXPECT_TRUE(NavigateToURLFromRenderer(new_child, isolated_suborigin_url));
  EXPECT_NE(new_root->current_frame_host()->GetSiteInstance(),
            new_child->current_frame_host()->GetSiteInstance());

  // Even though the two subframes should be in different BrowsingInstances,
  // they should share the same process due to the subframe process reuse
  // policy.
  EXPECT_FALSE(
      child->current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
          new_child->current_frame_host()->GetSiteInstance()));
  EXPECT_EQ(child->current_frame_host()->GetProcess(),
            new_child->current_frame_host()->GetProcess());
}

// In this test the sub-origin is isolated because the header requests it. It
// will have the same site instance as the main frame, and it will be in the
// same process.
IN_PROC_BROWSER_TEST_F(SameProcessOriginIsolationOptInHeaderTest,
                       SimpleSubOriginIsolationTest) {
  base::HistogramTester histograms;
  SetHeaderValue("?1");
  // Start off with an a(a) page, then navigate the subframe to an isolated sub
  // origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  GURL origin_url = url::Origin::Create(isolated_suborigin_url).GetURL();
  EXPECT_FALSE(
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled());
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());
  EXPECT_FALSE(child_frame_node->current_frame_host()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      root->current_frame_host()
                          ->GetSiteInstance()
                          ->GetIsolationContext(),
                      url::Origin::Create(isolated_suborigin_url),
                      MakeOACIsolationState(false))
                  .is_origin_agent_cluster());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       root->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(isolated_suborigin_url),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      web_contents()->GetBrowserContext(),
      url::Origin::Create(isolated_suborigin_url)));

  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kNotRequestedAndNotOriginKeyed),
              2),
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kRequestedAndOriginKeyed),
              1)));
}

// This test is *nearly* the same as SameProcessOriginIsolationOptInHeaderTest.
// SimpleSubOriginIsolationTest, but here we have command-line isolated foo.com
// so it will be in a site instance with a non-empty ProcessLock. But the
// same-process OAC isolated.foo.com will still be in the same SiteInstance,
// and checks on the expected ProcessLock for isolated.foo.com should pass,
// i.e. it should be the same as for the foo.com process.
IN_PROC_BROWSER_TEST_F(
    SameProcessOriginIsolationOptInHeaderWithIsolatedOriginTest,
    SimpleSubOriginIsolationTest) {
  base::HistogramTester histograms;
  SetHeaderValue("?1");
  // Start off with a foo(foo) page, then navigate the subframe to an isolated
  // sub origin. foo.com is isolated from the command line.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  GURL origin_url = url::Origin::Create(isolated_suborigin_url).GetURL();
  EXPECT_FALSE(
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled());
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(root->current_frame_host()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  EXPECT_TRUE(child_frame_node->current_frame_host()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  ProcessLock root_process_lock = ProcessLock::FromSiteInfo(
      root->current_frame_host()->GetSiteInstance()->GetSiteInfo());
  EXPECT_TRUE(root_process_lock.is_locked_to_site());
  EXPECT_EQ(root_process_lock.lock_url(), GURL("https://foo.com/"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      root->current_frame_host()
                          ->GetSiteInstance()
                          ->GetIsolationContext(),
                      url::Origin::Create(isolated_suborigin_url),
                      MakeOACIsolationState(false))
                  .is_origin_agent_cluster());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       root->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(isolated_suborigin_url),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      web_contents()->GetBrowserContext(),
      url::Origin::Create(isolated_suborigin_url)));

  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kNotRequestedAndNotOriginKeyed),
              2),
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kRequestedAndOriginKeyed),
              1)));
}

// Verify OAC is calculated using the base URL when using LoadDataWithBaseURL()
// (analogous to Android WebView's loadDataWithBaseURL()) when the actual site
// does not specify an Origin-Agent-Cluster value.
IN_PROC_BROWSER_TEST_F(SameProcessOriginIsolationOptInHeaderTest,
                       LoadDataWithBaseURLNoOAC) {
  const GURL test_url = https_server()->GetURL("foo.com", "/title1.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadDataWithBaseURL(
      test_url, "<!DOCTYPE html><html><body></body></html>", test_url);
  navigation_observer.Wait();

  // Even though this internally navigates to a data: URL (which would imply
  // `window.originAgentCluster === true`, the base URL should be used for the
  // OAC calculation.
  EXPECT_EQ(false, EvalJs(shell(), "window.originAgentCluster"));
  EXPECT_TRUE(ExecJs(
      shell(), "document.body.appendChild(document.createElement('iframe'))"));

  EXPECT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0), test_url));
  EXPECT_EQ(false,
            EvalJs(ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
                   "window.originAgentCluster"));

  // If OAC is incorrectly calculated for `LoadDataWithBaseURL()`, this will
  // fail the access checks in Blink because the two browsing contexts will be
  // treated as cross-origin.
  EXPECT_EQ("This page has no title.\n\n",
            EvalJs(shell(), "window[0].document.body.textContent"));
}

// Verify OAC is calculated using the base URL when using LoadDataWithBaseURL()
// (analogous to Android WebView's loadDataWithBaseURL()). Unlike the previous
// test, the actual site specifies an Origin-Agent-Cluster value, which should
// be ignored.
IN_PROC_BROWSER_TEST_F(SameProcessOriginIsolationOptInHeaderTest,
                       LoadDataWithBaseURLWithOAC) {
  const GURL test_url = https_server()->GetURL("foo.com", "/isolate_origin");
  SetHeaderValue("?1");

  // `tab2` and `shell()` will be in separate browsing instances. As an
  // optimization, browsing instances only track OAC consistency if an origin
  // has ever sent OAC headers. Once an origin has sent OAC headers, this is
  // tracked globally.
  //
  // This navigation marks "foo.com" as having sent OAC headers. This is
  // important to validate that `LoadDataWithBaseURL()` uses the origin
  // calculated from the base URL to update the non-isolated origin list in
  // `shell()`'s browsing instance. If this is not done correctly, then loading
  // "foo.com/isolate_origin" in the subframe will incorrectly use OAC in the
  // subframe, which will be inconsistent with the main frame loaded via
  // `LoadDataWithBaseURL()`.
  Shell* tab2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(tab2, test_url));

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadDataWithBaseURL(
      test_url, "<!DOCTYPE html><html><body></body></html>", test_url);
  navigation_observer.Wait();

  // Even though this internally navigates to a data: URL (which would imply
  // `window.originAgentCluster === true`, the base URL should be used for the
  // OAC calculation.
  EXPECT_EQ(false, EvalJs(shell(), "window.originAgentCluster"));
  EXPECT_TRUE(ExecJs(
      shell(), "document.body.appendChild(document.createElement('iframe'))"));

  // Even though this navigation sets the OAC header value, it should be
  // ignored, since the SiteInstance for foo.com is already site-keyed.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0), test_url));
  EXPECT_EQ(false,
            EvalJs(ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0),
                   "window.originAgentCluster"));

  // The two frames should be same-origin to each other, since the OAC header
  // value should be ignored.
  EXPECT_EQ("isolate me!",
            EvalJs(shell(), "window[0].document.body.textContent"));
}

// This test checks that same-process OriginAgentCluster won't crash and will
// apply properly when used on a localhost URL.  See https://crbug.com/1276155.
IN_PROC_BROWSER_TEST_F(SameProcessOriginIsolationOptInHeaderTest, Localhost) {
  SetHeaderValue("?1");
  GURL url(https_server()->GetURL("localhost", "/isolate_origin"));
  url::Origin origin(url::Origin::Create(url));

  EXPECT_TRUE(SiteIsolationPolicy::IsOriginAgentClusterEnabled());
  EXPECT_FALSE(
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled());
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));

  EXPECT_TRUE(NavigateToURL(shell(), url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_FALSE(root->current_frame_host()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());
  EXPECT_FALSE(ShouldOriginGetOptInProcessIsolation(origin));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  auto isolation_result = policy->DetermineOriginAgentClusterIsolation(
      root->current_frame_host()->GetSiteInstance()->GetIsolationContext(),
      origin, MakeOACIsolationState(false));
  EXPECT_TRUE(isolation_result.is_origin_agent_cluster());
  EXPECT_FALSE(isolation_result.requires_origin_keyed_process());
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      web_contents()->GetBrowserContext(), origin));
}

// This test verifies that --disable-web-security overrides same-process
// OriginAgentCluster (i.e. disables it).
IN_PROC_BROWSER_TEST_F(SameProcessNoWebSecurityOriginIsolationOptInHeaderTest,
                       DisableWebSecurityDisablesOriginAgentCluster) {
  // Make sure we request the header for OriginAgentCluster for the child; the
  // fact that this test uses --disable-web-security will override the header.
  SetHeaderValue("?1");
  GURL main_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, isolated_suborigin_url));

  // Web security is disabled so everything should be same-origin and
  // accessible across browsing contexts.
  EXPECT_EQ(false, EvalJs(child_frame_node, "window.originAgentCluster"));

  std::string parent_body_content =
      EvalJs(root, "document.body.textContent").ExtractString();
  // Make sure that the child frame doesn't think it's isolated.
  EXPECT_EQ(parent_body_content,
            EvalJs(child_frame_node, "window.parent.document.body.textContent")
                .ExtractString());
}

// In this test the sub-origin isn't isolated because no header is set. It will
// have the same site instance as the main frame.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       SimpleSubOriginNonIsolationTest) {
  base::HistogramTester histograms;
  // Start off with an a(a) page, then navigate the subframe to an isolated sub
  // origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node = root->child_at(0);
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node, isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node->current_frame_host()->GetSiteInstance());
  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(base::Bucket(
          static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                               kNotRequestedAndNotOriginKeyed),
          3)));
}

// This test verifies that renderer-initiated navigations to/from isolated
// sub-origins works as expected.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       RendererInitiatedNavigations) {
  SetHeaderValue("?1");
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  GURL isolated_sub_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  {
    // Navigate the child to an isolated origin.
    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecJs(
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
    EXPECT_TRUE(ExecJs(child, "location.href = '" +
                                  non_isolated_sub_origin_url.spec() + "';"));
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
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest, MainFrameNavigation) {
  SetHeaderValue("?1");
  GURL unisolated_url(https_server()->GetURL("www.foo.com", "/title1.html"));
  GURL isolated_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));

  EXPECT_TRUE(NavigateToURL(shell(), unisolated_url));

  // Open a same-site popup to keep the www.foo.com process alive.
  Shell* popup = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  SiteInstance* unisolated_instance =
      popup->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  RenderProcessHost* unisolated_process =
      popup->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // Go to isolated.foo.com with a renderer-initiated navigation.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), isolated_url));
  scoped_refptr<SiteInstance> isolated_instance =
      web_contents()->GetSiteInstance();
  RenderProcessHost* isolated_process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();

  EXPECT_NE(unisolated_instance, isolated_instance);
  EXPECT_NE(unisolated_process, isolated_process);

  // The site URL for isolated.foo.com should be the full origin rather than
  // scheme and eTLD+1.
  EXPECT_EQ(https_server()->GetURL("isolated.foo.com", "/"),
            isolated_instance->GetSiteURL());

  // Now use a renderer-initiated navigation to go to an unisolated origin,
  // www.foo.com. This should end up back in the `popup`'s process.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), unisolated_url));
  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process,
            web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Now, perform a browser-initiated navigation to an isolated origin and
  // ensure that this ends up in a new process and SiteInstance for
  // isolated.foo.com.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  scoped_refptr<SiteInstance> isolated_instance2 =
      web_contents()->GetSiteInstance();
  RenderProcessHost* isolated_process2 =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
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
  EXPECT_EQ(unisolated_process,
            web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Go back again.  This should go to isolated.foo.com in an isolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process,
            web_contents()->GetPrimaryMainFrame()->GetProcess());

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
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       OriginIsolationStateRetainedForBrowsingInstance) {
  base::HistogramTester histograms;
  SetHeaderValue("?1");
  // Start off with an a(a,a) page, then navigate the subframe to an isolated
  // sub origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com, foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(3u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);
  FrameTreeNode* child_frame_node1 = root->child_at(1);

  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node0, isolated_suborigin_url));
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change the server's responses to stop isolating the sub-origin. It should
  // still be isolated, to remain consistent with the other frame.
  SetHeaderValue("?0");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "The page did not request an origin-keyed agent cluster, but was put in "
      "one anyway*");

  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node1, isolated_suborigin_url));

  ASSERT_TRUE(console_observer.Wait());

  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());

  // The two sub-frames should be in the same site instance.
  EXPECT_EQ(child_frame_node0->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());

  // Make sure the master opt-in list still has the origin tracked.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      web_contents()->GetBrowserContext(),
      url::Origin::Create(isolated_suborigin_url)));

  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(
          // Original loads of a(a,a) go here.
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kNotRequestedAndNotOriginKeyed),
              3),
          // Second isolated subframe load goes here.
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kNotRequestedButOriginKeyed),
              1),
          // First isolated subframe load goes here.
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kRequestedAndOriginKeyed),
              1)));
}

// This test ensures that if an origin starts off not being isolated in a
// BrowsingInstance, it continues that way within the BrowsingInstance, even
// if the header starts being sent.
// Case #1 where the non-opted-in origin is currently in the frame tree.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       OriginNonIsolationStateRetainedForBrowsingInstance1) {
  base::HistogramTester histograms;
  SetHeaderValue("?0");
  // Start off with an a(a,a) page, then navigate the subframe to an isolated
  // sub origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com, foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(3u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);
  FrameTreeNode* child_frame_node1 = root->child_at(1);

  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node0, isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change the server responses to start isolating the sub-origin. It should
  // still be not-isolated, to remain consistent with the other frame.
  SetHeaderValue("?1");

  WebContentsConsoleObserver console_observer(shell()->web_contents());
  console_observer.SetPattern(
      "The page requested an origin-keyed agent cluster using the "
      "Origin-Agent-Cluster header, but could not be origin-keyed*");

  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node1, isolated_suborigin_url));

  ASSERT_TRUE(console_observer.Wait());

  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());

  // Make sure the master opt-in list has the origin listed.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      web_contents()->GetBrowserContext(),
      url::Origin::Create(isolated_suborigin_url)));

  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(
          // Original loads of a(a,a) go here.
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kNotRequestedAndNotOriginKeyed),
              4),
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kRequestedButNotOriginKeyed),
              1)));
}

// This test ensures that if an origin starts off not being isolated in a
// BrowsingInstance, it continues that way within the BrowsingInstance, even
// if the header starts being sent.
// Case #2 where the non-opted-in origin is currently not in the frame tree.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       OriginNonIsolationStateRetainedForBrowsingInstance2) {
  SetHeaderValue("?0");
  // Start off with an a(a) page, then navigate the subframe to an isolated sub
  // origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);

  // Even though we're navigating to isolated.foo.com, there's no manifest
  // requesting opt-in, so it should end up in the same SiteInstance as the
  // main frame.
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node0, isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // This navigation removes isolated_suborigin_url from the frame tree, but it
  // should still be in the session history.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      child_frame_node0, https_server()->GetURL("foo.com", "/title1.html")));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change the server to start isolating the sub-origin. It should
  // still be not isolated, to remain consistent with the other frame.
  SetHeaderValue("?1");
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node0, isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Make sure the master opt-in list has the origin listed.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      web_contents()->GetBrowserContext(),
      url::Origin::Create(isolated_suborigin_url)));

  // Make sure the current browsing instance does *not* isolate the origin.
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       root->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(isolated_suborigin_url),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
}

// This test makes sure that a different tab in the same BrowsingInstance where
// an origin originally did not opt-in respects that state even if the
// server sends a different header.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       OriginNonIsolationStateRetainedForPopup) {
  SetHeaderValue("?0");
  // Start off with an a(a,a) page, then navigate the subframe to an isolated
  // sub origin.
  GURL test_url(https_server()->GetURL("foo.com",
                                       "/cross_site_iframe_factory.html?"
                                       "foo.com(foo.com)"));
  GURL isolated_suborigin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node0 = root->child_at(0);

  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node0, isolated_suborigin_url));
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child_frame_node0->current_frame_host()->GetSiteInstance());

  // Change the server to start isolating the sub-origin. It should
  // not be isolated, to remain consistent with the other frame.
  SetHeaderValue("?1");

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
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       root->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(isolated_suborigin_url),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
}

// This test creates a no-opener popup that is origin-isolated, and has two
// same-sub-origin iframes, one of which requests isolation and one that
// doesn't. The non-isolated child commits first, so the second child shouldn't
// get isolation, but more importantly we shouldn't crash on a NOTREACHED() in
// RenderFrameHostManager that is verifying that the second child frame was
// put in a compatible renderer process.
// https://crbug.com/1099718
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       NoKillForBrowsingInstanceDifferencesInProcess) {
  SetHeaderValue("?1");
  GURL opener_url(https_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), opener_url));

  // Create content for popup. The first subframe is in a sub-domain of the
  // popup mainframe, which is an isolated base-origin. The second subframe is
  // in the same sub-origin as the first, but requests isolation. The isolation
  // request will fail, and both subframes will end up in the same site-locked
  // process as the opener document (due to subframe process reuse).
  GURL popup_subframe1_url(
      https_server()->GetURL("sub.foo.com", "/title1.html"));
  GURL popup_subframe2_url(
      https_server()->GetURL("sub.foo.com", "/isolate_origin"));
  // This is the HTML content for the popup mainframe.
  std::string popup_content = base::StringPrintf(
      R"(<!DOCTYPE html>
         <html><head>
         <meta charset="utf-8">
         <title>This page should not crash when window.open()ed</title>
         </head><body>
         <iframe src="%s"></iframe>
         <iframe></iframe>
         </body></html>)",
      popup_subframe1_url.spec().c_str());
  // The next navigation with relative URL = "/isolate_origin" should serve this
  // content.
  AddContentToQueue(popup_content);

  // Open popup.
  GURL isolated_popup_url(https_server()->GetURL("foo.com", "/isolate_origin"));
  // Opening the popup with "noopener" guarantees that the isolated popup is in
  // a different BrowsingInstance from the opener.
  Shell* popup =
      OpenPopup(shell(), isolated_popup_url, "windowName1", "noopener",
                false /* expect_return_from_window_open */);

  // If we got here without crashing, all that remains is to verify everything
  // is isolated/not-isolated as expected.
  ASSERT_NE(nullptr, popup);
  RenderFrameHostImpl* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetPrimaryMainFrame();
  EXPECT_EQ(2U, popup_root->child_count());
  FrameTreeNode* popup_child1 = popup_root->child_at(0);
  FrameTreeNode* popup_child2 = popup_root->child_at(1);

  // Navigate the second child iframe after the first one has loaded.
  EXPECT_TRUE(NavigateFrameToURL(popup_child2, popup_subframe2_url));

  // Set cookie on `popup_child1` to make sure we don't get a renderer kill in
  // the process with the opener.
  EXPECT_TRUE(ExecJs(popup_child1, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(popup_child1, "document.cookie"));

  // Verify state of various SiteIstances, BrowsingInstances and processes.
  SiteInstanceImpl* root_instance = popup_root->GetSiteInstance();
  EXPECT_TRUE(root_instance->GetSiteInfo().requires_origin_keyed_process());
  SiteInstanceImpl* child1_instance =
      popup_child1->current_frame_host()->GetSiteInstance();
  SiteInstanceImpl* child2_instance =
      popup_child2->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(child1_instance, child2_instance);
  EXPECT_NE(child1_instance, root_instance);

  // Make sure child1 and the opener share the same process, but different
  // BrowsingInstances.
  SiteInstanceImpl* opener_instance =
      static_cast<WebContentsImpl*>(shell()->web_contents())->GetSiteInstance();
  EXPECT_NE(child1_instance->GetBrowsingInstanceId(),
            opener_instance->GetBrowsingInstanceId());
  EXPECT_EQ(child1_instance->GetProcess(), opener_instance->GetProcess());
  EXPECT_FALSE(child2_instance->GetSiteInfo().requires_origin_keyed_process());
}

// Same as NoKillForBrowsingInstanceDifferencesInProcess, except the starting
// page has an isolated iframe that matches the origin that won't get isolation
// in the popup's BrowsingInstance. Since this means that the first
// BrowsingInstance will show sub.foo.com as isolated, then if
// CanAccessDataForOrigin only checks the first BrowsingInstance it will get the
// wrong result.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       NoKillForBrowsingInstanceDifferencesInProcess2) {
  SetHeaderValue("?1");
  // Start on a page with same-site iframe.
  GURL opener_url(https_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), opener_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  GURL isolated_opener_iframe_url(
      https_server()->GetURL("sub.foo.com", "/isolate_origin"));
  EXPECT_TRUE(NavigateFrameToURL(child, isolated_opener_iframe_url));
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(child->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .requires_origin_keyed_process());

  // Create content for popup. The first subframe is in a sub-domain of the
  // popup mainframe, which is an isolated base-origin. The second subframe is
  // in the same sub-origin as the first, but requests isolation. The isolation
  // request will fail, and both subframes will end up in the same site-locked
  // process as the opener document (due to subframe process reuse).
  GURL popup_subframe1_url(
      https_server()->GetURL("sub.foo.com", "/title1.html"));
  GURL popup_subframe2_url(
      https_server()->GetURL("sub.foo.com", "/isolate_origin"));
  // This is the HTML content for the popup mainframe.
  std::string popup_content = base::StringPrintf(
      R"(<!DOCTYPE html>
         <html><head>
         <meta charset="utf-8">
         <title>This page should not crash when window.open()ed</title>
         </head><body>
         <iframe src="%s"></iframe>
         <iframe></iframe>
         </body></html>)",
      popup_subframe1_url.spec().c_str());
  // The next navigation with relative URL = "/isolate_origin" should serve this
  // content.
  AddContentToQueue(popup_content);

  // Open popup.
  GURL isolated_popup_url(https_server()->GetURL("foo.com", "/isolate_origin"));
  // Opening the popup with "noopener" guarantees that the isolated popup is in
  // a different BrowsingInstance from the opener.
  Shell* popup =
      OpenPopup(shell(), isolated_popup_url, "windowName1", "noopener",
                false /* expect_return_from_window_open */);

  // If we got here without crashing, all that remains is to verify everything
  // is isolated/not-isolated as expected.
  ASSERT_NE(nullptr, popup);
  RenderFrameHostImpl* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetPrimaryMainFrame();
  EXPECT_EQ(2U, popup_root->child_count());
  FrameTreeNode* popup_child1 = popup_root->child_at(0);
  FrameTreeNode* popup_child2 = popup_root->child_at(1);

  // Navigate the second child iframe after the first one has loaded.
  EXPECT_TRUE(NavigateFrameToURL(popup_child2, popup_subframe2_url));

  // Set cookie on `popup_child1` to make sure we don't get a renderer kill in
  // the process with the opener.
  EXPECT_TRUE(ExecJs(popup_child1, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(popup_child1, "document.cookie"));

  // Verify state of various SiteIstances, BrowsingInstances and processes.
  SiteInstanceImpl* root_instance = popup_root->GetSiteInstance();
  EXPECT_TRUE(root_instance->GetSiteInfo().requires_origin_keyed_process());
  SiteInstanceImpl* child1_instance =
      popup_child1->current_frame_host()->GetSiteInstance();
  SiteInstanceImpl* child2_instance =
      popup_child2->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(child1_instance, child2_instance);
  EXPECT_NE(child1_instance, root_instance);

  // Make sure child1 and the opener share the same process, but different
  // BrowsingInstances.
  SiteInstanceImpl* opener_instance =
      static_cast<WebContentsImpl*>(shell()->web_contents())->GetSiteInstance();
  EXPECT_NE(child1_instance->GetBrowsingInstanceId(),
            opener_instance->GetBrowsingInstanceId());
  EXPECT_EQ(child1_instance->GetProcess(), opener_instance->GetProcess());
  EXPECT_FALSE(child2_instance->GetSiteInfo().requires_origin_keyed_process());
}

// This test handles the case where the base origin is isolated, but a
// sub-origin isn't. In this case we need to place the sub-origin in a site-
// keyed SiteInstance with the same site URL as the origin-keyed SiteInstance
// used for the isolated base origin. Note: only the isolated base origin will
// have a port in this test, as the non-isolated sub-origin will have its port
// value stripped. The test IsolatedBaseOriginNoPorts tests the case where
// neither the isolated base origin nor the non-isolated sub-origin has a port
// value.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest, IsolatedBaseOrigin) {
  base::HistogramTester histograms;
  SetHeaderValue("?1");
  // Start off with an isolated base-origin in an a(a) configuration, then
  // navigate the subframe to a sub-origin no requesting isolation.
  GURL test_url(https_server()->GetURL(
      "foo.com", "/isolated_base_origin_with_subframe.html"));
  GURL non_isolated_sub_origin1(
      https_server()->GetURL("non_isolated1.foo.com", "/title1.html"));
  GURL non_isolated_sub_origin2(
      https_server()->GetURL("non_isolated2.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(3u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node1 = root->child_at(0);
  FrameTreeNode* child_frame_node2 = root->child_at(1);
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node1, non_isolated_sub_origin1));
  EXPECT_TRUE(
      NavigateToURLFromRenderer(child_frame_node2, non_isolated_sub_origin2));

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(
      policy
          ->DetermineOriginAgentClusterIsolation(root->current_frame_host()
                                                     ->GetSiteInstance()
                                                     ->GetIsolationContext(),
                                                 url::Origin::Create(test_url),
                                                 MakeOACIsolationState(false))
          .requires_origin_keyed_process());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       child_frame_node1->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(non_isolated_sub_origin1),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       child_frame_node2->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(non_isolated_sub_origin2),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());

  // Base origin and subdomains should have different SiteInstances.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(root->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .requires_origin_keyed_process());
  EXPECT_FALSE(child_frame_node1->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .requires_origin_keyed_process());

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
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(test_url)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(non_isolated_sub_origin1)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(non_isolated_sub_origin2)));

  EXPECT_THAT(
      histograms.GetAllSamples("Navigation.OriginAgentCluster.Result"),
      testing::ElementsAre(
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kNotRequestedAndNotOriginKeyed),
              2),
          base::Bucket(
              static_cast<int>(NavigationRequest::OriginAgentClusterEndResult::
                                   kRequestedAndOriginKeyed),
              1)));
}

// This test is the same as OriginIsolationOptInHeaderTest
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
          if (params->url_request.url.path() != "/") {
            return false;
          }

          const std::string headers =
              "HTTP/1.1 200 OK\n"
              "Content-Type: text/html\n"
              "Origin-Agent-Cluster: ?1\n";
          // Note: this call would normally get the headers from
          // isolated_base_origin_with_subframe.html.mock-http-headers,
          // but those are meant for use with a
          // OriginIsolationOptInHeaderTest. and won't work here, so we
          // override them.
          URLLoaderInterceptor::WriteResponse(
              "content/test/data/isolated_base_origin_with_subframe.html",
              params->client.get(), &headers, std::optional<net::SSLInfo>());
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
  EXPECT_EQ(3u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child_frame_node1 = root->child_at(0);
  FrameTreeNode* child_frame_node2 = root->child_at(1);
  EXPECT_TRUE(NavigateToURLFromRenderer(child_frame_node1,
                                        non_isolated_sub_origin_url_a));
  EXPECT_TRUE(NavigateToURLFromRenderer(child_frame_node2,
                                        non_isolated_sub_origin_url_b));

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      root->current_frame_host()
                          ->GetSiteInstance()
                          ->GetIsolationContext(),
                      url::Origin::Create(isolated_base_origin_url),
                      MakeOACIsolationState(false))
                  .requires_origin_keyed_process());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       child_frame_node1->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(non_isolated_sub_origin_url_a),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       child_frame_node2->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       url::Origin::Create(non_isolated_sub_origin_url_b),
                       MakeOACIsolationState(false))
                   .requires_origin_keyed_process());
  // Base origin and subdomains should have different SiteInstances.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame_node1->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(root->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .requires_origin_keyed_process());
  EXPECT_FALSE(child_frame_node1->current_frame_host()
                   ->GetSiteInstance()
                   ->GetSiteInfo()
                   .requires_origin_keyed_process());

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
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  EXPECT_TRUE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(isolated_base_origin_url)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(non_isolated_sub_origin_url_a)));
  EXPECT_FALSE(policy->HasOriginEverRequestedOriginAgentClusterValue(
      browser_context, url::Origin::Create(non_isolated_sub_origin_url_b)));
}

IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       SeparateBrowserContextTest) {
  GURL isolated_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  Shell* shell_otr = CreateOffTheRecordBrowser();

  EXPECT_NE(shell()->web_contents()->GetBrowserContext(),
            shell_otr->web_contents()->GetBrowserContext());

  // The isolation header is not present, so this navigation will result in a
  // site-keyed instance.
  EXPECT_TRUE(NavigateToURL(shell_otr, isolated_origin_url));
  WebContentsImpl* web_contents_shell_otr =
      static_cast<WebContentsImpl*>(shell_otr->web_contents());
  SiteInstanceImpl* site_instance_shell_otr =
      web_contents_shell_otr->GetPrimaryFrameTree()
          .root()
          ->current_frame_host()
          ->GetSiteInstance();
  EXPECT_FALSE(
      site_instance_shell_otr->GetSiteInfo().requires_origin_keyed_process());

  url::Origin isolated_origin = url::Origin::Create(isolated_origin_url);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  // Now navigate a different BrowserContext to the same origin, but this time
  // requesting isolation. The presence of the site-keyed instance in a
  // different BrowsingInstance shouldn't prevent this navigation from being
  // isolated. The presence of the site-keyed instance in a different
  // BrowsingInstance (whether in the same BrowserContext or a different one)
  // shouldn't prevent this navigation from being isolated. We'll test
  // cross-BrowserContext interactions below.
  SetHeaderValue("?1");
  EXPECT_TRUE(NavigateToURL(shell(), isolated_origin_url));
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      static_cast<WebContentsImpl*>(shell()->web_contents())
                          ->GetPrimaryFrameTree()
                          .root()
                          ->current_frame_host()
                          ->GetSiteInstance()
                          ->GetIsolationContext(),
                      isolated_origin, MakeOACIsolationState(false))
                  .requires_origin_keyed_process());

  // Make sure isolating the origin in the main context didn't affect it in the
  // off-the-record context. Specifically, if the opting-in in shell() did leak
  // to shell_otr, then `isolated_origin` will be recorded as non-opted in in
  // that BrowsingInstance. The following check makes sure that
  // `isolated_origin` is not in the non-opt-in list, verifying that the
  // internal bookkeeping is specific to each BrowserContext. Isolating the
  // bookkeeping by BrowserContext prevents timing attacks from detecting
  // whether an origin has been visited in another BrowserContext by detecting
  // the global walk.
  // At this stage, `isolated_origin` is not in the non-opt-in list for this
  // BrowsingInstance, since we haven't yet done a global walk in the OTR
  // BrowserContext, so DetermineOriginAgentClusterIsolation will return true.
  // However, during the navigation by the OpenPopup call below that global walk
  // will be triggered before the url's isolation status is set. This walk is
  // triggered by the call to CheckForIsolationOptIn() in
  // NavigationRequest::OnResponseStarted().
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      static_cast<WebContentsImpl*>(shell_otr->web_contents())
                          ->GetPrimaryFrameTree()
                          .root()
                          ->current_frame_host()
                          ->GetSiteInstance()
                          ->GetIsolationContext(),
                      isolated_origin, MakeOACIsolationState(true))
                  .requires_origin_keyed_process());

  // Make sure the OTR context does a global (i.e. profile) walk if we attempt
  // to now opt-in when we didn't before.
  Shell* popup = OpenPopup(shell_otr, isolated_origin_url, "popup_otr");
  WebContentsImpl* web_contents_popup =
      static_cast<WebContentsImpl*>(popup->web_contents());
  SiteInstanceImpl* site_instance_popup =
      web_contents_popup->GetPrimaryFrameTree()
          .root()
          ->current_frame_host()
          ->GetSiteInstance();
  // This shouldn't be isolated because we already have a non-isolated version
  // of this origin in shell_otr's main frame, in the same BrowsingInstance.
  EXPECT_FALSE(
      site_instance_popup->GetSiteInfo().requires_origin_keyed_process());
  // Since the OpenPopup navigation triggered a global walk, `isolated_origin`
  // was added to the non-opt-in list, so now calling
  // DetermineOriginAgentClusterIsolation will return false.
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       site_instance_popup->GetIsolationContext(),
                       isolated_origin, MakeOACIsolationState(true))
                   .requires_origin_keyed_process());

  // Opening a new tab in the OTR profile, which will create a new
  // BrowsingInstance, should be allowed to isolate.
  Shell* shell_otr_tab2 = CreateOffTheRecordBrowser();
  EXPECT_TRUE(NavigateToURL(shell_otr_tab2, isolated_origin_url));
  WebContentsImpl* web_contenst_shell_otr_tab2 =
      static_cast<WebContentsImpl*>(shell_otr_tab2->web_contents());
  SiteInstanceImpl* site_instance_shell_otr_tab2 =
      web_contenst_shell_otr_tab2->GetPrimaryFrameTree()
          .root()
          ->current_frame_host()
          ->GetSiteInstance();
  EXPECT_TRUE(site_instance_shell_otr_tab2->GetSiteInfo()
                  .requires_origin_keyed_process());
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      site_instance_shell_otr_tab2->GetIsolationContext(),
                      isolated_origin, MakeOACIsolationState(true))
                  .requires_origin_keyed_process());
}

// This test creates a scenario where we have a frame that is on the initial
// NavigationEntry, and then we created another frame with the same origin
// that opts-in to isolation. The opt-in triggers a walk of the session history
// and the frame tree ... the session history won't pick up the first frame, but
// the frame-tree walk should.
// TODO(crbug.com/40467594): Once every created frame is guaranteed to
// have a FrameNavigationEntry and thus represented in the sesion history, we
// probably can remove the frame-tree walk.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest, FrameTreeTest) {
  EXPECT_TRUE(NavigateToURL(shell(),
                            https_server()->GetURL("bar.com", "/title1.html")));
  // Have tab1 call window.open() to create blank tab2.
  FrameTreeNode* tab1_root = web_contents()->GetPrimaryFrameTree().root();
  ShellAddedObserver new_shell_observer;
  ASSERT_TRUE(ExecJs(tab1_root->current_frame_host(),
                     "window.w = window.open('/nocontent')"));
  Shell* tab2_shell = new_shell_observer.GetShell();

  // Create iframe in tab2.
  FrameTreeNode* tab2_root =
      static_cast<WebContentsImpl*>(tab2_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  ASSERT_TRUE(ExecJs(tab2_root->current_frame_host(),
                     "var iframe = document.createElement('iframe');"
                     "document.body.appendChild(iframe);"));
  EXPECT_EQ(1U, tab2_root->child_count());
  FrameTreeNode* tab2_child = tab2_root->child_at(0);
  GURL isolated_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));
  // Navigate the iframe in tab2 to `isolated_origin_url` without requesting
  // isolation, so it won't be isolated.
  EXPECT_TRUE(NavigateFrameToURL(tab2_child, isolated_origin_url));

  // Do a browser-initiated navigation of tab1 to the same origin, but isolate
  // it this time. This should place the two frames with `isolated_origin_url`
  // into different BrowsingInstances.
  SetHeaderValue("?1");
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
  // Verify that `isolated origin` is in the non-opt-in list for tab2's
  // child's BrowsingInstance. We do this by requesting opt-in for the origin,
  // then verifying that it is denied by DoesOriginRequestOptInIsolation.
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       tab2_child->current_frame_host()
                           ->GetSiteInstance()
                           ->GetIsolationContext(),
                       isolated_origin, MakeOACIsolationState(true))
                   .requires_origin_keyed_process());
  // Verify that `isolated_origin` in tab1 is indeed isolated.
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      tab1_root->current_frame_host()
                          ->GetSiteInstance()
                          ->GetIsolationContext(),
                      isolated_origin, MakeOACIsolationState(false))
                  .requires_origin_keyed_process());
  // Verify that the tab2 child frame is on the initial NavigationEntry.
  EXPECT_TRUE(tab2_shell->web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->IsInitialEntry());

  // Now, create a second frame in tab2 and navigate it to
  // `isolated_origin_url`. Even though isolation is requested, it should not
  // be isolated.
  ASSERT_TRUE(ExecJs(tab2_root->current_frame_host(),
                     "var iframe = document.createElement('iframe');"
                     "document.body.appendChild(iframe);"));
  EXPECT_EQ(2U, tab2_root->child_count());
  FrameTreeNode* tab2_child2 = tab2_root->child_at(1);
  NavigateFrameToURL(tab2_child2, isolated_origin_url);
  EXPECT_EQ(tab2_child->current_frame_host()->GetSiteInstance(),
            tab2_child2->current_frame_host()->GetSiteInstance());

  // Check that the two child frames can script each other.
  EXPECT_TRUE(ExecJs(tab2_child2, R"(
      parent.frames[0].cross_frame_property_test = 'hello from t2c2'; )"));
  EXPECT_EQ("hello from t2c2",
            EvalJs(tab2_child, "window.cross_frame_property_test;"));
}

// Similar to FrameTreeTest, but we stop the navigation that's not requesting
// isolation at the pending commit state in tab2, then verify that the FrameTree
// walk has correctly registered the origin as non-isolated in tab2, but
// isolated in tab1.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
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
  // it this time. This should place the two frames with `isolated_origin_url`
  // into different BrowsingInstances.
  SetHeaderValue("?1");
  EXPECT_TRUE(NavigateToURL(tab2, isolated_origin_url));

  // Now commit the non-isolated navigation.
  ASSERT_TRUE(non_isolated_delayer.WaitForNavigationFinished());

  FrameTreeNode* tab1_root = web_contents()->GetPrimaryFrameTree().root();
  SiteInstanceImpl* tab1_site_instance =
      tab1_root->current_frame_host()->GetSiteInstance();
  FrameTreeNode* tab2_root = static_cast<WebContentsImpl*>(tab2->web_contents())
                                 ->GetPrimaryFrameTree()
                                 .root();
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
  // Verify that `isolated origin` is in the non-opt-in list for tab1's
  // BrowsingInstance. We do this by requesting opt-in for the origin, then
  // verifying that it is denied by DetermineOriginAgentClusterIsolation.
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       tab1_site_instance->GetIsolationContext(),
                       isolated_origin, MakeOACIsolationState(true))
                   .requires_origin_keyed_process());

  // Verify that `isolated_origin` in tab2 is indeed isolated.
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      tab2_site_instance->GetIsolationContext(),
                      isolated_origin, MakeOACIsolationState(false))
                  .requires_origin_keyed_process());
}

// Helper class to navigate a second tab to a specified URL that requests opt-in
// origin isolation just before the first tab processes the next
// DidCommitProvisionalLoad message.
class InjectIsolationRequestingNavigation
    : public DidCommitNavigationInterceptor {
 public:
  InjectIsolationRequestingNavigation(
      OriginIsolationOptInHeaderTest* test_framework,
      WebContents* tab1_web_contents,
      Shell* tab2,
      const GURL& url)
      : DidCommitNavigationInterceptor(tab1_web_contents),
        test_framework_(test_framework),
        tab2_(tab2),
        url_(url) {}

  InjectIsolationRequestingNavigation(
      const InjectIsolationRequestingNavigation&) = delete;
  InjectIsolationRequestingNavigation& operator=(
      const InjectIsolationRequestingNavigation&) = delete;

  bool was_called() { return was_called_; }

 private:
  // DidCommitNavigationInterceptor implementation.
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr*,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    was_called_ = true;

    // Performa a navigation of `tab2_` to `url_`. `url_` should request
    // isolation.
    test_framework_->SetHeaderValue("?1");
    EXPECT_TRUE(NavigateToURL(tab2_, *url_));

    return true;
  }

  raw_ptr<OriginIsolationOptInHeaderTest> test_framework_ = nullptr;
  raw_ptr<Shell> tab2_ = nullptr;
  const raw_ref<const GURL> url_;
  bool was_called_ = false;
};

// TODO(crbug.com/40708791): flaky on Android builders since 2020-07-28.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_FrameTreeTestBeforeDidCommit DISABLED_FrameTreeTestBeforeDidCommit
#else
#define MAYBE_FrameTreeTestBeforeDidCommit FrameTreeTestBeforeDidCommit
#endif
// This test is similar to the one above, but exercises the pending navigation
// when it's at a different stage, namely between the CommitNavigation and
// DidCommitProvisionalLoad, rather than at WillProcessResponse.
IN_PROC_BROWSER_TEST_F(OriginIsolationOptInHeaderTest,
                       MAYBE_FrameTreeTestBeforeDidCommit) {
  GURL isolated_origin_url(
      https_server()->GetURL("isolated.foo.com", "/isolate_origin"));

  FrameTreeNode* tab1_root = web_contents()->GetPrimaryFrameTree().root();
  // We use the following, slightly more verbose, code instead of
  // CreateBrowser() in order to avoid issues with NavigateToURL() in
  // InjectIsolationRequestingNavigation::WillProcessDidCommitNavigation()
  // getting stuck when it calls for WaitForLoadStop internally.
  Shell* tab2 =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL(), nullptr, gfx::Size());

  InjectIsolationRequestingNavigation injector(this, web_contents(), tab2,
                                               isolated_origin_url);
  {
    TestNavigationObserver tab1_navigation_observer(shell()->web_contents(), 1);
    tab1_navigation_observer.set_expected_initial_url(isolated_origin_url);
    shell()->LoadURL(isolated_origin_url);

    // Waiting for DidNavigationFinished is sufficient to ensure that
    // `injector.was_called()`.  We can't waiting for DidStopLoading, because
    // running a nested message loop in the injector confuses
    // TestNavigationObserver by changing the order of notifications.
    tab1_navigation_observer.WaitForNavigationFinished();
  }
  EXPECT_TRUE(injector.was_called());

  SiteInstanceImpl* tab1_site_instance =
      tab1_root->current_frame_host()->GetSiteInstance();
  FrameTreeNode* tab2_root = static_cast<WebContentsImpl*>(tab2->web_contents())
                                 ->GetPrimaryFrameTree()
                                 .root();
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
  // Verify that `isolated origin` is in the non-opt-in list for tab1's
  // BrowsingInstance. We do this by requesting opt-in for the origin, then
  // verifying that it is denied by DoesOriginRequestOptInIsolation.
  EXPECT_FALSE(policy
                   ->DetermineOriginAgentClusterIsolation(
                       tab1_site_instance->GetIsolationContext(),
                       isolated_origin, MakeOACIsolationState(true))
                   .requires_origin_keyed_process());

  // Verify that `isolated_origin` in tab2 is indeed isolated.
  EXPECT_TRUE(policy
                  ->DetermineOriginAgentClusterIsolation(
                      tab2_site_instance->GetIsolationContext(),
                      isolated_origin, MakeOACIsolationState(false))
                  .requires_origin_keyed_process());
}

class StrictOriginIsolationTest : public IsolatedOriginTestBase {
 public:
  StrictOriginIsolationTest() = default;
  ~StrictOriginIsolationTest() override = default;

  StrictOriginIsolationTest(const StrictOriginIsolationTest&) = delete;
  StrictOriginIsolationTest& operator=(const StrictOriginIsolationTest&) =
      delete;

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

  // Helper function that creates an http URL for `host` that includes the test
  // server's port and returns the strict ProcessLock for that URL.
  ProcessLock GetStrictProcessLockForHost(const std::string& host) {
    return GetStrictProcessLock(embedded_test_server()->GetURL(host, "/"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(StrictOriginIsolationTest, SubframesAreIsolated) {
  GURL test_url(embedded_test_server()->GetURL(
      "foo.com",
      "/cross_site_iframe_factory.html?"
      "foo.com(mail.foo.com,bar.foo.com(foo.com),foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ(5u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  // Make sure we have three separate processes.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  RenderFrameHost* main_frame = root->current_frame_host();
  int main_frame_id = main_frame->GetProcess()->GetID();
  RenderFrameHost* child_frame0 = root->child_at(0)->current_frame_host();
  RenderFrameHost* child_frame1 = root->child_at(1)->current_frame_host();
  RenderFrameHost* child_frame2 = root->child_at(2)->current_frame_host();
  RenderFrameHost* grandchild_frame0 =
      root->child_at(1)->child_at(0)->current_frame_host();
  EXPECT_NE(main_frame_id, child_frame0->GetProcess()->GetID());
  EXPECT_NE(main_frame_id, child_frame1->GetProcess()->GetID());
  EXPECT_EQ(main_frame_id, child_frame2->GetProcess()->GetID());
  EXPECT_EQ(main_frame_id, grandchild_frame0->GetProcess()->GetID());

  EXPECT_EQ(GetStrictProcessLockForHost("foo.com"),
            main_frame->GetProcess()->GetProcessLock());
  EXPECT_EQ(GetStrictProcessLockForHost("mail.foo.com"),
            child_frame0->GetProcess()->GetProcessLock());
  EXPECT_EQ(GetStrictProcessLockForHost("bar.foo.com"),
            child_frame1->GetProcess()->GetProcessLock());
  EXPECT_EQ(GetStrictProcessLockForHost("foo.com"),
            child_frame2->GetProcess()->GetProcessLock());
  EXPECT_EQ(GetStrictProcessLockForHost("foo.com"),
            grandchild_frame0->GetProcess()->GetProcessLock());

  // Navigate child_frame1 to a new origin ... it should get its own process.
  FrameTreeNode* child_frame2_node = root->child_at(2);
  GURL foo_url(embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  const auto expected_foo_lock = GetStrictProcessLock(foo_url);
  EXPECT_TRUE(NavigateToURLFromRenderer(child_frame2_node, foo_url));
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child_frame2_node->current_frame_host()->GetSiteInstance());
  // The old RenderFrameHost for subframe3 will no longer be valid, so get the
  // new one.
  child_frame2 = root->child_at(2)->current_frame_host();
  EXPECT_NE(main_frame->GetProcess()->GetID(),
            child_frame2->GetProcess()->GetID());
  EXPECT_EQ(expected_foo_lock, child_frame2->GetProcess()->GetProcessLock());
}

IN_PROC_BROWSER_TEST_F(StrictOriginIsolationTest, MainframesAreIsolated) {
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  const auto expected_foo_lock = GetStrictProcessLock(foo_url);
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  EXPECT_EQ(1u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();

  auto foo_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  SiteInstanceImpl* foo_site_instance = web_contents()->GetSiteInstance();
  EXPECT_EQ(expected_foo_lock,
            ProcessLock::FromSiteInfo(foo_site_instance->GetSiteInfo()));
  EXPECT_EQ(ProcessLock::FromSiteInfo(foo_site_instance->GetSiteInfo()),
            policy->GetProcessLock(foo_process_id));

  GURL sub_foo_url =
      embedded_test_server()->GetURL("sub.foo.com", "/title1.html");
  const auto expected_sub_foo_lock = GetStrictProcessLock(sub_foo_url);
  EXPECT_TRUE(NavigateToURL(shell(), sub_foo_url));
  auto sub_foo_process_id =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  SiteInstanceImpl* sub_foo_site_instance = web_contents()->GetSiteInstance();
  EXPECT_EQ(expected_sub_foo_lock,
            ProcessLock::FromSiteInfo(sub_foo_site_instance->GetSiteInfo()));
  EXPECT_EQ(ProcessLock::FromSiteInfo(sub_foo_site_instance->GetSiteInfo()),
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
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  SiteInstanceImpl* another_foo_site_instance =
      web_contents()->GetSiteInstance();
  EXPECT_NE(another_foo_process_id, sub_foo_process_id);
  EXPECT_NE(another_foo_process_id, foo_process_id);
  EXPECT_EQ(
      expected_another_foo_lock,
      ProcessLock::FromSiteInfo(another_foo_site_instance->GetSiteInfo()));
  EXPECT_EQ(ProcessLock::FromSiteInfo(another_foo_site_instance->GetSiteInfo()),
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

  // Set up effective URL translation that maps both `foo_url` and `bar_url` to
  // `app_url`.
  EffectiveURLContentBrowserTestContentBrowserClient modified_client(
      false /* requires_dedicated_process */);
  modified_client.AddTranslation(foo_url, app_url);
  modified_client.AddTranslation(bar_url, app_url);

  // Calculate the expected SiteInfo for each URL.  Both `foo_url` and
  // `bar_url` should have a site URL of `app_url`, but the process locks
  // should be foo.com and bar.com.
  SiteInfo foo_site_info = SiteInfo::CreateForTesting(
      web_contents()->GetSiteInstance()->GetIsolationContext(), foo_url);
  EXPECT_EQ(app_url, foo_site_info.site_url());
  EXPECT_EQ(foo_url.DeprecatedGetOriginAsURL(),
            foo_site_info.process_lock_url());
  SiteInfo bar_site_info = SiteInfo::CreateForTesting(
      web_contents()->GetSiteInstance()->GetIsolationContext(), bar_url);
  EXPECT_EQ(app_url, bar_site_info.site_url());
  EXPECT_EQ(bar_url.DeprecatedGetOriginAsURL(),
            bar_site_info.process_lock_url());
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
      popup->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  RenderProcessHost* unisolated_process =
      popup->web_contents()->GetPrimaryMainFrame()->GetProcess();

  // Go to isolated.foo.com with a renderer-initiated navigation.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), isolated_url));
  scoped_refptr<SiteInstance> isolated_instance =
      web_contents()->GetSiteInstance();
  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process,
            web_contents()->GetPrimaryMainFrame()->GetProcess());

  // The site URL for isolated.foo.com should be the full origin rather than
  // scheme and eTLD+1.
  EXPECT_EQ(GURL("http://isolated.foo.com/"), isolated_instance->GetSiteURL());

  // Now use a renderer-initiated navigation to go to an unisolated origin,
  // www.foo.com. This should end up back in the `popup`'s process.
  EXPECT_TRUE(NavigateToURLFromRenderer(web_contents(), unisolated_url));
  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process,
            web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Now, perform a browser-initiated navigation to an isolated origin and
  // ensure that this ends up in a new process and SiteInstance for
  // isolated.foo.com.
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_NE(web_contents()->GetSiteInstance(), unisolated_instance);
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            unisolated_process);

  // Go back to www.foo.com: this should end up in the unisolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(unisolated_instance, web_contents()->GetSiteInstance());
  EXPECT_EQ(unisolated_process,
            web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Go back again.  This should go to isolated.foo.com in an isolated process.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }

  EXPECT_EQ(isolated_instance, web_contents()->GetSiteInstance());
  EXPECT_NE(unisolated_process,
            web_contents()->GetPrimaryMainFrame()->GetProcess());

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

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  EXPECT_TRUE(
      ExecJs(grandchild, "location.href = '" + non_isolated_url.spec() + "';"));
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
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  // strict SiteInstaces are not enabled, this should bring the subframe back
  // into the main frame's SiteInstance. If strict SiteInstances are enabled,
  // we expect the SiteInstances to be different because a SiteInstance is not
  // allowed to contain multiple sites in that mode. In all cases though we
  // expect the navigation to end up in the same process.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_FALSE(IsIsolatedOrigin(bar_url));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);

  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(web_contents()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(web_contents()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  }
  EXPECT_EQ(web_contents()->GetSiteInstance()->GetProcess(),
            child->current_frame_host()->GetSiteInstance()->GetProcess());
}

// Check that a new isolated origin subframe will attempt to reuse an existing
// process for that isolated origin, even across BrowsingInstances.  Also check
// that main frame navigations to an isolated origin keep using the default
// process model and do not reuse existing processes.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, SubframeReusesExistingProcess) {
  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Open an unrelated tab in a separate BrowsingInstance, and navigate it to
  // to an isolated origin.  This SiteInstance should have a default process
  // reuse policy - only subframes attempt process reuse.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, isolated_url));
  scoped_refptr<SiteInstanceImpl> second_shell_instance =
      static_cast<SiteInstanceImpl*>(second_shell->web_contents()
                                         ->GetPrimaryMainFrame()
                                         ->GetSiteInstance());
  EXPECT_FALSE(second_shell_instance->IsRelatedSiteInstance(
      root->current_frame_host()->GetSiteInstance()));
  RenderProcessHost* isolated_process = second_shell_instance->GetProcess();
  EXPECT_EQ(ProcessReusePolicy::DEFAULT,
            second_shell_instance->process_reuse_policy());

  // Now navigate the first tab's subframe to an isolated origin.  See that it
  // reuses the existing `isolated_process`.
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(isolated_url, child->current_url());
  EXPECT_EQ(isolated_process, child->current_frame_host()->GetProcess());
  EXPECT_EQ(
      ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME,
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
            second_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Navigate the second tab's subframe to an isolated origin, and check that
  // this new subframe reuses the isolated process of the subframe in the first
  // tab, even though the two are in separate BrowsingInstances.
  NavigateIframeToURL(second_shell->web_contents(), "test_iframe",
                      isolated_url);
  FrameTreeNode* second_subframe =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root()
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
      third_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe cross-site, but not to an isolated origin.  This should
  // stay in the main frame's SiteInstance, unless we're in a strict
  // SiteInstance mode (including --site-per-process). (Note that the bug for
  // which this test is written is exclusive to --isolate-origins and does not
  // happen with --site-per-process.)
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  }

  // Open a blank popup from the iframe.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(child, "window.w = window.open();"));
  Shell* new_shell = new_shell_observer.GetShell();

  // Have the opener iframe navigate the popup to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), isolated_url);
    EXPECT_TRUE(ExecJs(
        child, "window.w.location.href = '" + isolated_url.spec() + "';"));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
  }

  // Simulate the isolated origin in the popup navigating back to bar.com.
  GURL bar_url2(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), bar_url2);
    EXPECT_TRUE(
        ExecJs(new_shell, "location.href = '" + bar_url2.spec() + "';"));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
  }

  // Check that the popup ended up in the same SiteInstance as its same-site
  // opener iframe.
  EXPECT_EQ(new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Check that the opener iframe can script the popup.
  EXPECT_EQ(bar_url2.spec(), EvalJs(child, "window.w.location.href;"));
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a blank popup.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "window.w = window.open();"));
  Shell* new_shell = new_shell_observer.GetShell();

  // Have the opener navigate the popup to an isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), isolated_url);
    EXPECT_TRUE(ExecJs(
        root, "window.w.location.href = '" + isolated_url.spec() + "';"));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
  }

  // Simulate the isolated origin in the popup navigating to bar.com.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), bar_url);
    EXPECT_TRUE(ExecJs(new_shell, "location.href = '" + bar_url.spec() + "';"));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
  }

  const SiteInstanceImpl* const root_site_instance_impl =
      static_cast<SiteInstanceImpl*>(
          root->current_frame_host()->GetSiteInstance());
  const SiteInstanceImpl* const newshell_site_instance_impl =
      static_cast<SiteInstanceImpl*>(
          new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  if (AreDefaultSiteInstancesEnabled()) {
    // When default SiteInstances are enabled, all sites that do not
    // require a dedicated process all end up in the same default SiteInstance.
    EXPECT_EQ(newshell_site_instance_impl, root_site_instance_impl);
    EXPECT_TRUE(newshell_site_instance_impl->IsDefaultSiteInstance());
  } else {
    // At this point, the popup and the opener should still be in separate
    // SiteInstances.
    EXPECT_NE(newshell_site_instance_impl, root_site_instance_impl);
    EXPECT_FALSE(newshell_site_instance_impl->IsDefaultSiteInstance());
    EXPECT_FALSE(root_site_instance_impl->IsDefaultSiteInstance());
  }

  // Simulate the isolated origin in the popup navigating to www.foo.com.
  {
    TestNavigationManager manager(new_shell->web_contents(), foo_url);
    EXPECT_TRUE(ExecJs(new_shell, "location.href = '" + foo_url.spec() + "';"));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
  }

  // The popup should now be in the same SiteInstance as its same-site opener.
  EXPECT_EQ(new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());

  // Check that the popup can script the opener.
  EXPECT_EQ(foo_url.spec(), EvalJs(new_shell, "window.opener.location.href;"));
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
  EXPECT_TRUE(ExecJs(shell(), "location.href = '" + bar_url.spec() + "';"));
  observer.Wait();

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate bar.com's subframe to an isolated origin with its own subframe.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(isolated_url, child->current_url());
  FrameTreeNode* grandchild = child->child_at(0);

  // Navigate the isolated origin's subframe back to bar.com, completing the
  // ABA hierarchy.
  EXPECT_TRUE(NavigateToURLFromRenderer(grandchild, bar_url));

  // The root and grandchild should be in the same SiteInstance, and the
  // middle child should be in a different SiteInstance.
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());

  // Check that the root frame can script the same-site grandchild frame.
  EXPECT_EQ(bar_url.spec(), EvalJs(root, "frames[0][0].location.href;"));
}

// Check that isolated origins can access cookies.  This requires cookie checks
// on the IO thread to be aware of isolated origins.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, Cookies) {
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  EXPECT_TRUE(ExecJs(web_contents(), "document.cookie = 'foo=bar';"));

  EXPECT_EQ("foo=bar", EvalJs(web_contents(), "document.cookie;"));
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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
        SiteInfo::CreateForTesting(isolation_context, url));
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
      new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
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
  // `isolated_foo_url` stays in that new process without consulting whether it
  // can now reuse a different process.  This should be fixed; see
  // https://crbug.com/513036.   Without --site-per-process, this works because
  // the site-less SiteInstance is allowed to reuse the first tab's foo.com
  // process (which isn't dedicated), and then it swaps to the isolated.foo.com
  // process during navigation.
  if (!AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(child->current_frame_host()->GetProcess(), isolated_foo_process);
  }

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
  // `new_shell` process.
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
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(isolated_foo_process,
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_EQ(isolated_bar_process,
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Navigate second tab to a non-isolated URL and see that it goes back into
  // the www.foo.com process, and that it does not share processes with any
  // isolated origins.
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  EXPECT_EQ(foo_process,
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(isolated_foo_process,
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_NE(isolated_bar_process,
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
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
  ASSERT_TRUE(foo_delayer.WaitForNavigationFinished());

  // Commit the isolated origin.
  ASSERT_TRUE(isolated_delayer.WaitForNavigationFinished());

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
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
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
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
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());

  // Wait for response from the first tab.  This should notice that the first
  // process is no longer suitable for the final destination (which is an
  // unisolated URL) and transfer to another process.  In
  // https://crbug.com/773809, this led to a CHECK due to origin lock mismatch.
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
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
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(starting_site_instance->HasSite());
  EXPECT_TRUE(starting_site_instance->HasProcess());
  EXPECT_TRUE(web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

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
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_FALSE(web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

  // Wait for response in the first tab.  This should notice that the first
  // process is no longer suitable for the isolated origin because it should
  // already be marked as used, and transfer to another process.
  ASSERT_TRUE(manager.WaitForNavigationFinished());

  // Ensure that the isolated origin did not share a process with the second
  // tab.
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
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
  ASSERT_TRUE(foo_delayer.WaitForNavigationFinished());

  // Ensure that the isolated origin did not share a process with the first
  // tab.
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
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
  EXPECT_TRUE(web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

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
                            ->GetPrimaryFrameTree()
                            .root();
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(child->current_frame_host()->GetProcess(),
            root->current_frame_host()->GetProcess());

  // Manipulating cookies from the main frame should not result in a renderer
  // kill.
  EXPECT_TRUE(
      ExecJs(root->current_frame_host(), "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(root->current_frame_host(), "document.cookie;"));
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
  EXPECT_TRUE(web_contents()->GetPrimaryMainFrame()->GetProcess()->IsUnused());

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
  BrowserContext* browser_context = web_contents()->GetBrowserContext();
  scoped_refptr<SiteInstanceImpl> sw_site_instance =
      SiteInstanceImpl::CreateForServiceWorker(
          browser_context,
          UrlInfo::CreateForTesting(
              hung_isolated_url,
              StoragePartitionConfig::CreateDefault(browser_context)),
          /* can_reuse_process= */ true);
  RenderProcessHost* sw_host = sw_site_instance->GetProcess();
  EXPECT_NE(new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            sw_host);

  // Cancel the hung request and commit a real navigation to an isolated
  // origin. This should now end up in the ServiceWorker's process.
  web_contents()->GetPrimaryFrameTree().root()->ResetNavigationRequest(
      NavigationDiscardReason::kExplicitCancellation);
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_EQ(web_contents()->GetPrimaryMainFrame()->GetProcess(), sw_host);
}

// Check that subdomains on an isolated origin (e.g., bar.isolated.foo.com)
// also end up in the isolated origin's SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, IsolatedOriginWithSubdomain) {
  // Start on a page with an isolated origin with a same-site iframe.
  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  EXPECT_TRUE(ExecJs(web_contents(), "location.href = '" +
                                         isolated_subdomain_url.spec() + "'"));
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

// This class allows intercepting the BindStorageArea and OpenLocalStorage
// methods in order to test what happens when parameters are changed.
class StoragePartitonInterceptor
    : public blink::mojom::DomStorageInterceptorForTesting,
      public RenderProcessHostObserver {
 public:
  StoragePartitonInterceptor(
      RenderProcessHostImpl* rph,
      mojo::PendingReceiver<blink::mojom::DomStorage> receiver,
      std::optional<blink::StorageKey> storage_key_to_inject,
      std::optional<blink::LocalFrameToken> local_frame_token_to_inject,
      bool inject_first_local_frame_token)
      : storage_key_to_inject_(storage_key_to_inject),
        local_frame_token_to_inject_(local_frame_token_to_inject),
        save_first_local_frame_token_(inject_first_local_frame_token) {
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(rph->GetStoragePartition());

    // Bind the real DomStorage implementation.
    mojo::PendingRemote<blink::mojom::DomStorageClient> unused_client;
    std::ignore = unused_client.InitWithNewPipeAndPassReceiver();
    mojo::ReceiverId receiver_id = storage_partition->BindDomStorage(
        rph->GetID(), std::move(receiver), std::move(unused_client));

    // Now replace it with this object and keep a pointer to the real
    // implementation.
    dom_storage_ = storage_partition->dom_storage_receivers_for_testing()
                       .SwapImplForTesting(receiver_id, this);

    // Register the `this` as a RenderProcessHostObserver, so it can be
    // correctly cleaned up when the process exits.
    rph->AddObserver(this);
  }

  StoragePartitonInterceptor(const StoragePartitonInterceptor&) = delete;
  StoragePartitonInterceptor& operator=(const StoragePartitonInterceptor&) =
      delete;

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

  // Override this method to allow changing the `storage_key` or
  // `local_frame_token`. It simulates a renderer process sending incorrect
  // data to the browser process, so security checks can be tested.
  void OpenLocalStorage(
      const blink::StorageKey& storage_key,
      const blink::LocalFrameToken& local_frame_token,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override {
    if (save_first_local_frame_token_ && !saved_first_local_frame_token_) {
      saved_first_local_frame_token_ = local_frame_token;
    }
    if (saved_first_local_frame_token_ && !local_frame_token_to_inject_) {
      local_frame_token_to_inject_ = saved_first_local_frame_token_;
    }
    GetForwardingInterface()->OpenLocalStorage(
        storage_key_to_inject_ ? *storage_key_to_inject_ : storage_key,
        local_frame_token_to_inject_ ? *local_frame_token_to_inject_
                                     : local_frame_token,
        std::move(receiver));
  }

  // Override this method to allow changing the `storage_key`. It simulates a
  // renderer process sending incorrect data to the browser process, so
  // security checks can be tested.
  void BindSessionStorageArea(
      const blink::StorageKey& storage_key,
      const blink::LocalFrameToken& local_frame_token,
      const std::string& namespace_id,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override {
    if (save_first_local_frame_token_ && !saved_first_local_frame_token_) {
      saved_first_local_frame_token_ = local_frame_token;
    }
    if (saved_first_local_frame_token_ && !local_frame_token_to_inject_) {
      local_frame_token_to_inject_ = saved_first_local_frame_token_;
    }
    GetForwardingInterface()->BindSessionStorageArea(
        storage_key_to_inject_ ? *storage_key_to_inject_ : storage_key,
        local_frame_token_to_inject_ ? *local_frame_token_to_inject_
                                     : local_frame_token,
        namespace_id, std::move(receiver));
  }

 private:
  static std::optional<blink::LocalFrameToken> saved_first_local_frame_token_;
  // Keep a pointer to the original implementation of the service, so all
  // calls can be forwarded to it.
  raw_ptr<blink::mojom::DomStorage> dom_storage_ = nullptr;
  std::optional<blink::StorageKey> storage_key_to_inject_;
  std::optional<blink::LocalFrameToken> local_frame_token_to_inject_;
  bool save_first_local_frame_token_;
};

std::optional<blink::LocalFrameToken>
    StoragePartitonInterceptor::saved_first_local_frame_token_ = std::nullopt;

// Save the first LocalFrameToken seen and inject it into future calls.
void CreateTestDomStorageBackendToSaveFirstFrame(
    RenderProcessHostImpl* rph,
    mojo::PendingReceiver<blink::mojom::DomStorage> receiver) {
  // This object will register as RenderProcessHostObserver, so it will
  // clean itself automatically on process exit.
  new StoragePartitonInterceptor(rph, std::move(receiver), std::nullopt,
                                 std::nullopt,
                                 /* save_first_local_frame_token_ */ true);
}

// Inject (or not if null) a StorageKey and LocalFrameToken.
void CreateTestDomStorageBackendToInjectValues(
    std::optional<blink::StorageKey> storage_key_to_inject,
    std::optional<blink::LocalFrameToken> local_frame_token_to_inject,
    RenderProcessHostImpl* rph,
    mojo::PendingReceiver<blink::mojom::DomStorage> receiver) {
  // This object will register as RenderProcessHostObserver, so it will
  // clean itself automatically on process exit.
  new StoragePartitonInterceptor(rph, std::move(receiver),
                                 storage_key_to_inject,
                                 local_frame_token_to_inject,
                                 /* save_first_local_frame_token_ */ false);
}

// Verify that a renderer process cannot read sessionStorage of another origin.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, SessionStorage_WrongOrigin) {
  auto mismatched_storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://bar.com");
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToInjectValues,
                          mismatched_storage_key, std::nullopt));

  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      web_contents()->GetPrimaryMainFrame()->GetProcess());
  // Use std::ignore here, since on Android the renderer process is
  // terminated, but ExecJs still returns true. It properly returns
  // false on all other platforms.
  std::ignore =
      ExecJs(web_contents()->GetPrimaryMainFrame(), "sessionStorage.length;");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Verify not fatal if the renderer reads sessionStorage from an empty
// LocalFrameToken.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       SessionStorage_EmptyLocalFrameToken) {
  // This sets up some initial sessionStorage state for the subsequent test.
  GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "sessionStorage.setItem('key', 'value');"));
  EXPECT_EQ(1, EvalJs(web_contents()->GetPrimaryMainFrame(),
                      "sessionStorage.length"));

  // Set up the IPC injection and crash the renderer process so that it's used.
  // Without crashing the renderer, the default IPC will be used.
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToInjectValues,
                          std::nullopt, blink::LocalFrameToken()));
  RenderProcessHost* renderer_process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  // Re-do tests now that injection is in place
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ(0, EvalJs(web_contents()->GetPrimaryMainFrame(),
                      "sessionStorage.length"));
}

// Verify fatal error if the renderer reads sessionStorage from the wrong
// LocalFrameToken.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       SessionStorage_WrongLocalFrameToken) {
  // This sets up some initial sessionStorage state for the subsequent test.
  GURL isolated_url(embedded_test_server()->GetURL(
      "isolated.foo.com",
      "/cross_site_iframe_factory.html?isolated.foo.com(bar.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "sessionStorage.setItem('key', 'value');"));
  EXPECT_EQ(1, EvalJs(web_contents()->GetPrimaryMainFrame(),
                      "sessionStorage.length"));
  EXPECT_TRUE(ExecJs(ChildFrameAt(shell(), 0),
                     "sessionStorage.setItem('key', 'value');"));
  EXPECT_EQ(1, EvalJs(ChildFrameAt(shell(), 0), "sessionStorage.length"));

  // Set up the IPC injection and crash the renderer process so that it's used.
  // Without crashing the renderer, the default IPC will be used.
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToSaveFirstFrame));
  RenderProcessHost* renderer_process_iframe =
      ChildFrameAt(shell(), 0)->GetProcess();
  RenderProcessHostWatcher crash_observer_iframe(
      renderer_process_iframe,
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process_iframe->Shutdown(0);
  crash_observer_iframe.Wait();
  RenderProcessHost* renderer_process_root =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer_root(
      renderer_process_root, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process_root->Shutdown(0);
  crash_observer_root.Wait();

  // Re-do tests now that injection is in place
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_EQ(1, EvalJs(web_contents()->GetPrimaryMainFrame(),
                      "sessionStorage.length"));
  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      ChildFrameAt(shell(), 0)->GetProcess());
  std::ignore = ExecJs(ChildFrameAt(shell(), 0), "sessionStorage.length");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
  // The subframe has crashed, but the main frame should still be alive and
  // working.
  EXPECT_EQ(1, EvalJs(web_contents()->GetPrimaryMainFrame(),
                      "sessionStorage.length"));
}

// Verify not fatal if the renderer reads localStorage from an empty
// LocalFrameToken.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, LocalStorage_EmptyLocalFrameToken) {
  // This sets up some initial localStorage state for the subsequent test.
  GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "localStorage.setItem('key', 'value');"));
  EXPECT_EQ(
      1, EvalJs(web_contents()->GetPrimaryMainFrame(), "localStorage.length"));

  // Set up the IPC injection and crash the renderer process so that it's used.
  // Without crashing the renderer, the default IPC will be used.
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToInjectValues,
                          std::nullopt, blink::LocalFrameToken()));
  RenderProcessHost* renderer_process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      renderer_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process->Shutdown(0);
  crash_observer.Wait();

  // Re-do tests now that injection is in place
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ(
      0, EvalJs(web_contents()->GetPrimaryMainFrame(), "localStorage.length"));
}

// Verify fatal error if the renderer reads localStorage from the wrong
// LocalFrameToken.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, LocalStorage_WrongLocalFrameToken) {
  // This sets up some initial localStorage state for the subsequent test.
  GURL isolated_url(embedded_test_server()->GetURL(
      "isolated.foo.com",
      "/cross_site_iframe_factory.html?isolated.foo.com(bar.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     "localStorage.setItem('key', 'value');"));
  EXPECT_EQ(
      1, EvalJs(web_contents()->GetPrimaryMainFrame(), "localStorage.length"));
  EXPECT_TRUE(ExecJs(ChildFrameAt(shell(), 0),
                     "localStorage.setItem('key', 'value');"));
  EXPECT_EQ(1, EvalJs(ChildFrameAt(shell(), 0), "localStorage.length"));

  // Set up the IPC injection and crash the renderer process so that it's used.
  // Without crashing the renderer, the default IPC will be used.
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToSaveFirstFrame));
  RenderProcessHost* renderer_process_iframe =
      ChildFrameAt(shell(), 0)->GetProcess();
  RenderProcessHostWatcher crash_observer_iframe(
      renderer_process_iframe,
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process_iframe->Shutdown(0);
  crash_observer_iframe.Wait();
  RenderProcessHost* renderer_process_root =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer_root(
      renderer_process_root, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  renderer_process_root->Shutdown(0);
  crash_observer_root.Wait();

  // Re-do tests now that injection is in place
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));
  EXPECT_EQ(
      1, EvalJs(web_contents()->GetPrimaryMainFrame(), "localStorage.length"));
  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      ChildFrameAt(shell(), 0)->GetProcess());
  std::ignore = ExecJs(ChildFrameAt(shell(), 0), "localStorage.length");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
  // The subframe has crashed, but the main frame should still be alive and
  // working.
  EXPECT_EQ(
      1, EvalJs(web_contents()->GetPrimaryMainFrame(), "localStorage.length"));
}

// Verify that an isolated renderer process cannot read localStorage of an
// origin outside of its isolated site.
IN_PROC_BROWSER_TEST_F(
    IsolatedOriginTest,
    LocalStorageOriginEnforcement_IsolatedAccessingNonIsolated) {
  auto mismatched_storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://abc.foo.com");
  EXPECT_FALSE(IsIsolatedOrigin(mismatched_storage_key.origin()));
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToInjectValues,
                          mismatched_storage_key, std::nullopt));

  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));

  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  // Use std::ignore here, since on Android the renderer process is
  // terminated, but ExecJs still returns true. It properly returns
  // false on all other platforms.
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       "localStorage.length;");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

#if BUILDFLAG(IS_ANDROID)
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
  auto isolated_storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://isolated.foo.com");
  EXPECT_TRUE(IsIsolatedOrigin(isolated_storage_key.origin()));

  GURL nonisolated_url(
      embedded_test_server()->GetURL("non-isolated.com", "/title1.html"));
  EXPECT_FALSE(IsIsolatedOrigin(url::Origin::Create(nonisolated_url)));

  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToInjectValues,
                          isolated_storage_key, std::nullopt));
  EXPECT_TRUE(NavigateToURL(shell(), nonisolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  // Use std::ignore here, since on Android the renderer process is
  // terminated, but ExecJs still returns true. It properly returns
  // false on all other platforms.
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       "localStorage.length;");
  EXPECT_EQ(bad_message::RPH_MOJO_PROCESS_ERROR, kill_waiter.Wait());
}

// Verify that an IPC request for reading localStorage of an *opaque* origin
// will be rejected.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest,
                       LocalStorageOriginEnforcement_OpaqueOrigin) {
  url::Origin precursor_origin =
      url::Origin::Create(GURL("https://non-isolated.com"));
  const blink::StorageKey opaque_storage_key =
      blink::StorageKey::CreateFirstParty(
          precursor_origin.DeriveNewOpaqueOrigin());
  RenderProcessHostImpl::SetDomStorageBinderForTesting(
      base::BindRepeating(&CreateTestDomStorageBackendToInjectValues,
                          opaque_storage_key, std::nullopt));

  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/title1.html"));
  EXPECT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  content::RenderProcessHostBadIpcMessageWaiter kill_waiter(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess());
  // Use std::ignore here, since on Android the renderer process is
  // terminated, but ExecJs still returns true. It properly returns
  // false on all other platforms.
  std::ignore = ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
                       "localStorage.length;");
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
  ~IsolatedOriginFieldTrialTest() override = default;

  IsolatedOriginFieldTrialTest(const IsolatedOriginFieldTrialTest&) = delete;
  IsolatedOriginFieldTrialTest& operator=(const IsolatedOriginFieldTrialTest&) =
      delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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
  IsolatedOriginCommandLineAndFieldTrialTest(
      const IsolatedOriginCommandLineAndFieldTrialTest&) = delete;
  IsolatedOriginCommandLineAndFieldTrialTest& operator=(
      const IsolatedOriginCommandLineAndFieldTrialTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginFieldTrialTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kIsolateOrigins,
        "https://cmd.line.com/,https://cmdline.com/");
  }
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
  IsolatedOriginLongListTest() = default;
  ~IsolatedOriginLongListTest() override = default;

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

  EXPECT_EQ(4u, CollectAllRenderFrameHosts(shell()->web_contents()).size());
  RenderFrameHost* main_frame = shell()->web_contents()->GetPrimaryMainFrame();
  RenderFrameHost* subframe1 = ChildFrameAt(main_frame, 0);
  RenderFrameHost* subframe2 = ChildFrameAt(main_frame, 1);
  RenderFrameHost* subframe3 = ChildFrameAt(main_frame, 2);
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
    if (AreStrictSiteInstancesEnabled()) {
      EXPECT_NE(main_frame->GetSiteInstance(), subframe3->GetSiteInstance());
    } else {
      EXPECT_EQ(main_frame->GetSiteInstance(), subframe3->GetSiteInstance());
    }
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
// subframe into an OOPIF and its own SiteInstance.  Also check that the error
// page in a subframe ends up in the correct SiteInstance.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, SubframeErrorPages) {
  GURL top_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  GURL isolated_url(
      embedded_test_server()->GetURL("isolated.foo.com", "/close-socket"));
  GURL regular_url(embedded_test_server()->GetURL("a.com", "/close-socket"));

  EXPECT_TRUE(NavigateToURL(shell(), top_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_EQ(2u, root->child_count());

  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  {
    TestFrameNavigationObserver observer(child1);
    NavigationHandleObserver handle_observer(web_contents(), isolated_url);
    EXPECT_TRUE(
        ExecJs(child1, "location.href = '" + isolated_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(child1->current_url(), isolated_url);
    EXPECT_TRUE(handle_observer.is_error());

    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child1->current_frame_host()->GetSiteInstance());
    if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
            /*in_main_frame=*/false)) {
      EXPECT_EQ(GURL("http://isolated.foo.com/"),
                child1->current_frame_host()->GetSiteInstance()->GetSiteURL());
    } else {
      EXPECT_TRUE(child1->current_frame_host()
                      ->GetSiteInstance()
                      ->GetSiteInfo()
                      .is_error_page());
    }
  }

  {
    TestFrameNavigationObserver observer(child2);
    NavigationHandleObserver handle_observer(web_contents(), regular_url);
    EXPECT_TRUE(
        ExecJs(child2, "location.href = '" + regular_url.spec() + "';"));
    observer.Wait();
    EXPECT_EQ(child2->current_url(), regular_url);
    EXPECT_TRUE(handle_observer.is_error());
    if (AreStrictSiteInstancesEnabled()) {
      EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
                child2->current_frame_host()->GetSiteInstance());
      if (!SiteIsolationPolicy::IsErrorPageIsolationEnabled(
              /*in_main_frame=*/false)) {
        EXPECT_EQ(
            SiteInfo::CreateForTesting(
                IsolationContext(web_contents()->GetBrowserContext()),
                regular_url),
            child2->current_frame_host()->GetSiteInstance()->GetSiteInfo());
      }
    } else {
      EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
                child2->current_frame_host()->GetSiteInstance());
    }
    EXPECT_EQ(SiteIsolationPolicy::IsErrorPageIsolationEnabled(
                  /*in_main_frame=*/false),
              child2->current_frame_host()
                  ->GetSiteInstance()
                  ->GetSiteInfo()
                  .is_error_page());
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
  GURL main_url(
      embedded_test_server()->GetURL("www.foo.com",
                                     "/cross_site_iframe_factory.html?www.foo."
                                     "com(isolated.foo.com(c(www.foo.com)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  EXPECT_TRUE(ExecJs(a, "window.name = 'a';"));
  EXPECT_TRUE(ExecJs(d, R"(
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
  } else if (AreStrictSiteInstancesEnabled()) {
    // All sites have their own SiteInstance and sites that are not isolated
    // are all placed in the same process.
    EXPECT_NE(a->GetProcess()->GetID(), b->GetProcess()->GetID());
    EXPECT_EQ(a->GetProcess()->GetID(), c->GetProcess()->GetID());

    EXPECT_NE(a->GetSiteInstance(), b->GetSiteInstance());
    EXPECT_NE(a->GetSiteInstance(), c->GetSiteInstance());
    EXPECT_EQ(a->GetSiteInstance(), d->GetSiteInstance());
    EXPECT_NE(b->GetSiteInstance(), c->GetSiteInstance());

    EXPECT_FALSE(HasDefaultSiteInstance(a));
    EXPECT_FALSE(HasDefaultSiteInstance(b));
    EXPECT_FALSE(HasDefaultSiteInstance(c));
  } else {
    FAIL() << "Unexpected process model configuration.";
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, NavigateToBlobURL) {
  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  NavigateIframeToURL(web_contents(), "test_iframe", isolated_url);
  EXPECT_EQ(child->current_url(), isolated_url);
  EXPECT_TRUE(child->current_frame_host()->IsCrossProcessSubframe());

  // Now navigate the child frame to a Blob URL.
  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecJs(shell()->web_contents()->GetPrimaryMainFrame(),
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

// Test that same-site cross-origin navigations keep user activation even when
// origin isolation is enabled.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTest, UserActivationSameSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(bar)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Sanity check that there is no sticky user activation at first.
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Load cross-origin same-site page into iframe and verify there is still no
  // sticky user activation.
  GURL first_http_url(
      embedded_test_server()->GetURL("isolated.bar.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(child, first_http_url));
  EXPECT_FALSE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(false, EvalJs(child->current_frame_host(),
                          "navigator.userActivation.hasBeenActive",
                          EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Give the child iframe user activation.
  EXPECT_TRUE(ExecJs(child, "// No-op script"));
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Perform another cross-origin same-site navigation in the iframe.
  GURL second_http_url(
      embedded_test_server()->GetURL("bar.com", "/title1.html"));
  EXPECT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(child, second_http_url));

  // The cross-origin same-site navigation should keep the sticky user
  // activation from the previous page.
  EXPECT_TRUE(child->current_frame_host()->HasStickyUserActivation());
  EXPECT_EQ(true, EvalJs(child->current_frame_host(),
                         "navigator.userActivation.hasBeenActive",
                         EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Ensure that top-level navigations can still happen.
  EXPECT_TRUE(ExecJs(child->current_frame_host(),
                     JsReplace("window.open($1, $2)", first_http_url, "_top"),
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(first_http_url, shell()->web_contents()->GetLastCommittedURL());
}

// Ensure that --disable-site-isolation-trials disables origin isolation.
class IsolatedOriginTrialOverrideTest : public IsolatedOriginFieldTrialTest {
 public:
  IsolatedOriginTrialOverrideTest() = default;
  ~IsolatedOriginTrialOverrideTest() override = default;

  IsolatedOriginTrialOverrideTest(const IsolatedOriginTrialOverrideTest&) =
      delete;
  IsolatedOriginTrialOverrideTest& operator=(
      const IsolatedOriginTrialOverrideTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginFieldTrialTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginTrialOverrideTest, Test) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://field.trial.com/")));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://bar.com/")));
}

// Ensure that --disable-site-isolation-trials and/or
// --disable-site-isolation-for-policy do not override the flag.
class IsolatedOriginPolicyOverrideTest : public IsolatedOriginFieldTrialTest {
 public:
  IsolatedOriginPolicyOverrideTest() = default;
  ~IsolatedOriginPolicyOverrideTest() override = default;

  IsolatedOriginPolicyOverrideTest(const IsolatedOriginPolicyOverrideTest&) =
      delete;
  IsolatedOriginPolicyOverrideTest& operator=(
      const IsolatedOriginPolicyOverrideTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginFieldTrialTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
#if BUILDFLAG(IS_ANDROID)
    command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
#endif
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginPolicyOverrideTest, Test) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://field.trial.com/")));
  EXPECT_FALSE(IsIsolatedOrigin(GURL("https://bar.com/")));
}

// Ensure that --disable-site-isolation-trials and/or
// --disable-site-isolation-for-policy do not override the flag.
class IsolatedOriginNoFlagOverrideTest : public IsolatedOriginTest {
 public:
  IsolatedOriginNoFlagOverrideTest() = default;
  ~IsolatedOriginNoFlagOverrideTest() override = default;

  IsolatedOriginNoFlagOverrideTest(const IsolatedOriginNoFlagOverrideTest&) =
      delete;
  IsolatedOriginNoFlagOverrideTest& operator=(
      const IsolatedOriginNoFlagOverrideTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
#if BUILDFLAG(IS_ANDROID)
    command_line->AppendSwitch(switches::kDisableSiteIsolationForPolicy);
#endif
  }
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
      "isolated.foo.com",
      "/cross_site_iframe_factory.html?isolated.foo.com(b(c),d(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  EXPECT_TRUE(ExecJs(c1, "window.name = 'c1';"));
  EXPECT_TRUE(ExecJs(c2, R"(
      c1 = window.open('', 'c1');
      c1.cross_frame_property_test = 'hello from c2'; )"));
  EXPECT_EQ("hello from c2", EvalJs(c1, "window.cross_frame_property_test;"));

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
  ~DynamicIsolatedOriginTest() override = default;

  DynamicIsolatedOriginTest(const DynamicIsolatedOriginTest&) = delete;
  DynamicIsolatedOriginTest& operator=(const DynamicIsolatedOriginTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);

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
};

// Check that dynamically added isolated origins take effect for future
// BrowsingInstances only.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       IsolationAppliesToFutureBrowsingInstances) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Start on a non-isolated origin with same-site iframe.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe cross-site.
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(child->current_url(), bar_url);

  // The two frames should be in the same process, since neither site is
  // isolated so far.
  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  }
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Start isolating foo.com.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
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
  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
    EXPECT_NE(child->current_frame_host()->GetSiteInstance(),
              grandchild->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
    EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
              grandchild->current_frame_host()->GetSiteInstance());
  }
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            grandchild->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
  EXPECT_EQ(child->current_frame_host()->GetProcess(),
            grandchild->current_frame_host()->GetProcess());

  // Create an unrelated window, which will be in a new BrowsingInstance.
  // Ensure that foo.com becomes an isolated origin in that window.  A
  // cross-site bar.com subframe on foo.com should now become an OOPIF.
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, foo_url));

  FrameTreeNode* second_root =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
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
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

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
  policy->AddFutureIsolatedOrigins({url::Origin::Create(bar_url)},
                                   IsolatedOriginSource::TEST);

  // Do a renderer-initiated navigation in each of the existing three windows.
  // None of them should swap to a new process, since bar.com shouldn't be
  // isolated in those older BrowsingInstances.
  int old_process_id =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell(), bar_url));
  EXPECT_EQ(old_process_id,
            web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());

  old_process_id =
      shell2->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell2, bar_url));
  EXPECT_EQ(
      old_process_id,
      shell2->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());

  old_process_id =
      shell3->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell3, bar_url));
  EXPECT_EQ(
      old_process_id,
      shell3->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());

  // Now try the same in a new window and BrowsingInstance, and ensure that the
  // navigation to bar.com swaps processes in that case.
  Shell* shell4 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell4, foo_url));

  old_process_id =
      shell4->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(shell4, bar_url));
  EXPECT_NE(
      old_process_id,
      shell4->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());

  // Go back to foo.com in window 1, ensuring this stays in the same process.
  {
    old_process_id =
        web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
    EXPECT_EQ(old_process_id,
              web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());
  }

  // Go back to foo.com in window 4, ensuring this swaps processes.
  {
    old_process_id =
        shell4->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
    TestNavigationObserver back_observer(shell4->web_contents());
    shell4->web_contents()->GetController().GoBack();
    back_observer.Wait();
    EXPECT_NE(
        old_process_id,
        shell4->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());
  }
}

// Check that dynamically added isolated origins do not prevent older processes
// for the same origin from accessing cookies.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, OldProcessCanAccessCookies) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Since foo.com isn't isolated yet, its process lock should allow any site.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(root->current_frame_host()
                  ->GetProcess()
                  ->GetProcessLock()
                  .allows_any_site());

  // Start isolating foo.com.
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
                                   IsolatedOriginSource::TEST);

  // Create an unrelated window, which will be in a new BrowsingInstance.
  // foo.com will become an isolated origin in that window.
  Shell* second_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(second_shell, foo_url));
  FrameTreeNode* second_root =
      static_cast<WebContentsImpl*>(second_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();

  // The new window's process should be locked to "foo.com".
  int isolated_foo_com_process_id =
      second_root->current_frame_host()->GetProcess()->GetID();
  EXPECT_EQ(ProcessLockFromUrl("http://foo.com"),
            policy->GetProcessLock(isolated_foo_com_process_id));

  // Make sure both old and new foo.com processes can access cookies without
  // renderer kills.
  EXPECT_TRUE(ExecJs(root, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(root, "document.cookie"));
  EXPECT_TRUE(ExecJs(second_root, "document.cookie = 'foo=bar';"));
  EXPECT_EQ("foo=bar", EvalJs(second_root, "document.cookie"));

  // Navigate to sub.foo.com in `second_shell`, staying in same
  // BrowsingInstance.  This should stay in the same process.
  GURL sub_foo_url(
      embedded_test_server()->GetURL("sub.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURLInSameBrowsingInstance(second_shell, sub_foo_url));
  EXPECT_EQ(isolated_foo_com_process_id,
            second_root->current_frame_host()->GetProcess()->GetID());

  // Now, start isolating sub.foo.com.
  policy->AddFutureIsolatedOrigins({url::Origin::Create(sub_foo_url)},
                                   IsolatedOriginSource::TEST);

  // Make sure the process locked to foo.com, which currently has sub.foo.com
  // committed in it, can still access sub.foo.com cookies.
  EXPECT_TRUE(ExecJs(second_root, "document.cookie = 'foo=baz';"));
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
            second_root->current_frame_host()->GetProcess()->GetProcessLock());

  // Make sure that process can also access sub.foo.com cookies.
  EXPECT_TRUE(ExecJs(second_root, "document.cookie = 'foo=qux';"));
  EXPECT_EQ("foo=qux", EvalJs(second_root, "document.cookie"));
}

// Verify that when isolating sub.foo.com dynamically, foo.com and sub.foo.com
// start to be treated as cross-site for process model decisions.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, IsolatedSubdomain) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Start isolating sub.foo.com.
  GURL sub_foo_url(
      embedded_test_server()->GetURL("sub.foo.com", "/title1.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(sub_foo_url)},
                                   IsolatedOriginSource::TEST);

  // Navigate to foo.com and then to sub.foo.com in a new BrowsingInstance.
  // foo.com and sub.foo.com should now be considered cross-site for the
  // purposes of process assignment, and we should swap processes.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  int initial_process_id =
      new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  EXPECT_TRUE(NavigateToURLFromRenderer(new_shell, sub_foo_url));
  EXPECT_NE(
      initial_process_id,
      new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID());

  // Repeat this, but now navigate a subframe on foo.com to sub.foo.com and
  // ensure that it is rendered in an OOPIF.
  new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  NavigateIframeToURL(new_shell->web_contents(), "test_iframe", sub_foo_url);
  FrameTreeNode* root = static_cast<WebContentsImpl*>(new_shell->web_contents())
                            ->GetPrimaryFrameTree()
                            .root();
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
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Force process reuse for main frames in new BrowsingInstances.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start on a non-isolated origin with same-site iframe.
  GURL foo_url(https_server()->GetURL("foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe cross-site.
  GURL bar_url(https_server()->GetURL("bar.com", "/title1.html"));
  NavigateIframeToURL(web_contents(), "test_iframe", bar_url);
  EXPECT_EQ(child->current_url(), bar_url);

  // The iframe should not be in an OOPIF yet.
  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());

  } else {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  }
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // Start isolating bar.com.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(bar_url)},
                                   IsolatedOriginSource::TEST);

  // Open a new window in a new BrowsingInstance.  Navigate to foo.com and
  // check that the old foo.com process is reused.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
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

  // The old foo.com process should still be able to access bar.com data,
  // since it isn't locked to a specific site.
  int old_process_id = root->current_frame_host()->GetProcess()->GetID();
  EXPECT_TRUE(policy->CanAccessDataForOrigin(old_process_id,
                                             url::Origin::Create(bar_url)));

  // In particular, make sure the bar.com iframe in the old foo.com process can
  // still access bar.com cookies.
  EXPECT_TRUE(
      ExecJs(child, "document.cookie = 'foo=bar;SameSite=None;Secure';"));
  EXPECT_EQ("foo=bar", EvalJs(child, "document.cookie"));

  // Make sure the BrowsingInstanceId is cleaned up immediately.
  policy->SetBrowsingInstanceCleanupDelayForTesting(0);

  // Now close the first window.  This destroys the first BrowsingInstance and
  // leaves only the newer BrowsingInstance (with a foo.com main frame) in the
  // old process.
  shell()->Close();

  // Now that the process only contains a BrowsingInstance where bar.com is
  // considered isolated and cannot reuse the old process, it should lose access
  // to bar.com's data due to citadel enforcement in CanAccessDataForOrigin.
  EXPECT_FALSE(policy->CanAccessDataForOrigin(old_process_id,
                                              url::Origin::Create(bar_url)));
}

// Verify that a process locked to foo.com is not reused for a navigation to
// foo.com that does not require a dedicated process.  See
// https://crbug.com/950453.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest,
                       LockedProcessNotReusedForNonisolatedSameSiteNavigation) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Set the process limit to 1.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Start on a non-isolated foo.com URL.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Navigate to a different isolated origin and wait for the original foo.com
  // process to shut down.  Note that the foo.com SiteInstance will stick
  // around in session history.
  RenderProcessHostWatcher foo_process_observer(
      web_contents()->GetPrimaryMainFrame()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);

  // Disable the BackForwardCache to ensure the old process is going to be
  // released.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL isolated_bar_url(
      embedded_test_server()->GetURL("isolated.bar.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_bar_url));
  foo_process_observer.Wait();
  EXPECT_TRUE(foo_process_observer.did_exit_normally());

  // Start isolating foo.com.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
                                   IsolatedOriginSource::TEST);

  // Create a new window, forcing a new BrowsingInstance, and navigate it to
  // foo.com, which will spin up a process locked to foo.com.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  RenderProcessHost* new_process =
      new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(ProcessLockFromUrl("http://foo.com"),
            new_process->GetProcessLock());

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
  EXPECT_NE(web_contents()->GetPrimaryMainFrame()->GetProcess(), new_process);
}

// Checks that isolated origins can be added only for a specific profile,
// and that they don't apply to other profiles.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, PerProfileIsolation) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

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

  // Start isolating foo.com in `other_context` only.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/page_with_iframe.html"));
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
                                   IsolatedOriginSource::TEST, other_context);

  // Verify that foo.com is indeed isolated in `other_shell`, by navigating to
  // it in a new BrowsingInstance and checking that a bar.com subframe becomes
  // an OOPIF.
  EXPECT_TRUE(NavigateToURL(other_shell, foo_url));
  WebContentsImpl* other_contents =
      static_cast<WebContentsImpl*>(other_shell->web_contents());
  NavigateIframeToURL(other_contents, "test_iframe", bar_url);
  FrameTreeNode* root = other_contents->GetPrimaryFrameTree().root();
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
  root = web_contents()->GetPrimaryFrameTree().root();
  child = root->child_at(0);
  EXPECT_EQ(child->current_url(), bar_url);
  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  }
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

// Check that a dynamically added isolated origin can take effect on the next
// main frame navigation by forcing a BrowsingInstance swap, in the case that
// there are no script references to the frame being navigated.
IN_PROC_BROWSER_TEST_F(DynamicIsolatedOriginTest, ForceBrowsingInstanceSwap) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate to a non-isolated page with a cross-site iframe.  The frame
  // shouldn't be in an OOPIF.
  GURL foo_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(bar.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();

  if (AreStrictSiteInstancesEnabled()) {
    EXPECT_NE(first_instance, child->current_frame_host()->GetSiteInstance());
  } else {
    EXPECT_EQ(first_instance, child->current_frame_host()->GetSiteInstance());
  }
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(first_instance->GetProcess()->GetProcessLock().allows_any_site());

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
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
            second_instance->GetProcess()->GetProcessLock());

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
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate to a foo.com page.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  EXPECT_TRUE(first_instance->GetProcess()->GetProcessLock().allows_any_site());

  // Set a sessionStorage value, to sanity check that foo.com's session storage
  // will still be accessible after the BrowsingInstance swap.
  EXPECT_TRUE(ExecJs(root, "window.sessionStorage['foo'] = 'bar';"));

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
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
            second_instance->GetProcess()->GetProcessLock());

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
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate to a page that won't be in a dedicated process.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
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
  EXPECT_TRUE(first_instance->GetProcess()->GetProcessLock().allows_any_site());
}

// This test ensures that when a page becomes isolated in the middle of
// creating and navigating a new window, the new window prevents a
// BrowsingInstance swap.
IN_PROC_BROWSER_TEST_F(
    DynamicIsolatedOriginTest,
    DontForceBrowsingInstanceSwapWithPendingNavigationInNewWindow) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate to a page that won't be in a dedicated process.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  scoped_refptr<SiteInstance> first_instance =
      root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());

  // Open and start navigating a popup to a URL that never finishes loading.
  GURL popup_url(embedded_test_server()->GetURL("a.com", "/hung"));
  EXPECT_TRUE(ExecJs(root, JsReplace("window.open($1);", popup_url)));

  // Start isolating foo.com.
  BrowserContext* context = shell()->web_contents()->GetBrowserContext();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddFutureIsolatedOrigins({url::Origin::Create(foo_url)},
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
  EXPECT_TRUE(first_instance->GetProcess()->GetProcessLock().allows_any_site());
}

class IsolatedOriginTestWithStrictSiteInstances : public IsolatedOriginTest {
 public:
  IsolatedOriginTestWithStrictSiteInstances() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kProcessSharingWithStrictSiteInstances);
  }
  ~IsolatedOriginTestWithStrictSiteInstances() override = default;

  IsolatedOriginTestWithStrictSiteInstances(
      const IsolatedOriginTestWithStrictSiteInstances&) = delete;
  IsolatedOriginTestWithStrictSiteInstances& operator=(
      const IsolatedOriginTestWithStrictSiteInstances&) = delete;

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
};

IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       NonIsolatedFramesCanShareDefaultProcess) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL top_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(top_url)));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  GURL bar_url(embedded_test_server()->GetURL("www.bar.com", "/title3.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(bar_url)));
  {
    TestFrameNavigationObserver observer(child1);
    NavigationHandleObserver handle_observer(web_contents(), bar_url);
    EXPECT_TRUE(ExecJs(child1, "location.href = '" + bar_url.spec() + "';"));
    observer.Wait();
  }

  GURL baz_url(embedded_test_server()->GetURL("www.baz.com", "/title3.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(baz_url)));
  {
    TestFrameNavigationObserver observer(child2);
    NavigationHandleObserver handle_observer(web_contents(), baz_url);
    EXPECT_TRUE(ExecJs(child2, "location.href = '" + baz_url.spec() + "';"));
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
      DepictFrameTree(*root));

  // But none are isolated, so all should share the default process for their
  // BrowsingInstance.
  RenderProcessHost* host = root->current_frame_host()->GetProcess();
  EXPECT_EQ(host, child1->current_frame_host()->GetProcess());
  EXPECT_EQ(host, child2->current_frame_host()->GetProcess());
  EXPECT_TRUE(host->GetProcessLock().allows_any_site());
}

// Creates a non-isolated main frame with an isolated child and non-isolated
// grandchild. With strict site isolation disabled and
// kProcessSharingWithStrictSiteInstances enabled, the main frame and the
// grandchild should be in the same process even though they have different
// SiteInstances.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       IsolatedChildWithNonIsolatedGrandchild) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL top_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  ASSERT_FALSE(IsIsolatedOrigin(url::Origin::Create(top_url)));
  EXPECT_TRUE(NavigateToURL(shell(), top_url));

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  ASSERT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
  EXPECT_TRUE(
      ExecJs(grandchild, "location.href = '" + non_isolated_url.spec() + "';"));
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
      DepictFrameTree(*root));
}

// Navigate a frame into and out of an isolated origin. This should not
// confuse BrowsingInstance into holding onto a stale default_process_.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       SubframeNavigatesOutofIsolationThenToIsolation) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL isolated_url(embedded_test_server()->GetURL("isolated.foo.com",
                                                   "/page_with_iframe.html"));
  ASSERT_TRUE(IsIsolatedOrigin(url::Origin::Create(isolated_url)));
  EXPECT_TRUE(NavigateToURL(shell(), isolated_url));

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
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
      DepictFrameTree(*root));
}

// Ensure a popup and its opener can go in the same process, even though
// they have different SiteInstances with kProcessSharingWithStrictSiteInstances
// enabled.
IN_PROC_BROWSER_TEST_F(IsolatedOriginTestWithStrictSiteInstances,
                       NonIsolatedPopup) {
  // This test is designed to run without strict site isolation.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL foo_url(
      embedded_test_server()->GetURL("www.foo.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Open a blank popup.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(root, "window.w = window.open();"));
  Shell* new_shell = new_shell_observer.GetShell();

  // Have the opener navigate the popup to a non-isolated origin.
  GURL isolated_url(
      embedded_test_server()->GetURL("www.bar.com", "/title1.html"));
  {
    TestNavigationManager manager(new_shell->web_contents(), isolated_url);
    EXPECT_TRUE(ExecJs(
        root, "window.w.location.href = '" + isolated_url.spec() + "';"));
    ASSERT_TRUE(manager.WaitForNavigationFinished());
  }

  // The popup and the opener should not share a SiteInstance, but should
  // end up in the same process.
  EXPECT_NE(new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            new_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(*root));
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = http://bar.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(*static_cast<WebContentsImpl*>(new_shell->web_contents())
                           ->GetPrimaryFrameTree()
                           .root()));
}

class WildcardOriginIsolationTest : public IsolatedOriginTestBase {
 public:
  WildcardOriginIsolationTest() = default;
  ~WildcardOriginIsolationTest() override = default;

  WildcardOriginIsolationTest(const WildcardOriginIsolationTest&) = delete;
  WildcardOriginIsolationTest& operator=(const WildcardOriginIsolationTest&) =
      delete;

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
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> a_foo_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  EXPECT_TRUE(NavigateToURL(shell(), b_foo_url));
  int b_foo_pid =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> b_foo_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

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
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> a_isolated_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  EXPECT_TRUE(NavigateToURL(shell(), b_isolated_url));
  int b_isolated_pid =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  scoped_refptr<SiteInstance> b_isolated_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // Navigating from a non-wildcard domain to a wildcard domain should result in
  // a new process.
  EXPECT_NE(b_foo_pid, b_isolated_pid);
  EXPECT_NE(b_foo_instance, b_isolated_instance);

  // Navigating to another URL within the wildcard domain should always result
  // in a new process.
  EXPECT_NE(a_isolated_pid, b_isolated_pid);
  EXPECT_NE(a_isolated_instance, b_isolated_instance);

  EXPECT_TRUE(NavigateToURL(shell(), a_foo_url));
  a_foo_pid =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  a_foo_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  EXPECT_TRUE(NavigateToURL(shell(), b_foo_url));
  b_foo_pid =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();
  b_foo_instance =
      shell()->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

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
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

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
      DepictFrameTree(*root));
}

// Helper class for testing site isolation triggered by
// Cross-Origin-Opener-Policy headers.  These tests disable strict site
// isolation by default, so that we can check whether a site becomes isolated
// due to COOP on both desktop and Android.
class COOPIsolationTest : public IsolatedOriginTestBase {
 public:
  // Note: the COOP header is only populated for HTTPS.
  COOPIsolationTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    // Note: OriginKeyedProcessesByDefault should only apply when strict site
    // isolation is in effect, and these tests turn that off via
    // NoSiteIsolationContentBrowserClient.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kSiteIsolationForCrossOriginOpenerPolicy},
        /*disabled_features=*/{features::kOriginKeyedProcessesByDefault});
  }

  ~COOPIsolationTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolatedOriginTestBase::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  }

  void SetUpOnMainThread() override {
    IsolatedOriginTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();

    https_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server()->Start());

    browser_client_ = std::make_unique<NoSiteIsolationContentBrowserClient>();

    // The custom ContentBrowserClient above typically ensures that this test
    // runs without strict site isolation, but it's still possible to
    // inadvertently override this when running with --site-per-process on the
    // command line. This might happen on try bots, so these tests take this
    // into account to prevent failures, but this is not an intended
    // configuration for these tests, since with strict site isolation COOP
    // doesn't need to dynamically isolate any sites.
    if (AreAllSitesIsolatedForTesting()) {
      LOG(WARNING) << "This test should be run without --site-per-process, "
                   << "as it's designed to exercise code paths when strict "
                   << "site isolation is turned off.";
    }
  }

  void TearDownOnMainThread() override {
    IsolatedOriginTestBase::TearDownOnMainThread();
    browser_client_.reset();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  // A custom ContentBrowserClient to turn off strict site isolation, since
  // COOP isolation only matters in environments like Android where it
  // is not used.  Note that kSitePerProcess is a higher-layer feature, so we
  // can't just disable it here.
  class NoSiteIsolationContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    bool ShouldEnableStrictSiteIsolation() override { return false; }
  };

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  net::EmbeddedTestServer https_server_;

  std::unique_ptr<NoSiteIsolationContentBrowserClient> browser_client_;
};

// Check that a main frame navigation to a COOP site (with no subsequent user
// gesture) triggers isolation for that site within the current
// BrowsingInstance.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, SameOrigin) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL no_coop_url = https_server()->GetURL("a.com", "/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), no_coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  scoped_refptr<SiteInstance> first_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_FALSE(first_instance->RequiresDedicatedProcess());

  // Navigate to a b.com URL with COOP, swapping BrowsingInstances.
  GURL coop_url = https_server()->GetURL(
      "b.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  SiteInstanceImpl* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  // The b.com COOP page should trigger the isolation heuristic and require a
  // dedicated process locked to b.com.
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());

  auto lock = coop_instance->GetProcess()->GetProcessLock();
  EXPECT_TRUE(lock.is_locked_to_site());
  EXPECT_EQ(ProcessLockFromUrl("https://b.com"), lock);

  // Check that a cross-site subframe in a non-isolated site becomes an OOPIF
  // in a new, non-isolated SiteInstance.
  ASSERT_TRUE(ExecJs(shell(),
                     "var iframe = document.createElement('iframe');"
                     "iframe.id = 'child';"
                     "document.body.appendChild(iframe);",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  GURL c_url(https_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child", c_url));
  SiteInstanceImpl* child_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(coop_instance, child_instance);
  EXPECT_NE(coop_instance->GetProcess(), child_instance->GetProcess());
  EXPECT_FALSE(child_instance->RequiresDedicatedProcess());

  // Navigating the subframe back to b.com should bring it back to the parent
  // SiteInstance.
  GURL b_url(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child", b_url));
  child_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(coop_instance, child_instance);

  // Create a new window, forcing a new BrowsingInstance, and check that b.com
  // is *not* isolated in it.  Since b.com in `coop_instance`'s
  // BrowsingInstance hasn't received a user gesture, the COOP isolation does
  // not apply to other BrowsingInstances.
  Shell* new_shell = CreateBrowser();
  GURL no_coop_b_url = https_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(NavigateToURL(new_shell, no_coop_b_url));
  SiteInstanceImpl* new_instance = static_cast<SiteInstanceImpl*>(
      new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(new_instance->RequiresDedicatedProcess());
}

// Verify that the same-origin-allow-popups COOP header value triggers
// isolation, and that this behaves sanely with window.open().
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, SameOriginAllowPopups) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate to a coop.com URL with COOP.
  GURL coop_url = https_server()->GetURL(
      "coop.com",
      "/set-header?Cross-Origin-Opener-Policy: same-origin-allow-popups");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups);
  SiteInstanceImpl* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  // The coop.com COOP page should trigger the isolation heuristic and require
  // a dedicated process locked to coop.com.
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());

  auto lock = coop_instance->GetProcess()->GetProcessLock();
  EXPECT_TRUE(lock.is_locked_to_site());
  EXPECT_EQ(ProcessLockFromUrl("https://coop.com"), lock);

  // Open a non-COOP same-site URL in a popup, which should stay in the same
  // BrowsingInstance because of same-origin-allow-popups.  Verify that the
  // popup ends up in the same SiteInstance as the opener (which requires a
  // dedicated process).
  GURL popup_url(https_server()->GetURL("coop.com", "/title1.html"));
  Shell* popup = OpenPopup(shell(), popup_url, "");
  RenderFrameHostImpl* popup_rfh = static_cast<RenderFrameHostImpl*>(
      popup->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy().value,
            network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  EXPECT_EQ(popup_rfh->GetSiteInstance(), coop_instance);

  // Navigate the popup to another non-isolated site, staying in the same
  // BrowsingInstance, and verify that it swaps to a new non-isolated
  // SiteInstance.  The non-isolated site has a child which is same-origin with
  // the COOP page; verify that it's placed in the same SiteInstance as the
  // COOP page, as they are allowed to synchronously script each other.
  GURL a_url(https_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com(coop.com)"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, a_url));
  SiteInstanceImpl* new_instance = static_cast<SiteInstanceImpl*>(
      popup->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(new_instance->RequiresDedicatedProcess());
  EXPECT_NE(new_instance, coop_instance);
  FrameTreeNode* popup_child =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0);
  EXPECT_EQ(popup_child->current_frame_host()->GetSiteInstance(),
            coop_instance);

  // Navigate the popup to coop.com again, staying in the same
  // BrowsingInstance, and verify that it goes back to the opener's
  // SiteInstance.
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, popup_url));
  EXPECT_EQ(popup->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            coop_instance);
}

class COOPIsolationNoopenerTest : public COOPIsolationTest {
 public:
  COOPIsolationNoopenerTest() {
    feature_list_.InitAndEnableFeature(
        network::features::kCoopNoopenerAllowPopups);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verify that the `noopener-allow-popups COOP header value triggers isolation,
// and that this behaves sanely with window.open().
IN_PROC_BROWSER_TEST_F(COOPIsolationNoopenerTest, NoopenerAllowPopups) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate to a coop.com URL with no COOP.
  GURL coop_url = https_server()->GetURL(
      "coop.com", "/set-header?Cross-Origin-Opener-Policy: unsafe-none");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone);
  SiteInstanceImpl* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  // The coop.com unsafe-none COOP page should not trigger the isolation
  // heuristic and not require a dedicated process locked to coop.com.
  EXPECT_FALSE(coop_instance->RequiresDedicatedProcess());

  auto lock = coop_instance->GetProcess()->GetProcessLock();
  EXPECT_FALSE(lock.is_locked_to_site());

  // Open a noopener-allow-popups COOP same-site URL in a popup, which should
  // swap a BrowsingInstance because of noopener-allow-popups.  Verify that the
  // popup ends up in a different SiteInstance from the opener.
  GURL popup_url(https_server()->GetURL(
      "coop.com",
      "/set-header?Cross-Origin-Opener-Policy: noopener-allow-popups"));
  Shell* popup = OpenPopup(shell(), popup_url, "");
  RenderFrameHostImpl* popup_rfh = static_cast<RenderFrameHostImpl*>(
      popup->web_contents()->GetPrimaryMainFrame());
  EXPECT_EQ(popup_rfh->cross_origin_opener_policy().value,
            network::mojom::CrossOriginOpenerPolicyValue::kNoopenerAllowPopups);
  EXPECT_NE(popup_rfh->GetSiteInstance(), coop_instance);
  EXPECT_NE(popup_rfh->GetSiteInstance()->GetProcess(),
            coop_instance->GetProcess());

  // Navigate the popup to another non-isolated site, staying in the same
  // BrowsingInstance, and verify that it swaps to a new non-isolated
  // SiteInstance.  The non-isolated site has a child which is same-origin with
  // the COOP page; verify that it's placed in the same SiteInstance as the
  // COOP page, as they are allowed to synchronously script each other.
  GURL a_url(https_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a.com(coop.com)"));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, a_url));
  SiteInstanceImpl* new_instance = static_cast<SiteInstanceImpl*>(
      popup->web_contents()->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_FALSE(new_instance->RequiresDedicatedProcess());
  EXPECT_NE(new_instance, coop_instance);
  FrameTreeNode* popup_child =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetPrimaryFrameTree()
          .root()
          ->child_at(0);
  EXPECT_NE(popup_child->current_frame_host()->GetSiteInstance(),
            coop_instance);

  // Navigate the popup to coop.com again, staying in the same
  // BrowsingInstance, and verify that it goes back to the opener's
  // SiteInstance.
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, popup_url));
  EXPECT_NE(popup->web_contents()->GetPrimaryMainFrame()->GetSiteInstance(),
            coop_instance);
}

// Verify that COOP isolation applies at a site (and not origin) granularity.
//
// Isolating sites rather than origins may seem counterintuitive, considering
// the COOP header value that triggers isolation is "same-origin".  However,
// process isolation granularity that we can infer from COOP is quite different
// from what that actual COOP value controls. The COOP "same-origin" value
// specifies when to sever opener relationships and create a new
// BrowsingInstance; a COOP "same-origin" main frame document may only stay in
// the same BrowsingInstance as other same-origin COOP documents.  However,
// this does not apply to iframes, and it's possible to have a
// foo.bar.coop.com(baz.coop.com) hierarchy where the main frame has COOP
// "same-origin" but both frames set document.domain to coop.com and
// synchronously script each other (*).  Hence, in this case, we must isolate
// the coop.com site and place the two frames in the same process. This test
// covers that precise scenario.
//
// (*) In the future, COOP may disallow document.domain, in which case we may
// need to revisit this.  See https://github.com/whatwg/html/issues/6177.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, SiteGranularity) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate to a URL with COOP, where the origin doesn't match the site.
  GURL coop_url = https_server()->GetURL(
      "foo.bar.coop.com",
      "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  SiteInstanceImpl* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());

  // Ensure that the process lock is for the site, not origin.
  auto lock = coop_instance->GetProcess()->GetProcessLock();
  EXPECT_TRUE(lock.is_locked_to_site());
  EXPECT_EQ(ProcessLockFromUrl("https://coop.com"), lock);

  // Check that a same-site cross-origin subframe stays in the same
  // SiteInstance and process.
  ASSERT_TRUE(ExecJs(shell(),
                     "var iframe = document.createElement('iframe');"
                     "iframe.id = 'child';"
                     "document.body.appendChild(iframe);"));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  GURL c_url(https_server()->GetURL("baz.coop.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child", c_url));
  SiteInstanceImpl* child_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(coop_instance, child_instance);

  // Check that ChildProcessSecurityPolicy considers coop.com (and not its
  // subdomain) to be the matching isolated origin for `coop_url`.
  url::Origin matching_isolated_origin;
  ChildProcessSecurityPolicyImpl::GetInstance()
      ->GetMatchingProcessIsolatedOrigin(coop_instance->GetIsolationContext(),
                                         url::Origin::Create(GURL(coop_url)),
                                         false /* origin_requests_isolation */,
                                         &matching_isolated_origin);
  EXPECT_EQ(matching_isolated_origin,
            url::Origin::Create(GURL("https://coop.com")));
}

// Verify that COOP isolation applies when both COOP and COEP headers are set
// (i.e., for a cross-origin-isolated page).  This results in a different COOP
// header value (kSameOriginPlusCoep) which should still trigger isolation.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, COOPAndCOEP) {
  // Navigate to a URL with COOP + COEP.
  GURL coop_url = https_server()->GetURL(
      "coop.com",
      "/set-header?Cross-Origin-Opener-Policy: same-origin&"
      "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);

  // Make sure that site isolation for coop.com was triggered and that the
  // navigation ended up in a site-locked process.
  SiteInstanceImpl* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());
  auto lock = coop_instance->GetProcess()->GetProcessLock();
  EXPECT_TRUE(lock.GetWebExposedIsolationInfo().is_isolated());
  EXPECT_TRUE(lock.is_locked_to_site());
  EXPECT_TRUE(
      lock.MatchesOrigin(url::Origin::Create(GURL("https://coop.com"))));
}

// Check that when a site triggers both COOP isolation and OriginAgentCluster,
// both mechanisms take effect.  This test uses a URL with default ports so
// that we can exercise the site URL being the same with both COOP and OAC.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, COOPAndOriginAgentClusterNoPorts) {
  // Since the embedded test server only works for URLs with non-default ports,
  // use a URLLoaderInterceptor to mimic port-free operation.  This allows
  // checking the site URL being identical for both COOP and OAC isolation,
  // since otherwise OAC would include ports in the site URL.  The interceptor
  // below returns COOP and OAC headers for any page on foo.com, and returns a
  // simple test page without any headers for a.foo.com and b.foo.com.
  URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "foo.com") {
          const std::string headers =
              "HTTP/1.1 200 OK\n"
              "Content-Type: text/html\n"
              "Origin-Agent-Cluster: ?1\n"
              "Cross-Origin-Opener-Policy: same-origin\n";
          URLLoaderInterceptor::WriteResponse(
              "content/test/data" + params->url_request.url.path(),
              params->client.get(), &headers, std::optional<net::SSLInfo>());
          return true;
        } else if (params->url_request.url.host() == "a.foo.com" ||
                   params->url_request.url.host() == "b.foo.com") {
          URLLoaderInterceptor::WriteResponse("content/test/data/title1.html",
                                              params->client.get());
          return true;
        }
        // Not handled by us.
        return false;
      }));

  // Navigate to a URL with with COOP and OriginAgentCluster headers, embedding
  // two iframes at a.foo.com and b.foo.com.
  GURL coop_oac_url(
      "https://foo.com/cross_site_iframe_factory.html?"
      "foo.com(a.foo.com,b.foo.com)");
  EXPECT_TRUE(NavigateToURL(shell(), coop_oac_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  // The two subframes should end up in the same SiteInstance, different from
  // the main frame's SiteInstance.  Both SiteInstances should be in a process
  // dedicated to foo.com, but the main frame's process should be for
  // origin-keyed foo.com (strictly foo.com excluding subdomains) due to
  // Origin-Agent-Cluster, whereas the subframe process should be for
  // site-keyed foo.com.
  SiteInstanceImpl* main_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  SiteInstanceImpl* child_instance =
      child1->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(child_instance, child2->current_frame_host()->GetSiteInstance());
  EXPECT_NE(child_instance, main_instance);

  EXPECT_TRUE(main_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(child_instance->RequiresDedicatedProcess());

  EXPECT_TRUE(main_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_FALSE(child_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_EQ(main_instance->GetSiteInfo().site_url(),
            child_instance->GetSiteInfo().site_url());
  EXPECT_EQ(main_instance->GetSiteInfo().process_lock_url(),
            child_instance->GetSiteInfo().process_lock_url());

  auto main_lock = main_instance->GetProcess()->GetProcessLock();
  auto child_lock = child_instance->GetProcess()->GetProcessLock();
  EXPECT_TRUE(main_lock.is_locked_to_site());
  EXPECT_TRUE(child_lock.is_locked_to_site());
  EXPECT_TRUE(main_lock.is_origin_keyed_process());
  EXPECT_FALSE(child_lock.is_origin_keyed_process());
  auto foo_origin = url::Origin::Create(GURL("https://foo.com"));
  EXPECT_TRUE(main_lock.MatchesOrigin(foo_origin));
  EXPECT_TRUE(child_lock.MatchesOrigin(foo_origin));
}

// Check that when a site triggers both COOP isolation and OriginAgentCluster,
// both mechanisms take effect.  Similar to the test above, but starts on a URL
// where the origin doesn't match the site.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest,
                       COOPAndOriginAgentClusterOnSubdomain) {
  // Navigate to a URL with with COOP and OriginAgentCluster headers.
  GURL coop_oac_url = https_server()->GetURL(
      "oac.coop.com",
      "/set-header?Cross-Origin-Opener-Policy: same-origin&"
      "Origin-Agent-Cluster: ?1");
  EXPECT_TRUE(NavigateToURL(shell(), coop_oac_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);

  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();

  // Add a subframe and navigate to foo.coop.com.
  ASSERT_TRUE(ExecJs(shell(),
                     "var iframe = document.createElement('iframe');"
                     "iframe.id = 'child';"
                     "document.body.appendChild(iframe);"));
  FrameTreeNode* child = root->child_at(0);
  GURL child_url(https_server()->GetURL("foo.coop.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child", child_url));

  // The subframe should end up in a different SiteInstance from the main
  // frame's SiteInstance.  The main frame's SiteInstance should be in an
  // origin-keyed process locked to oac.foo.com, whereas the child's
  // SiteInstance should be in a site-keyed process locked to foo.com.
  SiteInstanceImpl* main_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  SiteInstanceImpl* child_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(child_instance, main_instance);

  EXPECT_TRUE(main_instance->RequiresDedicatedProcess());
  EXPECT_TRUE(child_instance->RequiresDedicatedProcess());

  EXPECT_TRUE(main_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_FALSE(child_instance->GetSiteInfo().requires_origin_keyed_process());
  EXPECT_NE(main_instance->GetSiteInfo().site_url(),
            child_instance->GetSiteInfo().site_url());
  EXPECT_NE(main_instance->GetSiteInfo().process_lock_url(),
            child_instance->GetSiteInfo().process_lock_url());

  auto main_lock = main_instance->GetProcess()->GetProcessLock();
  auto child_lock = child_instance->GetProcess()->GetProcessLock();
  EXPECT_TRUE(main_lock.is_locked_to_site());
  EXPECT_TRUE(child_lock.is_locked_to_site());
  EXPECT_TRUE(main_lock.is_origin_keyed_process());
  EXPECT_FALSE(child_lock.is_origin_keyed_process());
  auto oac_coop_origin = url::Origin::Create(coop_oac_url);
  auto coop_origin = url::Origin::Create(GURL("https://coop.com"));
  EXPECT_TRUE(main_lock.MatchesOrigin(oac_coop_origin));
  EXPECT_TRUE(child_lock.MatchesOrigin(coop_origin));
}

// Verify that if strict site isolation is in place, COOP isolation does not
// add redundant isolated origins to ChildProcessSecurityPolicy.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, SiteAlreadyRequiresDedicatedProcess) {
  // Enable --site-per-process and navigate to a COOP-enabled document.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  GURL coop_url = https_server()->GetURL(
      "coop.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  // Simulate user activation, which normally triggers COOP isolation for
  // future BrowsingInstances.
  EXPECT_TRUE(ExecJs(shell(), "// no-op"));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  SiteInstanceImpl* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();

  // The SiteInstance should require a dedicated process, but
  // ChildProcessSecurityPolicy shouldn't have added an isolated origin
  // for coop.com.
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  auto origins = policy->GetIsolatedOrigins(
      ChildProcessSecurityPolicy::IsolatedOriginSource::WEB_TRIGGERED);
  EXPECT_EQ(0U, origins.size());
  EXPECT_FALSE(policy->IsIsolatedOrigin(coop_instance->GetIsolationContext(),
                                        url::Origin::Create(coop_url),
                                        false /* origin_requests_isolation */));
}

// Verify that seeing a user activation on a COOP document triggers isolation
// of that document's site in future BrowsingInstances, but doesn't affect any
// existing BrowsingInstances.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, UserActivation) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL coop_url = https_server()->GetURL(
      "b.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  FrameTreeNode* coop_root = web_contents()->GetPrimaryFrameTree().root();
  SiteInstance* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  // The b.com COOP page should trigger the isolation heuristic and require a
  // dedicated process locked to b.com.
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());

  // At this point, the COOP page shouldn't have user activation.
  EXPECT_FALSE(coop_root->HasTransientUserActivation());

  // Create a new window, forcing a new BrowsingInstance, and check that b.com
  // is *not* isolated in it.  Since b.com in `coop_instance`'s
  // BrowsingInstance hasn't been interacted with, the COOP isolation does not
  // apply to other BrowsingInstances yet.
  Shell* shell2 = CreateBrowser();
  GURL no_coop_b_url = https_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(NavigateToURL(shell2, no_coop_b_url));
  FrameTreeNode* shell2_root =
      static_cast<WebContentsImpl*>(shell2->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  scoped_refptr<SiteInstance> instance2 =
      shell2->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_FALSE(instance2->RequiresDedicatedProcess());

  // Simulate a user activation in the original COOP page by running a dummy
  // script (ExecJs sends user activation by default).
  EXPECT_TRUE(ExecJs(coop_root, "// no-op"));
  EXPECT_TRUE(coop_root->HasTransientUserActivation());

  // Create a third window in a new BrowsingInstance and navigate it to a
  // non-COOP b.com URL. The above user activation should've forced COOP
  // isolation for b.com to apply to future BrowsingInstances, so check that
  // this navigation ends up requiring a dedicated process.
  Shell* shell3 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell3, no_coop_b_url));
  SiteInstance* instance3 =
      shell3->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(instance3->RequiresDedicatedProcess());
  EXPECT_FALSE(instance2->IsRelatedSiteInstance(instance3));
  EXPECT_FALSE(coop_instance->IsRelatedSiteInstance(instance3));

  // Ensure that the older BrowsingInstance in the second window wasn't
  // affected by the new isolation. Adding a b.com subframe or popup should
  // stay in the same SiteInstance. Navigating the popup out from and back to
  // b.com should also end up on the same SiteInstance.
  ASSERT_TRUE(ExecJs(shell2,
                     "var iframe = document.createElement('iframe');"
                     "iframe.id = 'child';"
                     "document.body.appendChild(iframe);",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  FrameTreeNode* child = shell2_root->child_at(0);
  GURL another_b_url(https_server()->GetURL("b.com", "/title3.html"));
  EXPECT_TRUE(
      NavigateIframeToURL(shell2->web_contents(), "child", another_b_url));
  SiteInstanceImpl* child_instance =
      child->current_frame_host()->GetSiteInstance();
  EXPECT_EQ(child_instance, instance2);

  Shell* popup = OpenPopup(shell2, another_b_url, "");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_EQ(popup_root->current_frame_host()->GetSiteInstance(), instance2);

  EXPECT_TRUE(NavigateToURLFromRenderer(
      popup, https_server()->GetURL("c.com", "/title1.html")));
  EXPECT_TRUE(NavigateToURLFromRenderer(popup, another_b_url));
  EXPECT_EQ(popup_root->current_frame_host()->GetSiteInstance(), instance2);

  // Close the popup.
  popup->Close();

  // Without any related windows, navigating to b.com in the second window's
  // main frame should trigger a proactive BrowsingInstance swap (see
  // ShouldSwapBrowsingInstancesForDynamicIsolation()), since we notice that
  // b.com would be isolated in a fresh BrowsingInstance, and nothing prevents
  // the BrowsingInstance swap. Hence, in that case, the navigation should be
  // in a new BrowsingInstance and in an isolated process.
  EXPECT_TRUE(NavigateToURLFromRenderer(
      shell2, https_server()->GetURL("b.com", "/title3.html")));
  scoped_refptr<SiteInstance> instance2_new =
      shell2->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(instance2_new->RequiresDedicatedProcess());
  EXPECT_NE(instance2_new, instance2);
  EXPECT_FALSE(instance2_new->IsRelatedSiteInstance(instance2.get()));
}

// Similar to the test above, but verify that a user activation on a same-site
// subframe also triggers isolation of a COOP site in the main frame for future
// BrowsingInstances.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, UserActivationInSubframe) {
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  GURL coop_url = https_server()->GetURL(
      "b.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  SiteInstance* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());

  // Add a cross-site subframe.
  ASSERT_TRUE(ExecJs(shell(),
                     "var iframe = document.createElement('iframe');"
                     "iframe.id = 'child';"
                     "document.body.appendChild(iframe);",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);
  GURL c_url(https_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child", c_url));

  EXPECT_FALSE(root->HasTransientUserActivation());
  EXPECT_FALSE(child->HasTransientUserActivation());

  // Simulate a user activation in the subframe by running a dummy script.
  EXPECT_TRUE(ExecJs(child, "// no-op"));
  EXPECT_TRUE(child->HasTransientUserActivation());

  // Since the iframe is cross-origin, it shouldn't trigger isolation of b.com
  // for future BrowsingInstances.
  GURL no_coop_b_url = https_server()->GetURL("b.com", "/title2.html");
  {
    Shell* new_shell = CreateBrowser();
    EXPECT_TRUE(NavigateToURL(new_shell, no_coop_b_url));
    scoped_refptr<SiteInstance> instance =
        new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
    EXPECT_FALSE(instance->RequiresDedicatedProcess());
  }

  // Now, make the iframe same-origin and simulate a user gesture.
  GURL b_url(https_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents(), "child", b_url));

  EXPECT_TRUE(ExecJs(child, "// no-op"));

  // Ensure that b.com is now isolated in a new tab and BrowsingInstance.
  {
    Shell* new_shell = CreateBrowser();
    EXPECT_TRUE(NavigateToURL(new_shell, no_coop_b_url));
    scoped_refptr<SiteInstance> instance =
        new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
    EXPECT_TRUE(instance->RequiresDedicatedProcess());
  }
}

// Similar to the test above, but verify that a user activation on a
// same-origin about:blank subframe triggers isolation of a COOP site in the
// main frame for future BrowsingInstances.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, UserActivationInAboutBlankSubframe) {
  GURL coop_url = https_server()->GetURL(
      "b.com", "/set-header?Cross-Origin-Opener-Policy: same-origin");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin);
  SiteInstance* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_TRUE(coop_instance->RequiresDedicatedProcess());

  // Add a cross-site blank subframe.
  ASSERT_TRUE(ExecJs(shell(),
                     "var iframe = document.createElement('iframe');"
                     "iframe.id = 'child';"
                     "document.body.appendChild(iframe);",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  FrameTreeNode* child = root->child_at(0);

  EXPECT_FALSE(root->HasTransientUserActivation());
  EXPECT_FALSE(child->HasTransientUserActivation());

  // Simulate a user activation in the subframe by running a dummy script.
  EXPECT_TRUE(ExecJs(child, "// no-op"));
  EXPECT_TRUE(child->HasTransientUserActivation());

  // Ensure that b.com is isolated in a new tab and BrowsingInstance.
  {
    Shell* new_shell = CreateBrowser();
    GURL no_coop_b_url = https_server()->GetURL("b.com", "/title2.html");
    EXPECT_TRUE(NavigateToURL(new_shell, no_coop_b_url));
    scoped_refptr<SiteInstance> instance =
        new_shell->web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
    EXPECT_TRUE(instance->RequiresDedicatedProcess());
  }
}

// Ensure that navigating to http://localhost which has COOP+COEP headers, and
// hence will attempt to trigger COOP isolation, will not crash.  See
// https://crbug.com/1276155.
IN_PROC_BROWSER_TEST_F(COOPIsolationTest, Localhost) {
  // Navigate to a URL with COOP + COEP on http://localhost.
  GURL coop_url = https_server()->GetURL(
      "localhost",
      "/set-header?Cross-Origin-Opener-Policy: same-origin&"
      "Cross-Origin-Embedder-Policy: require-corp");
  EXPECT_TRUE(NavigateToURL(shell(), coop_url));
  EXPECT_EQ(
      web_contents()->GetPrimaryMainFrame()->cross_origin_opener_policy().value,
      network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep);

  // http://localhost isn't currently considered a valid isolated origin (since
  // it won't work for subdomain matching), so the navigation should not
  // trigger site isolation.  Note, however, that the process lock should still
  // reflect COOP+COEP isolation.
  SiteInstanceImpl* coop_instance =
      web_contents()->GetPrimaryMainFrame()->GetSiteInstance();
  EXPECT_FALSE(coop_instance->RequiresDedicatedProcess());
  auto lock = coop_instance->GetProcess()->GetProcessLock();
  EXPECT_TRUE(lock.GetWebExposedIsolationInfo().is_isolated());
  EXPECT_FALSE(lock.is_locked_to_site());
}

// Helper class for testing site isolation triggered by different JIT policies
// being applied.
class JITIsolationTest : public IsolatedOriginTest,
                         public ::testing::WithParamInterface<bool> {
 public:
  JITIsolationTest() = default;

  ~JITIsolationTest() override = default;

  // A custom ContentBrowserTestContentBrowserClient to selectively turn off JIT
  // for certain sites.
  class JitContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    JitContentBrowserClient(bool jit_disabled_default,
                            bool disable_site_isolation_entirely)
        : is_jit_disabled_by_default_(jit_disabled_default),
          is_site_isolation_disabled_entirely_(
              disable_site_isolation_entirely) {}

    bool IsJitDisabledForSite(BrowserContext* browser_context,
                              const GURL& site_url) override {
      if (site_url.is_empty()) {
        return is_jit_disabled_by_default_;
      }
      if (site_url.DomainIs("jit-disabled.com")) {
        return true;
      }
      if (site_url.DomainIs("jit-enabled.com")) {
        return false;
      }
      return is_jit_disabled_by_default_;
    }

    bool ShouldEnableStrictSiteIsolation() override {
      return !is_site_isolation_disabled_entirely_;
    }

   private:
    bool is_jit_disabled_by_default_;
    bool is_site_isolation_disabled_entirely_;
  };
};

IN_PROC_BROWSER_TEST_P(JITIsolationTest, MainFrameTest) {
  bool jit_disabled_by_default = GetParam();
  JitContentBrowserClient policy(jit_disabled_by_default,
                                 /* disable_site_isolation_entirely */ false);

  // Navigate to jit-disabled.com which should always have JIT disabled.
  GURL disabled_url(
      embedded_test_server()->GetURL("www.jit-disabled.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), disabled_url));

  EXPECT_TRUE(shell()
                  ->web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsJitDisabled());

  // Navigate to jit-enabled.com which should always have JIT enabled.
  GURL enabled_url(
      embedded_test_server()->GetURL("www.jit-enabled.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), enabled_url));

  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetPrimaryMainFrame()
                   ->GetProcess()
                   ->IsJitDisabled());

  // Navigate to a site with no policy and it should match the default.
  GURL default_url(
      embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), default_url));

  EXPECT_EQ(jit_disabled_by_default, shell()
                                         ->web_contents()
                                         ->GetPrimaryMainFrame()
                                         ->GetProcess()
                                         ->IsJitDisabled());
}

IN_PROC_BROWSER_TEST_P(JITIsolationTest, DefaultSiteTest) {
  // Skip the test if --site-per-process is used on the command line, as the
  // test needs to run without strict site isolation (see
  // JitContentBrowserClient below).
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  bool jit_disabled_by_default = GetParam();
  JitContentBrowserClient policy(jit_disabled_by_default,
                                 /* disable_site_isolation_entirely */ true);

  // All three sites should have JIT enabled or disabled together, if site
  // isolation is disabled, since they are all put into the default
  // SiteInstance.
  GURL disabled_url(
      embedded_test_server()->GetURL("www.jit-disabled.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), disabled_url));

  EXPECT_EQ(jit_disabled_by_default, shell()
                                         ->web_contents()
                                         ->GetPrimaryMainFrame()
                                         ->GetProcess()
                                         ->IsJitDisabled());

  GURL enabled_url(
      embedded_test_server()->GetURL("www.jit-enabled.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), enabled_url));

  EXPECT_EQ(jit_disabled_by_default, shell()
                                         ->web_contents()
                                         ->GetPrimaryMainFrame()
                                         ->GetProcess()
                                         ->IsJitDisabled());

  GURL default_url(
      embedded_test_server()->GetURL("www.foo.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), default_url));

  EXPECT_EQ(jit_disabled_by_default, shell()
                                         ->web_contents()
                                         ->GetPrimaryMainFrame()
                                         ->GetProcess()
                                         ->IsJitDisabled());
}

INSTANTIATE_TEST_SUITE_P(JITEnabledByDefault,
                         JITIsolationTest,
                         ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(JITDisabledByDefault,
                         JITIsolationTest,
                         ::testing::Values(true));

IN_PROC_BROWSER_TEST_F(JITIsolationTest, SubFrameTest) {
  // Set JIT to be enabled by default.
  JitContentBrowserClient policy(
      /* jit_disabled_default */ false,
      /* disable_site_isolation_entirely */ false);

  GURL default_embeds_disabled(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(jit-disabled.com)"));

  EXPECT_TRUE(NavigateToURL(shell(), default_embeds_disabled));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(shell()->web_contents()).size());

  // Top frame 'foo.com' should have JIT enabled as that's the default.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_FALSE(root->current_frame_host()->GetProcess()->IsJitDisabled());
  // The frame containing jit-disabled.com should have JIT disabled.
  FrameTreeNode* child_frame_node = root->child_at(0);
  EXPECT_TRUE(
      child_frame_node->current_frame_host()->GetProcess()->IsJitDisabled());

  // And the other way round, where jit-disabled.com embeds foo.com.
  GURL disabled_embeds_default(embedded_test_server()->GetURL(
      "jit-disabled.com",
      "/cross_site_iframe_factory.html?jit-disabled.com(foo.com)"));

  EXPECT_TRUE(NavigateToURL(shell(), disabled_embeds_default));
  EXPECT_EQ(2u, CollectAllRenderFrameHosts(
                    shell()->web_contents()->GetPrimaryMainFrame())
                    .size());

  // Top frame 'jit-disabled.com' should have JIT disabled.
  root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_TRUE(root->current_frame_host()->GetProcess()->IsJitDisabled());
  // The frame containing foo.com should have JIT enabled as that's the default.
  child_frame_node = root->child_at(0);
  EXPECT_FALSE(
      child_frame_node->current_frame_host()->GetProcess()->IsJitDisabled());
}

// Check that jitless subframes obey process reuse policies.
IN_PROC_BROWSER_TEST_F(JITIsolationTest, SubFrameProcessReuse) {
  // Set JIT to be enabled by default.
  JitContentBrowserClient policy(
      /* jit_disabled_default */ false,
      /* disable_site_isolation_entirely */ false);

  GURL default_embeds_disabled(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(jit-disabled.com)"));

  EXPECT_TRUE(NavigateToURL(shell(), default_embeds_disabled));

  // Top frame 'foo.com' should have JIT enabled as that's the default.
  FrameTreeNode* root = web_contents()->GetPrimaryFrameTree().root();
  EXPECT_FALSE(root->current_frame_host()->GetProcess()->IsJitDisabled());
  // The frame containing jit-disabled.com should have JIT disabled.
  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(child->current_frame_host()->GetProcess()->IsJitDisabled());

  // Create a new window, unrelated to the current one, and set up the same
  // frame hierarchy.
  Shell* new_shell = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(new_shell, default_embeds_disabled));

  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetPrimaryFrameTree()
          .root();
  EXPECT_FALSE(new_root->current_frame_host()->GetProcess()->IsJitDisabled());

  FrameTreeNode* new_child = new_root->child_at(0);
  EXPECT_TRUE(new_child->current_frame_host()->GetProcess()->IsJitDisabled());

  // The subframes should be in separate BrowsingInstances, but because they
  // have the same site, they should share the same process.
  EXPECT_FALSE(
      new_child->current_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
          child->current_frame_host()->GetSiteInstance()));
  EXPECT_EQ(new_child->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

}  // namespace content
