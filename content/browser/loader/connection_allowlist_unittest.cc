// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/connection_allowlist_utils.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_navigation_url_loader.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr char kConnectionAllowlistEnabledUrl[] = "https://example.com";

class PreconnectTestContentBrowserClient : public ContentBrowserClient {
 public:
  PreconnectTestContentBrowserClient()
      : old_browser_client_(SetBrowserClientForTesting(this)) {}
  ~PreconnectTestContentBrowserClient() override {
    SetBrowserClientForTesting(old_browser_client_);
  }

  bool ShouldPreconnectNavigation(RenderFrameHost* render_frame_host) override {
    return true;
  }

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

}  // namespace

class ConnectionAllowlistTest : public RenderViewHostImplTestHarness {
 public:
  ConnectionAllowlistTest() = default;
  ~ConnectionAllowlistTest() override = default;

  const PolicyContainerPolicies& GetPolicyContainerPolicies(
      const RenderFrameHost* rfh) const {
    return static_cast<const RenderFrameHostImpl*>(rfh)
        ->policy_container_host()
        ->policies();
  }

  bool HasConnectionAllowlist(const RenderFrameHost* rfh) const {
    const PolicyContainerPolicies& policies = GetPolicyContainerPolicies(rfh);
    return policies.connection_allowlists.enforced.has_value() ||
           policies.connection_allowlists.report_only.has_value();
  }

  void AbortPendingErrorPageCommit() {
    TestRenderFrameHost* rfh = main_test_rfh();
    if (rfh->navigation_requests().empty()) {
      rfh = static_cast<TestRenderFrameHost*>(
          rfh->frame_tree_node()->render_manager()->speculative_frame_host());
    }
    ASSERT_TRUE(rfh);
    ASSERT_FALSE(rfh->navigation_requests().empty());
    rfh->AbortCommit(rfh->navigation_requests().begin()->first);
  }

 private:
  blink::ScopedTestOriginTrialPolicy scoped_test_origin_trial_policy_;
};

// The base::Feature as a kill switch should disable the connection allowlist.
TEST_F(ConnectionAllowlistTest, BaseFeatureKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// When there is a copy of the policy container, the connection allowlist stored
// in the policy container is also copied unconditionally. This is expected
// behavior for connection Allowlists to check that creating an empty or local
// scheme iframe should not be used as a workaround to the network restrictions
// of the parent frame.
TEST_F(ConnectionAllowlistTest, MainFrameCreatesEmptyIframeInheritsAllowlist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_TRUE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));

  // The main frame creates an iframe.
  RenderFrameHost* main_frame = navigation->GetFinalRenderFrameHost();
  RenderFrameHostTester* main_frame_tester =
      RenderFrameHostTester::For(main_frame);
  RenderFrameHost* child_rfh = main_frame_tester->AppendChild("child");

  // Verify the iframe enables the connection allowlist.
  EXPECT_TRUE(HasConnectionAllowlist(child_rfh));
}

class ConnectionAllowlistTestNetworkContext
    : public network::TestNetworkContext {
 public:
  ConnectionAllowlistTestNetworkContext() = default;

  struct Report {
    std::string type;
    std::string group;
    GURL url;
    std::optional<base::UnguessableToken> reporting_source;
    net::NetworkAnonymizationKey network_anonymization_key;
    base::DictValue body;
  };

  const std::vector<Report>& reports() const { return reports_; }
  void ClearReports() { reports_.clear(); }
  const std::vector<std::vector<base::UnguessableToken>>&
  cleared_network_restrictions_calls() const {
    return cleared_network_restrictions_calls_;
  }
  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::PendingRemote<network::mojom::NetworkContext>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void QueueReport(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::DictValue body) override {
    reports_.push_back({type, group, url, reporting_source,
                        network_anonymization_key, std::move(body)});
  }

  void RestrictNetworkForIds(
      std::vector<network::mojom::IdAndAllowlistedPatternsPtr>
          ids_to_allowlisted_patterns,
      RestrictNetworkForIdsCallback callback) override {
    std::move(callback).Run();
  }

  void ClearNetworkRestrictions(const std::vector<base::UnguessableToken>&
                                    network_restrictions_ids) override {
    cleared_network_restrictions_calls_.push_back(network_restrictions_ids);
  }

 private:
  std::vector<Report> reports_;
  std::vector<std::vector<base::UnguessableToken>>
      cleared_network_restrictions_calls_;
  mojo::Receiver<network::mojom::NetworkContext> receiver_{this};
};

