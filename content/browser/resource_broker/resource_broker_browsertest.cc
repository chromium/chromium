// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/resource_broker/resource_broker_host.h"
#include "content/browser/resource_broker/resource_broker_routing.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/services/resource_broker/public/mojom/resource_broker.mojom.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Returns the `ChildProcessHost` of the broker's utility process, or nullptr if
// the process is not running. `ServiceProcessHost::Launch()` uses the service's
// interface name as the process's `metrics_name`, which is how the broker's
// process is identified among the utility processes.
ChildProcessHost* FindBrokerProcessHost() {
  for (BrowserChildProcessHostIterator it(PROCESS_TYPE_UTILITY); !it.Done();
       ++it) {
    if (it.GetData().metrics_name ==
        resource_broker::mojom::ResourceBrokerService::Name_) {
      return it.GetHost();
    }
  }
  return nullptr;
}

// Asks the broker's utility process to exit via the browser's child process
// machinery. Returns false if the process is not running.
//
// `base::Process::Terminate()` cannot be used for this: on Android, utility
// processes are isolated services running under a different UID, so `kill()`
// from the browser fails with EPERM. Note that `ForceShutdown()` removes the
// host from the list that `BrowserChildProcessHostIterator` walks, so it must
// not be called while iterating.
bool ShutDownBrokerProcess() {
  ChildProcessHost* broker = FindBrokerProcessHost();
  if (!broker) {
    return false;
  }
  broker->ForceShutdown();
  return true;
}

}  // namespace

class ResourceBrokerBrowserTest : public ContentBrowserTest {
 public:
  ResourceBrokerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kResourceBroker);
  }

  void TearDownOnMainThread() override {
    ResourceBrokerHost::ResetForTesting();
    ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The first `GetService()` call launches the broker in a real utility process.
IN_PROC_BROWSER_TEST_F(ResourceBrokerBrowserTest, LaunchAndInitialize) {
  auto& host = ResourceBrokerHost::GetInstance();
  ASSERT_TRUE(host.GetService());

  // A failed launch surfaces as a disconnect during this round-trip, so a
  // still-bound remote afterwards means the utility process started and bound
  // the receiver.
  host.MaybeFlushForTesting();
  EXPECT_TRUE(host.IsServiceRunningForTesting());
  EXPECT_TRUE(FindBrokerProcessHost());
}

// Exercises the host's disconnect handling against a real process exit: the
// host must notice it and refuse to relaunch while the cooldown is active.
IN_PROC_BROWSER_TEST_F(ResourceBrokerBrowserTest, ProcessExitStartsCooldown) {
  auto& host = ResourceBrokerHost::GetInstance();
  ASSERT_TRUE(host.GetService());

  // Wait for the process to be up before asking it to exit.
  host.MaybeFlushForTesting();
  ASSERT_TRUE(ShutDownBrokerProcess()) << "broker utility process not found";

  // The shutdown request travels on the child's control pipe, independently of
  // the service pipe, so a `MaybeFlushForTesting()` round-trip may complete
  // before the process exits. Wait for the host to observe the disconnect
  // instead.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !host.IsServiceRunningForTesting(); }));

  // `GetService()` must not relaunch the service during the cooldown.
  EXPECT_FALSE(host.GetService());
}

class ResourceBrokerRoutingBrowserTest : public ContentBrowserTest {
 public:
  ResourceBrokerRoutingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kResourceBroker);
  }

  // Ensure strict site isolation is enabled on all platforms (e.g. Android).
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    // Navigate once to flush the initial empty document's factory sample.
    ASSERT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  }

 protected:
  static constexpr char kHistogram[] = "ResourceBroker.RoutingDecision";
  static constexpr char kMainFrameHistogram[] =
      "ResourceBroker.RoutingDecision.MainFrame";
  static constexpr char kSubframeHistogram[] =
      "ResourceBroker.RoutingDecision.Subframe";

  void ExpectOnlyDecision(const base::HistogramTester& histogram_tester,
                          const char* histogram,
                          ResourceBrokerRoutingDecision decision) {
    histogram_tester.ExpectUniqueSample(histogram, decision, 1);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest, RoutesWebPage) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kRouted);
  ExpectOnlyDecision(histogram_tester, kMainFrameHistogram,
                     ResourceBrokerRoutingDecision::kRouted);
  EXPECT_TRUE(histogram_tester.GetAllSamples(kSubframeHistogram).empty());
}

// WebUI documents with bindings use a non-network default factory.
IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       IgnoresWebUIWithBindings) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(shell(), GetWebUIURL("gpu")));

  EXPECT_TRUE(histogram_tester.GetAllSamples(kHistogram).empty());
}

