// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

class MockContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  bool ShouldDisableOriginAgentClusterDefault(BrowserContext*) override {
    return should_disable_origin_agent_cluster_default_;
  }

  bool should_disable_origin_agent_cluster_default_ = false;
};

}  // anonymous namespace

// Test the effect of the OriginAgentCluster: header on document.domain
// settability and how it (doesn't) affect process assignment.
class OriginAgentClusterBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OriginAgentClusterBrowserTest()
      : OriginAgentClusterBrowserTest(
            /* origin_cluster_default_enabled */ false,
            /* secure_context */ true) {}
  ~OriginAgentClusterBrowserTest() override = default;

  void SetUp() override {
    mock_cert_verifier_.SetUpCommandLine(
        base::CommandLine::ForCurrentProcess());

    ContentBrowserTest::SetUp();
  }

  static std::string OriginIsolationParamToString(
      const ::testing::TestParamInfo<bool>& info) {
    return info.param ? "OriginKeyedProcessesByDefaultEnabled"
                      : "OriginKeyedProcessesByDefaultDisabled";
  }

  bool AreOriginKeyedProcessesEnabled() { return GetParam(); }

 protected:
  enum OriginAgentClusterState { kUnset, kSetTrue, kSetFalse, kMalformed };

  OriginAgentClusterBrowserTest(bool origin_cluster_default_enabled,
                                bool secure_context)
      : server_(net::EmbeddedTestServer::TYPE_HTTPS),
        origin_cluster_default_enabled_(origin_cluster_default_enabled),
        secure_context_(secure_context) {
    server_.AddDefaultHandlers(GetTestDataFilePath());

    // InitWithFeatures needs to be called in the constructor in multi-threaded
    // tests.
    std::vector<base::test::FeatureRef> enabled, disabled;
    (origin_cluster_default_enabled_ ? enabled : disabled)
        .push_back(blink::features::kOriginAgentClusterDefaultEnabled);
    if (AreOriginKeyedProcessesEnabled()) {
      enabled.push_back(features::kOriginKeyedProcessesByDefault);
      // This is needed because Android builds do not have site-per-process
      // enabled by default.
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kSitePerProcess);
    } else {
      disabled.push_back(features::kOriginKeyedProcessesByDefault);
    }
    features_.InitWithFeatures(enabled, disabled);
  }

  void SetUpOnMainThread() override {
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(server());
    ASSERT_TRUE(server()->Start());
    browser_client_ = std::make_unique<MockContentBrowserClient>();
  }

  void TearDownOnMainThread() override { browser_client_.reset(); }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  net::EmbeddedTestServer* server() {
    return secure_context_ ? &server_ : embedded_test_server();
  }

  RenderProcessHost* NavigateAndGetProcess(const char* domain,
                                           OriginAgentClusterState oac_state) {
    DCHECK(domain);
    WebContents* contents = Navigate(domain, oac_state);
    DCHECK(contents);
    return static_cast<WebContentsImpl*>(contents)
        ->GetPrimaryMainFrame()
        ->GetProcess();
  }

  std::string GetSameSiteCrossOriginDomain(
      const std::string& original_hostname) {
    std::string registrable_domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            original_hostname,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    // Ensure the new domain is cross-origin but same-site.
    std::string new_domain = "alt." + registrable_domain;

    // Avoid accidentally using the same subdomain.
    if (new_domain == original_hostname) {
      new_domain = "othersub." + registrable_domain;
    }

    return new_domain;
  }

  bool CanDocumentDomain(std::string from,
                         std::string to,
                         OriginAgentClusterState oac_state) {
    // This initial navigation is needed to do a browsing instance swap, which
    // will ensure that the test's actual navigation swaps to a fresh
    // BrowsingInstance. This is so the correct MockContentBrowserClient is
    // installed before that BrowsingInstance is created and looks up OAC
    // isolation policy.
    EXPECT_TRUE(NavigateToURL(
        shell(), server()->GetURL("othersite.com", "/empty.html")));

    WebContents* contents = Navigate(from, oac_state);
    DCHECK(contents);

    // Create a new cross-origin same-site pop-up window that has an OAC header
    // set to the same value as its opener.
    Shell* popup_shell =
        OpenPopup(contents->GetPrimaryMainFrame(),
                  server()->GetURL(GetSameSiteCrossOriginDomain(from),
                                   ClusterStateToRelativeURL(oac_state)),
                  "");
    EXPECT_TRUE(popup_shell);
    WebContents* popup_contents = popup_shell->web_contents();
    DCHECK(popup_contents);

    // Set both document's domains to the same domain.
    EXPECT_TRUE(ExecJs(contents, SetDocumentDomainTo(to)));
    EXPECT_TRUE(ExecJs(popup_contents, SetDocumentDomainTo(to)));

    // Attempt to script the popup page's opener.
    auto result = EvalJs(popup_contents,
                         "try { "
                         "  window.opener.location.href; "
                         "} catch (e) { "
                         "  'error'; "
                         "}");
    if (result.ExtractString() == "error") {
      return false;
    }

    return result.ExtractString() ==
           EvalJs(contents, "window.location.href").ExtractString();
  }

  // This tries to set document.domain. But instead of checking whether setting
  // succeeded (which is what CanDocumentDomain above does), it checks whether
  // a warning message is posted to the console.
  bool CanDocumentDomainMessage(std::string from,
                                std::string to,
                                OriginAgentClusterState oac_state) {
    WebContents* contents = Navigate(from, oac_state);
    DCHECK(contents);

    WebContentsConsoleObserver console(contents);
    console.SetPattern("document.domain mutation is ignored*");
    CHECK(ExecJs(contents, SetDocumentDomainTo(to)));
    return !console.messages().size();
  }

  // Simulate setting the OriginAgentClusterDefaultEnabled enterprise policy.
  void SetEnterprisePolicy(bool value) {
    // Note that the enterprise policy has different 'polarity', and true
    // means Chromium picks the default and false is legacy behaviour, while
    // ContentBrowserClientShould::DisableOriginAgentClusterDefault is a
    // disable switch, meaning that false means Chromium picks the default
    // and true is legacy behaviour.
    browser_client_->should_disable_origin_agent_cluster_default_ = !value;
  }

 private:
  std::string SetDocumentDomainTo(std::string to) const {
    // Assign document.domain and check whether it changed.
    // Wrap the statement in a try-catch, since since document.domain setting
    // may throw.
    return JsReplace(
        "try { "
        "document.domain = $1; "
        "document.domain == $1; "
        "} catch (e) { false; }",
        to);
  }

  WebContents* Navigate(std::string domain, std::string path) {
    GURL url(server()->GetURL(domain, path));
    EXPECT_TRUE(NavigateToURL(shell(), url));
    return shell()->web_contents();
  }

  std::string ClusterStateToRelativeURL(OriginAgentClusterState state) {
    switch (state) {
      case kUnset:
        return "/empty.html";
      case kSetTrue:
        return "/set-header?Origin-Agent-Cluster: ?1";
      case kSetFalse:
        return "/set-header?Origin-Agent-Cluster: ?0";
      case kMalformed:
        return "/set-header?Origin-Agent-Cluster: potato";
    }
  }

  // For the purpose of this test, we only care about the Origin-Agent-Cluster:
  // header. The test setup has four pages corresponding to the three valid
  // states (absent, true ("?1"), and false ("?0"), and one malformed one
  // ("?2"). For ease of use, we have an enum to designate the appropriate
  // test page.
  WebContents* Navigate(std::string domain,
                        const OriginAgentClusterState state) {
    return Navigate(domain, ClusterStateToRelativeURL(state));
  }

 protected:
  // https:-embedded test server.
  // The BrowserTestBase::embedded_test_server_ is a private member and is
  // constructed as http:-only, and so we cannot change or replace it.
  // The setup of server_ emulates that of embedded_test_server_.
  net::EmbeddedTestServer server_;
  content::ContentMockCertVerifier mock_cert_verifier_;

  std::unique_ptr<MockContentBrowserClient> browser_client_;

  const bool origin_cluster_default_enabled_;
  const bool secure_context_;
  base::test::ScopedFeatureList features_;
};