TEST(ConnectionAllowlistReportingTest, ReportsViolations) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  ConnectionAllowlistTestNetworkContext network_context;
  net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(GURL("https://example.com")));
  base::UnguessableToken reporting_source = base::UnguessableToken::Create();

  PolicyContainerPolicies policies;
  policies.connection_allowlists.response_url =
      GURL("https://example.com/page");
  policies.connection_allowlists.reporting_source = reporting_source;

  network::ConnectionAllowlist enforced;
  enforced.allowlist = {"https://allowed.com"};
  enforced.reporting_endpoint = "endpoint-group";
  enforced.redirect_behavior =
      network::ConnectionAllowlist::RedirectBehavior::kBlock;
  policies.connection_allowlists.enforced = enforced;

  // Test IsRedirectAllowedByConnectionAllowlist
  GURL original_url("https://blocked.com/redirect");
  EXPECT_FALSE(IsRedirectAllowedByConnectionAllowlist(
      policies, original_url, &network_context, network_anonymization_key,
      reporting_source));

  ASSERT_EQ(network_context.reports().size(), 1u);
  EXPECT_EQ(network_context.reports()[0].type, "connection-allowlist");
  EXPECT_EQ(network_context.reports()[0].group, "endpoint-group");
  EXPECT_EQ(network_context.reports()[0].url, GURL("https://example.com/page"));
  EXPECT_EQ(network_context.reports()[0].reporting_source, reporting_source);
  EXPECT_EQ(network_context.reports()[0].network_anonymization_key,
            network_anonymization_key);
  EXPECT_EQ(*network_context.reports()[0].body.FindString("connection"),
            "https://blocked.com/redirect");
  EXPECT_EQ(*network_context.reports()[0].body.FindString("disposition"),
            "enforce");

  // Test ConnectionAllowlistAllowsUrlAndReportIfNeeded
  GURL allowed_url("https://allowed.com/path");
  EXPECT_TRUE(ConnectionAllowlistAllowsUrlAndReportIfNeeded(
      policies, allowed_url, &network_context, network_anonymization_key,
      reporting_source));
  EXPECT_EQ(network_context.reports().size(), 1u);  // No new report

  GURL blocked_url("https://blocked.com/path");
  EXPECT_FALSE(ConnectionAllowlistAllowsUrlAndReportIfNeeded(
      policies, blocked_url, &network_context, network_anonymization_key,
      reporting_source));
  ASSERT_EQ(network_context.reports().size(), 2u);
  EXPECT_EQ(network_context.reports()[1].type, "connection-allowlist");
  EXPECT_EQ(network_context.reports()[1].group, "endpoint-group");
  EXPECT_EQ(network_context.reports()[1].url, GURL("https://example.com/page"));
  EXPECT_EQ(network_context.reports()[1].reporting_source, reporting_source);
  EXPECT_EQ(network_context.reports()[1].network_anonymization_key,
            network_anonymization_key);
  EXPECT_EQ(*network_context.reports()[1].body.FindString("connection"),
            "https://blocked.com/path");
  EXPECT_EQ(*network_context.reports()[1].body.FindString("disposition"),
            "enforce");

  // When `network_context` is nullptr (e.g., during navigation preconnect
  // checks or history navigations), enforcement still applies without queueing
  // reports.
  network::ConnectionAllowlist report_only;
  report_only.allowlist = {"https://allowed.com"};
  report_only.reporting_endpoint = "endpoint-group";
  report_only.redirect_behavior =
      network::ConnectionAllowlist::RedirectBehavior::kBlock;
  policies.connection_allowlists.report_only = report_only;

  EXPECT_FALSE(IsRedirectAllowedByConnectionAllowlist(
      policies, original_url, /*network_context=*/nullptr,
      network_anonymization_key, reporting_source));
  EXPECT_TRUE(ConnectionAllowlistAllowsUrlAndReportIfNeeded(
      policies, allowed_url, /*network_context=*/nullptr,
      network_anonymization_key, reporting_source));
  EXPECT_FALSE(ConnectionAllowlistAllowsUrlAndReportIfNeeded(
      policies, blocked_url, /*network_context=*/nullptr,
      network_anonymization_key, reporting_source));
  EXPECT_EQ(network_context.reports().size(), 2u);
}