// chrome-untrusted:// has no WebUI bindings and gets a network-backed default
// factory, which is excluded by scheme.
IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest, BypassesWebUI) {
  const GURL untrusted_url("chrome-untrusted://test-host/title1.html");
  AddUntrustedDataSource(shell()->web_contents()->GetBrowserContext(),
                         untrusted_url.GetHost());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(shell(), untrusted_url));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kWebUI);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest, BypassesErrorPage) {
  base::HistogramTester histogram_tester;
  ASSERT_FALSE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/close-socket")));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kErrorPage);
  ExpectOnlyDecision(histogram_tester, kMainFrameHistogram,
                     ResourceBrokerRoutingDecision::kErrorPage);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       BypassesCrossOriginIsolatedPage) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "/set-header?Cross-Origin-Opener-Policy: same-origin&"
                   "Cross-Origin-Embedder-Policy: require-corp")));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kCrossOriginIsolated);
  ExpectOnlyDecision(histogram_tester, kMainFrameHistogram,
                     ResourceBrokerRoutingDecision::kCrossOriginIsolated);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       BypassesSandboxedIframe) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
                                         "/page_with_sandboxed_iframe.html")));

  histogram_tester.ExpectTotalCount(kHistogram, 2);
  histogram_tester.ExpectBucketCount(kHistogram,
                                     ResourceBrokerRoutingDecision::kRouted, 1);
  histogram_tester.ExpectBucketCount(
      kHistogram, ResourceBrokerRoutingDecision::kOpaqueOrigin, 1);
  ExpectOnlyDecision(histogram_tester, kMainFrameHistogram,
                     ResourceBrokerRoutingDecision::kRouted);
  ExpectOnlyDecision(histogram_tester, kSubframeHistogram,
                     ResourceBrokerRoutingDecision::kOpaqueOrigin);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       BypassesNoncePartitionedIframe) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_credentialless_iframe.html")));

  histogram_tester.ExpectTotalCount(kHistogram, 2);
  histogram_tester.ExpectBucketCount(kHistogram,
                                     ResourceBrokerRoutingDecision::kRouted, 1);
  histogram_tester.ExpectBucketCount(
      kHistogram, ResourceBrokerRoutingDecision::kNoncePartitioned, 1);
  ExpectOnlyDecision(histogram_tester, kMainFrameHistogram,
                     ResourceBrokerRoutingDecision::kRouted);
  ExpectOnlyDecision(histogram_tester, kSubframeHistogram,
                     ResourceBrokerRoutingDecision::kNoncePartitioned);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       BypassesOpaqueOriginDocument) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(shell(), GURL("data:text/html,<p>opaque</p>")));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kOpaqueOrigin);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       BypassesNonHttpScheme) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(nullptr, "title1.html")));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kSchemeNotHttpOrHttps);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       BypassesServiceWorkerControlledPage) {
  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  ASSERT_EQ(true, EvalJs(shell()->web_contents(), R"(
      (async () => {
        await navigator.serviceWorker.register('/service_worker/empty.js');
        await navigator.serviceWorker.ready;
        return true;
      })();)"));

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kServiceWorkerControlled);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       IgnoresTrustedCrossOriginPrefetchFactory) {
  ASSERT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/page_with_credentialless_iframe.html")));
  base::HistogramTester histogram_tester;
  auto* main_frame = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame());
  auto* credentialless_iframe =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(main_frame, 0));
  ASSERT_TRUE(credentialless_iframe);
  ASSERT_TRUE(main_frame->CreateCrossOriginPrefetchLoaderFactoryBundle());
  ASSERT_TRUE(
      credentialless_iframe->CreateCrossOriginPrefetchLoaderFactoryBundle());
  EXPECT_TRUE(histogram_tester.GetAllSamples(kHistogram).empty());
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       ClassifiesIsolatedWorldByIsolatedWorldOrigin) {
  class IsolatedWorldBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    void OverrideURLLoaderFactoryParams(
        BrowserContext* browser_context,
        const url::Origin& origin,
        bool is_for_isolated_world,
        bool is_for_service_worker,
        network::mojom::URLLoaderFactoryParams* factory_params) override {
      if (is_for_isolated_world) {
        factory_params->ignore_isolated_world_origin = false;
      }
    }
  };

  IsolatedWorldBrowserClient browser_client;
  base::HistogramTester histogram_tester;
  shell()
      ->web_contents()
      ->GetPrimaryMainFrame()
      ->MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
          {url::Origin::Create(GURL("chrome-untrusted://test-host"))},
          /*push_to_renderer_now=*/true);

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kWebUI);
  ExpectOnlyDecision(histogram_tester, kMainFrameHistogram,
                     ResourceBrokerRoutingDecision::kWebUI);
}

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingBrowserTest,
                       IgnoresSameDocumentNavigation) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/title1.html#frag")));
  EXPECT_TRUE(histogram_tester.GetAllSamples(kHistogram).empty());
}

class ResourceBrokerRoutingDisabledBrowserTest : public ContentBrowserTest {
 public:
  ResourceBrokerRoutingDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kResourceBroker);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingDisabledBrowserTest,
                       DoesNotRecordWhenFeatureDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(
      histogram_tester.GetAllSamples("ResourceBroker.RoutingDecision").empty());
}

class ResourceBrokerRoutingNoSiteIsolationBrowserTest
    : public ResourceBrokerRoutingBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ResourceBrokerRoutingBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kSitePerProcess);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }
};

IN_PROC_BROWSER_TEST_F(ResourceBrokerRoutingNoSiteIsolationBrowserTest,
                       BypassesProcessNotLockedToSite) {
  ASSERT_FALSE(AreAllSitesIsolatedForTesting());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

  ExpectOnlyDecision(histogram_tester, kHistogram,
                     ResourceBrokerRoutingDecision::kNotLockedToSite);
}

}  // namespace content