// Test fixture wih the default behaviour change enabled.
// (blink::features::kOriginAgentClusterDefaultEnabled)
class OriginAgentClusterEnabledBrowserTest
    : public OriginAgentClusterBrowserTest {
 public:
  OriginAgentClusterEnabledBrowserTest()
      : OriginAgentClusterBrowserTest(
            /* origin_cluster_default_enabled */ true,
            /* secure_context */ true) {}
  ~OriginAgentClusterEnabledBrowserTest() override = default;
};

// Test fixture wih the default behaviour change enabled, but using an insecure
// context. (blink::features::kOriginAgentClusterDefaultEnabled + http:)
class OriginAgentClusterInsecureEnabledBrowserTest
    : public OriginAgentClusterBrowserTest {
 public:
  OriginAgentClusterInsecureEnabledBrowserTest()
      : OriginAgentClusterBrowserTest(
            /* origin_cluster_default_enabled */ true,
            /* secure_context */ false) {}
  ~OriginAgentClusterInsecureEnabledBrowserTest() override = default;
};

// DocumentDomain: Can we set document.domain?
//
// Tests are for each Origin-Agent-Cluster: header state
// (enabled/disabled/default/malformed), and flag being enabled/disabled.
//
// These tests ensure that the flag will change the default behaviour only.

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, DocumentDomain_Default) {
  EXPECT_EQ(CanDocumentDomain("a.domain.test", "domain.test", kUnset),
            !AreOriginKeyedProcessesEnabled());
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, DocumentDomain_Enabled) {
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, DocumentDomain_Disabled) {
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest,
                       DocumentDomain_Malformed) {
  EXPECT_EQ(CanDocumentDomain("a.domain.test", "domain.test", kMalformed),
            !AreOriginKeyedProcessesEnabled());
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       DocumentDomain_Default) {
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kUnset));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       DocumentDomain_Enabled) {
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       DocumentDomain_Disabled) {
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       DocumentDomain_Malformed) {
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kMalformed));
}