TEST_F(ConnectionAllowlistTest, HistoryNavigationDoesNotQueueReport) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  ConnectionAllowlistTestNetworkContext network_context;
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  storage_partition_impl->SetNetworkContextForTesting(
      network_context.BindNewPipeAndPassRemote());

  const GURL first_url("https://a.example/first?param=1");
  const GURL second_url("https://b.example/second");

  // 1. Navigate to first_url.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), first_url);

  // 2. Navigate to second_url with both enforced and report-only allowlists.
  auto second_nav =
      NavigationSimulator::CreateBrowserInitiated(second_url, web_contents());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader(
      "Connection-Allowlist",
      "(response-origin); redirects=block; report-to=endpoint-group");
  response_headers->SetHeader(
      "Connection-Allowlist-Report-Only",
      "(response-origin); redirects=block; report-to=endpoint-group");
  second_nav->SetResponseHeaders(response_headers);
  second_nav->Commit();

  // Flush the back-forward cache so the history traversal requires a network
  // navigation request.
  controller().GetBackForwardCache().Flush();

  // 3. Initiate a renderer-initiated history navigation back to first_url.
  // It should be blocked by the enforced Connection-Allowlist, and must not
  // queue any violation reports containing first_url.
  NavigationHandleObserver history_blocked_observer(web_contents(), first_url);
  main_test_rfh()->GoToEntryAtOffset(-1, /*has_user_gesture=*/true,
                                     base::TimeTicks::Now(), std::nullopt);
  main_test_rfh()->SimulateBeforeUnloadCompleted(/*proceed=*/true);
  AbortPendingErrorPageCommit();

  EXPECT_TRUE(history_blocked_observer.is_error());
  EXPECT_EQ(history_blocked_observer.net_error_code(),
            net::ERR_NETWORK_ACCESS_REVOKED);
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.reports().empty());
}

TEST_F(ConnectionAllowlistTest, HistoryRedirectNavigationDoesNotQueueReport) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  ConnectionAllowlistTestNetworkContext network_context;
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  storage_partition_impl->SetNetworkContextForTesting(
      network_context.BindNewPipeAndPassRemote());

  const GURL first_url("https://a.example/first?param=1");
  const GURL second_url("https://b.example/second");

  // 1. Navigate to first_url.
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), first_url);

  // 2. Navigate to second_url with a report-only allowlist and an enforced
  // allowlist that permits first_url but blocks redirects.
  auto second_nav =
      NavigationSimulator::CreateBrowserInitiated(second_url, web_contents());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist",
                              "(response-origin \"https://a.example/*\"); "
                              "redirects=block; report-to=endpoint-group");
  response_headers->SetHeader(
      "Connection-Allowlist-Report-Only",
      "(response-origin); redirects=block; report-to=endpoint-group");
  second_nav->SetResponseHeaders(response_headers);
  second_nav->Commit();

  controller().GetBackForwardCache().Flush();

  // 3. Initiate a renderer-initiated history navigation to first_url (offset
  // -1) that redirects. Neither the initial report-only check nor the redirect
  // check should queue a report.
  NavigationHandleObserver history_redirect_observer(web_contents(), first_url);
  main_test_rfh()->GoToEntryAtOffset(-1, /*has_user_gesture=*/true,
                                     base::TimeTicks::Now(), std::nullopt);
  main_test_rfh()->SimulateBeforeUnloadCompleted(/*proceed=*/true);
  NavigationRequest* request =
      main_test_rfh()->frame_tree_node()->navigation_request();
  ASSERT_TRUE(request);
  auto* url_loader =
      static_cast<TestNavigationURLLoader*>(request->loader_for_testing());
  ASSERT_TRUE(url_loader);
  url_loader->SimulateServerRedirect(GURL("https://a.example/redirected"));
  AbortPendingErrorPageCommit();

  EXPECT_TRUE(history_redirect_observer.is_error());
  EXPECT_EQ(history_redirect_observer.net_error_code(),
            net::ERR_UNSAFE_REDIRECT);
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.reports().empty());
}