// Process: Will two pages (same site, different origin) be assigned to the
// same process?
//
// Tests are for each Origin-Agent-Cluster: header state
// (enabled/disabled/default/malformed), and the flag being enabled/disabled.
//
// These tests mainly ensure that the enabled-flag will not actually change
// this behaviour, since we use same-process clustering. (Unlike some earlier
// plans, where we were trying to change the process model as well.)

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, SameProcess_Default) {
  if (AreOriginKeyedProcessesEnabled()) {
    EXPECT_NE(NavigateAndGetProcess("a.domain.test", kUnset),
              NavigateAndGetProcess("b.domain.test", kUnset));
  } else {
    EXPECT_EQ(NavigateAndGetProcess("a.domain.test", kUnset),
              NavigateAndGetProcess("b.domain.test", kUnset));
  }
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, SameProcess_Enabled) {
  EXPECT_NE(NavigateAndGetProcess("a.domain.test", kSetTrue),
            NavigateAndGetProcess("b.domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, SameProcess_Disabled) {
  EXPECT_EQ(NavigateAndGetProcess("a.domain.test", kSetFalse),
            NavigateAndGetProcess("b.domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, SameProcess_Malformed) {
  if (AreOriginKeyedProcessesEnabled()) {
    EXPECT_NE(NavigateAndGetProcess("a.domain.test", kMalformed),
              NavigateAndGetProcess("b.domain.test", kMalformed));
  } else {
    EXPECT_EQ(NavigateAndGetProcess("a.domain.test", kMalformed),
              NavigateAndGetProcess("b.domain.test", kMalformed));
  }
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       SameProcess_Default) {
  if (AreOriginKeyedProcessesEnabled()) {
    EXPECT_NE(NavigateAndGetProcess("a.domain.test", kUnset),
              NavigateAndGetProcess("b.domain.test", kUnset));
  } else {
    EXPECT_EQ(NavigateAndGetProcess("a.domain.test", kUnset),
              NavigateAndGetProcess("b.domain.test", kUnset));
  }
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       SameProcess_Enabled) {
  EXPECT_NE(NavigateAndGetProcess("a.domain.test", kSetTrue),
            NavigateAndGetProcess("b.domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       SameProcess_Disabled) {
  EXPECT_EQ(NavigateAndGetProcess("a.domain.test", kSetFalse),
            NavigateAndGetProcess("b.domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       SameProcess_Malformed) {
  if (AreOriginKeyedProcessesEnabled()) {
    EXPECT_NE(NavigateAndGetProcess("a.domain.test", kMalformed),
              NavigateAndGetProcess("b.domain.test", kMalformed));
  } else {
    EXPECT_EQ(NavigateAndGetProcess("a.domain.test", kMalformed),
              NavigateAndGetProcess("b.domain.test", kMalformed));
  }
}

// WarningMessage: Test whether setting document.domain triggers a console
// message, for each Origin-Agent-Cluster: header state
// (enabled/disabled/default/malformed), and each flag (none/enable/message).

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, WarningMessage_Default) {
  EXPECT_EQ(CanDocumentDomainMessage("a.domain.test", "domain.test", kUnset),
            !AreOriginKeyedProcessesEnabled());
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, WarningMessage_Enabled) {
  EXPECT_FALSE(
      CanDocumentDomainMessage("a.domain.test", "domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest, WarningMessage_Disabled) {
  EXPECT_TRUE(
      CanDocumentDomainMessage("a.domain.test", "domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterBrowserTest,
                       WarningMessage_Malformed) {
  EXPECT_EQ(
      CanDocumentDomainMessage("a.domain.test", "domain.test", kMalformed),
      !AreOriginKeyedProcessesEnabled());
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       WarningMessage_Default) {
  EXPECT_FALSE(
      CanDocumentDomainMessage("a.domain.test", "domain.test", kUnset));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       WarningMessage_Enabled) {
  EXPECT_FALSE(
      CanDocumentDomainMessage("a.domain.test", "domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       WarningMessage_Disabled) {
  EXPECT_TRUE(
      CanDocumentDomainMessage("a.domain.test", "domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       WarningMessage_Malformed) {
  EXPECT_FALSE(
      CanDocumentDomainMessage("a.domain.test", "domain.test", kMalformed));
}

// Policy: Ensure that the legacy behaviour remains if the appropriate
// enterprise policy is set.
//
// (The case without policy is adequately covered by the tests above, since
// none of them modify the policy.)

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetTrue_Default) {
  SetEnterprisePolicy(false);
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kUnset));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetFalse_Default) {
  SetEnterprisePolicy(true);
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kUnset));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetTrue_Enabled) {
  SetEnterprisePolicy(false);
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetFalse_Enabled) {
  SetEnterprisePolicy(true);
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kSetTrue));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetTrue_Disabled) {
  SetEnterprisePolicy(false);
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetFalse_Disabled) {
  SetEnterprisePolicy(true);
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetTrue_Malformed) {
  SetEnterprisePolicy(false);
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kMalformed));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       Policy_SetFalse_Malformed) {
  SetEnterprisePolicy(true);
  EXPECT_FALSE(CanDocumentDomain("a.domain.test", "domain.test", kMalformed));
}

// Make sure that the default isolation state is correctly generated with
// respect to the enterprise policy.
IN_PROC_BROWSER_TEST_P(OriginAgentClusterEnabledBrowserTest,
                       DefaultIsolationStateEnterprisePolicyTest) {
  // OAC by default not disabled by enterprise policy.
  SetEnterprisePolicy(true);
  {
    OriginAgentClusterIsolationState default_isolation_state =
        OriginAgentClusterIsolationState::CreateForDefaultIsolation(
            shell()->web_contents()->GetBrowserContext());
    EXPECT_TRUE(default_isolation_state.is_origin_agent_cluster());
  }
  // OAC by default disabled by enterprise policy.
  SetEnterprisePolicy(false);
  {
    OriginAgentClusterIsolationState default_isolation_state =
        OriginAgentClusterIsolationState::CreateForDefaultIsolation(
            shell()->web_contents()->GetBrowserContext());
    EXPECT_FALSE(default_isolation_state.is_origin_agent_cluster());
  }
}

// Test that the legacy behaviour continues working in insecure contexts.
//
// Origin-Agent-Cluster: is supposed to be a "powerful feature", which is
// restricted to secure contexts. But disabling it is certainly not powerful,
// and is specifically geared towards legacy applications, which means we
// should ensure that one can still set document.domain in an insecure context
// when Origin-Agent-Cluster: ?0 is set.

IN_PROC_BROWSER_TEST_P(OriginAgentClusterInsecureEnabledBrowserTest,
                       DocumentDomain_Default) {
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kUnset));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterInsecureEnabledBrowserTest,
                       DocumentDomain_Disabled) {
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kSetFalse));
}

IN_PROC_BROWSER_TEST_P(OriginAgentClusterInsecureEnabledBrowserTest,
                       DocumentDomain_Malformed) {
  EXPECT_TRUE(CanDocumentDomain("a.domain.test", "domain.test", kMalformed));
}

// We are (deliberately) not testing the kSetTrue case in this set of tests,
// since that is bound to a secure context. But the disable cases should
// continue to remain compatible with legacy behaviour.

INSTANTIATE_TEST_SUITE_P(
    All,
    OriginAgentClusterBrowserTest,
    ::testing::Bool(),
    OriginAgentClusterBrowserTest::OriginIsolationParamToString);
INSTANTIATE_TEST_SUITE_P(
    All,
    OriginAgentClusterEnabledBrowserTest,
    ::testing::Bool(),
    OriginAgentClusterBrowserTest::OriginIsolationParamToString);
INSTANTIATE_TEST_SUITE_P(
    All,
    OriginAgentClusterInsecureEnabledBrowserTest,
    ::testing::Bool(),
    OriginAgentClusterBrowserTest::OriginIsolationParamToString);

}  // namespace content