TEST_F(ConnectionAllowlistTest,
       PreconnectDoesNotQueueDuplicateOrHistoryReport) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  PreconnectTestContentBrowserClient browser_client;

  ConnectionAllowlistTestNetworkContext network_context;
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  storage_partition_impl->SetNetworkContextForTesting(
      network_context.BindNewPipeAndPassRemote());

  const GURL first_url("https://a.example/first?param=1");
  const GURL second_url("https://b.example/second");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), first_url);

  auto second_nav =
      NavigationSimulator::CreateBrowserInitiated(second_url, web_contents());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist",
                              "(response-origin); report-to=endpoint-group");
  response_headers->SetHeader("Connection-Allowlist-Report-Only",
                              "(response-origin); report-to=endpoint-group");
  second_nav->SetResponseHeaders(response_headers);
  second_nav->Commit();

  // 1. Append a top-navigation-sandboxed child frame and request a history
  // traversal to offset -1. The request is discarded before starting due to the
  // sandbox check, and the preconnect check in the NavigationRequest
  // constructor must not queue a report.
  RenderFrameHost* child_rfh =
      RenderFrameHostTester::For(main_rfh())->AppendChild("sandboxed_child");
  auto child_nav = NavigationSimulator::CreateRendererInitiated(
      GURL("https://b.example/subframe"), child_rfh);
  auto child_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  child_headers->SetHeader("Content-Security-Policy", "sandbox allow-scripts");
  child_headers->SetHeader("Connection-Allowlist-Report-Only",
                           "(response-origin); report-to=endpoint-group");
  child_nav->SetResponseHeaders(child_headers);
  child_nav->Commit();
  child_rfh = child_nav->GetFinalRenderFrameHost();

  controller().GetBackForwardCache().Flush();

  static_cast<RenderFrameHostImpl*>(child_rfh)->GoToEntryAtOffset(
      -1, /*has_user_gesture=*/true, base::TimeTicks::Now(), std::nullopt);
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.reports().empty());

  // 2. A normal renderer-initiated navigation to a disallowed URL with
  // preconnect enabled should queue only a single report per policy (not one
  // during preconnect in the constructor and a second in BeginNavigationImpl).
  const GURL disallowed_url("https://d.example/target");
  NavigationHandleObserver normal_blocked_observer(web_contents(),
                                                   disallowed_url);
  auto disallowed_nav =
      NavigationSimulator::CreateRendererInitiated(disallowed_url, main_rfh());
  disallowed_nav->Start();
  AbortPendingErrorPageCommit();

  EXPECT_TRUE(normal_blocked_observer.is_error());
  EXPECT_EQ(normal_blocked_observer.net_error_code(),
            net::ERR_NETWORK_ACCESS_REVOKED);
  network_context.FlushForTesting();
  ASSERT_EQ(network_context.reports().size(), 2u);
  EXPECT_EQ(*network_context.reports()[0].body.FindString("connection"),
            disallowed_url.spec());
  EXPECT_EQ(*network_context.reports()[0].body.FindString("disposition"),
            "report");
  EXPECT_EQ(*network_context.reports()[1].body.FindString("connection"),
            disallowed_url.spec());
  EXPECT_EQ(*network_context.reports()[1].body.FindString("disposition"),
            "enforce");
}

TEST_F(ConnectionAllowlistTest, ClearNetworkRestrictionsSkipsUnregisteredIds) {
  ConnectionAllowlistTestNetworkContext network_context;
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  storage_partition_impl->SetNetworkContextForTesting(
      network_context.BindNewPipeAndPassRemote());

  const base::UnguessableToken id_a = base::UnguessableToken::Create();
  const base::UnguessableToken id_b = base::UnguessableToken::Create();
  network::ConnectionAllowlists allowlists;
  allowlists.enforced.emplace();

  base::test::TestFuture<void> restricted;
  storage_partition_impl->RestrictNetworkForIdsInNetworkContext(
      {{id_a, std::move(allowlists)}}, restricted.GetCallback());
  ASSERT_TRUE(restricted.Wait());
  EXPECT_TRUE(storage_partition_impl->HasNetworkRestrictions(id_a));
  EXPECT_FALSE(storage_partition_impl->HasNetworkRestrictions(id_b));

  // Clearing `{id_a, id_b}` should only send `id_a` to `NetworkContext`.
  {
    base::test::TestFuture<void> cleared;
    storage_partition_impl->SetClearNetworkRestrictionsParamsForTesting(
        base::TimeDelta(), cleared.GetRepeatingCallback());
    storage_partition_impl->ClearNetworkRestrictionsAfterDelay({id_a, id_b});
    ASSERT_TRUE(cleared.Wait());
  }
  network_context.FlushForTesting();
  EXPECT_FALSE(storage_partition_impl->HasNetworkRestrictions(id_a));
  ASSERT_EQ(network_context.cleared_network_restrictions_calls().size(), 1u);
  EXPECT_EQ(network_context.cleared_network_restrictions_calls()[0],
            std::vector<base::UnguessableToken>{id_a});

  // Clearing unregistered `id_b` (and already-cleared `id_a`) must not send any
  // `ClearNetworkRestrictions` IPC.
  {
    base::test::TestFuture<void> cleared;
    storage_partition_impl->SetClearNetworkRestrictionsParamsForTesting(
        base::TimeDelta(), cleared.GetRepeatingCallback());
    storage_partition_impl->ClearNetworkRestrictionsAfterDelay({id_b});
    ASSERT_TRUE(cleared.Wait());
  }
  {
    base::test::TestFuture<void> cleared;
    storage_partition_impl->SetClearNetworkRestrictionsParamsForTesting(
        base::TimeDelta(), cleared.GetRepeatingCallback());
    storage_partition_impl->ClearNetworkRestrictionsAfterDelay({id_a});
    ASSERT_TRUE(cleared.Wait());
  }
  storage_partition_impl->SetClearNetworkRestrictionsParamsForTesting(
      base::TimeDelta(), base::DoNothing());
  network_context.FlushForTesting();
  EXPECT_EQ(network_context.cleared_network_restrictions_calls().size(), 1u);

  // Destroying an unrestricted committed document (which mints a non-NoOp
  // `network_restrictions_id` that was never registered) must not schedule a
  // delayed clear task in `~DocumentAssociatedData()`.
  bool clear_callback_invoked = false;
  storage_partition_impl->SetClearNetworkRestrictionsParamsForTesting(
      base::TimeDelta(),
      base::BindRepeating([](bool* invoked) { *invoked = true; },
                          base::Unretained(&clear_callback_invoked)));
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://a.example/unrestricted1"));
  EXPECT_NE(main_rfh()->GetNetworkRestrictionsID(),
            network::GetNoOpNetworkRestrictionsId());
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://a.example/unrestricted2"));
  controller().GetBackForwardCache().Flush();
  base::RunLoop flush_ui_tasks;
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, flush_ui_tasks.QuitClosure());
  flush_ui_tasks.Run();
  EXPECT_FALSE(clear_callback_invoked);
  storage_partition_impl->SetClearNetworkRestrictionsParamsForTesting(
      base::TimeDelta(), base::DoNothing());
}

TEST_F(ConnectionAllowlistTest,
       SameDocumentRestartAndErrorPageResetNetworkRestrictionsId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  ConnectionAllowlistTestNetworkContext network_context;
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      browser_context()->GetDefaultStoragePartition());
  storage_partition_impl->SetNetworkContextForTesting(
      network_context.BindNewPipeAndPassRemote());
  storage_partition_impl->SetClearNetworkRestrictionsParamsForTesting(
      base::TimeDelta(), base::DoNothing());

  // A cross-document navigation failing before response commits an error page
  // with the NoOp ID rather than its provisional candidate ID.
  {
    auto failed_nav = NavigationSimulator::CreateBrowserInitiated(
        GURL("https://b.example/fail"), web_contents());
    failed_nav->Fail(net::ERR_CONNECTION_REFUSED);
    failed_nav->CommitErrorPage();
    EXPECT_EQ(failed_nav->GetFinalRenderFrameHost()->GetNetworkRestrictionsID(),
              network::GetNoOpNetworkRestrictionsId());
  }

  // Commit a document with `Connection-Allowlist: ()` at `#1`, then navigate
  // same-document to `#2`.
  auto initial_nav = NavigationSimulator::CreateBrowserInitiated(
      GURL("https://a.example/page#1"), web_contents());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "()");
  initial_nav->SetResponseHeaders(response_headers);
  initial_nav->Commit();

  const base::UnguessableToken active_id =
      main_rfh()->GetNetworkRestrictionsID();
  ASSERT_NE(active_id, network::GetNoOpNetworkRestrictionsId());
  EXPECT_TRUE(storage_partition_impl->HasNetworkRestrictions(active_id));

  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://a.example/page#2"));
  EXPECT_EQ(main_rfh()->GetNetworkRestrictionsID(), active_id);

  // 1. Direct check on `ResetForCrossDocumentRestart()`: start a same-document
  // history navigation back to `#1` (`active_id` is copied onto `request`),
  // verify `ResetForCrossDocumentRestart()` resets `network_restrictions_id()`
  // to NoOp, and abort the same-document commit cleanly.
  {
    auto history_sim = NavigationSimulator::CreateHistoryNavigation(
        -1, web_contents(), /*is_renderer_initiated=*/false);
    history_sim->Start();
    NavigationRequest* request =
        static_cast<NavigationRequest*>(history_sim->GetNavigationHandle());
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->IsSameDocument());
    EXPECT_EQ(request->network_restrictions_id(), active_id);
    const base::UnguessableToken nav_token =
        request->commit_params().navigation_token;

    request->ResetForCrossDocumentRestart();
    EXPECT_EQ(request->network_restrictions_id(),
              network::GetNoOpNetworkRestrictionsId());

    main_test_rfh()->SimulateOnSameDocumentCommitProcessed(
        nav_token, /*should_replace_current_entry=*/false,
        blink::mojom::CommitResult::Aborted);
  }

  // 2. End-to-end `RestartCrossDocument` + pre-response failure: restarting as
  // cross-document replaces `active_id` with a fresh candidate ID, and failing
  // into an error page resets it back to NoOp without touching `active_id`.
  {
    auto history_sim = NavigationSimulator::CreateHistoryNavigation(
        -1, web_contents(), /*is_renderer_initiated=*/false);
    history_sim->Start();
    NavigationRequest* request =
        static_cast<NavigationRequest*>(history_sim->GetNavigationHandle());
    ASSERT_TRUE(request);
    ASSERT_TRUE(request->IsSameDocument());
    EXPECT_EQ(request->network_restrictions_id(), active_id);
    const base::UnguessableToken nav_token =
        request->commit_params().navigation_token;

    main_test_rfh()->SimulateOnSameDocumentCommitProcessed(
        nav_token, /*should_replace_current_entry=*/false,
        blink::mojom::CommitResult::RestartCrossDocument);

    NavigationRequest* restarted_request =
        main_test_rfh()->frame_tree_node()->navigation_request();
    ASSERT_TRUE(restarted_request);
    EXPECT_FALSE(restarted_request->IsSameDocument());
    EXPECT_NE(restarted_request->network_restrictions_id(), active_id);
    EXPECT_NE(restarted_request->network_restrictions_id(),
              network::GetNoOpNetworkRestrictionsId());

    auto* url_loader = static_cast<TestNavigationURLLoader*>(
        restarted_request->loader_for_testing());
    ASSERT_TRUE(url_loader);
    url_loader->SimulateError(net::ERR_CONNECTION_REFUSED);

    TestRenderFrameHost* error_rfh = main_test_rfh();
    if (error_rfh->navigation_requests().empty()) {
      error_rfh =
          static_cast<TestRenderFrameHost*>(error_rfh->frame_tree_node()
                                                ->render_manager()
                                                ->speculative_frame_host());
    }
    ASSERT_TRUE(error_rfh);
    ASSERT_FALSE(error_rfh->navigation_requests().empty());
    EXPECT_EQ(error_rfh->navigation_requests()
                  .begin()
                  ->first->network_restrictions_id(),
              network::GetNoOpNetworkRestrictionsId());
    AbortPendingErrorPageCommit();
  }

  // `main_rfh()` is still active and its `active_id` remains registered.
  EXPECT_TRUE(storage_partition_impl->HasNetworkRestrictions(active_id));
}

}  // namespace content
