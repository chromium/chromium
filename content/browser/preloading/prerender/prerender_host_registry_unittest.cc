// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host_registry.h"

#include <cstdint>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/speculation_rules/speculation_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/load_flags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {
namespace {

blink::mojom::SpeculationCandidatePtr CreatePrerenderCandidate(
    const GURL& url) {
  auto candidate = blink::mojom::SpeculationCandidate::New();
  candidate->action = blink::mojom::SpeculationAction::kPrerender;
  candidate->url = url;
  candidate->referrer = blink::mojom::Referrer::New();
  candidate->eagerness = blink::mojom::SpeculationEagerness::kEager;
  return candidate;
}

void SendCandidates(const std::vector<GURL>& urls,
                    mojo::Remote<blink::mojom::SpeculationHost>& remote) {
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  candidates.resize(urls.size());
  base::ranges::transform(urls, candidates.begin(), &CreatePrerenderCandidate);
  remote->UpdateSpeculationCandidates(std::move(candidates));
  remote.FlushForTesting();
}

void SendCandidate(const GURL& url,
                   mojo::Remote<blink::mojom::SpeculationHost>& remote) {
  SendCandidates({url}, remote);
}

std::unique_ptr<NavigationSimulatorImpl> CreateActivation(
    const GURL& prerendering_url,
    WebContentsImpl& web_contents) {
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(
          prerendering_url, web_contents.GetPrimaryMainFrame());
  navigation->SetReferrer(blink::mojom::Referrer::New(
      web_contents.GetPrimaryMainFrame()->GetLastCommittedURL(),
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));
  return navigation;
}

// Finish a prerendering navigation that was already started with
// CreateAndStartHost().
void CommitPrerenderNavigation(PrerenderHost& host) {
  // Normally we could use EmbeddedTestServer to provide a response, but these
  // tests use RenderViewHostImplTestHarness so the load goes through a
  // TestNavigationURLLoader which we don't have access to in order to
  // complete. Use NavigationSimulator to finish the navigation.
  FrameTreeNode* ftn = FrameTreeNode::From(host.GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulator> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  sim->Commit();
  EXPECT_TRUE(host.is_ready_for_activation());
}

class PrerenderHostRegistryTest : public RenderViewHostImplTestHarness {
 public:
  PrerenderHostRegistryTest() = default;
  ~PrerenderHostRegistryTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    web_contents_delegate_ =
        std::make_unique<test::ScopedPrerenderWebContentsDelegate>(*contents());
    contents()->NavigateAndCommit(GURL("https://example.com/"));
  }

  RenderFrameHostImpl* NavigatePrimaryPage(TestWebContents* web_contents,
                                           const GURL& dest_url) {
    std::unique_ptr<NavigationSimulatorImpl> navigation =
        NavigationSimulatorImpl::CreateRendererInitiated(
            dest_url, web_contents->GetPrimaryMainFrame());
    navigation->SetTransition(ui::PAGE_TRANSITION_LINK);
    navigation->Start();
    navigation->Commit();
    RenderFrameHostImpl* render_frame_host =
        web_contents->GetPrimaryMainFrame();
    EXPECT_EQ(render_frame_host->GetLastCommittedURL(), dest_url);
    return render_frame_host;
  }

  // Helper method to test the navigation param matching logic which allows a
  // prerender host to be used in a potential activation navigation only if its
  // params match the potential activation navigation params. Use setup_callback
  // to set the parameters. Returns true if the host was selected as a
  // potential candidate for activation, and false otherwise.
  [[nodiscard]] bool CheckIsActivatedForParams(
      base::OnceCallback<void(NavigationSimulatorImpl*)> setup_callback) {
    RenderFrameHostImpl* render_frame_host = contents()->GetPrimaryMainFrame();

    const GURL kPrerenderingUrl("https://example.com/next");
    registry().CreateAndStartHost(GeneratePrerenderAttributes(
        kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
        blink::mojom::SpeculationEagerness::kEager, render_frame_host));
    PrerenderHost* prerender_host =
        registry().FindHostByUrlForTesting(kPrerenderingUrl);
    CommitPrerenderNavigation(*prerender_host);

    std::unique_ptr<NavigationSimulatorImpl> navigation =
        NavigationSimulatorImpl::CreateRendererInitiated(kPrerenderingUrl,
                                                         render_frame_host);
    // Set a default referrer policy that matches the initial prerender
    // navigation.
    // TODO(falken): Fix NavigationSimulatorImpl to do this itself.
    navigation->SetReferrer(blink::mojom::Referrer::New(
        contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
        network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin));

    // Change a parameter to differentiate the activation request from the
    // prerendering request.
    std::move(setup_callback).Run(navigation.get());
    navigation->Start();
    NavigationRequest* navigation_request = navigation->GetNavigationHandle();
    // Use is_running_potential_prerender_activation_checks() instead of
    // IsPrerenderedPageActivation() because the NavigationSimulator does not
    // proceed past CommitDeferringConditions on potential activations,
    // so IsPrerenderedPageActivation() will fail with a CHECK because
    // prerender_frame_tree_node_id_ is not populated.
    // TODO(crbug.com/40784651): Fix NavigationSimulator to wait for
    // commit deferring conditions as it does throttles.
    return navigation_request
        ->is_running_potential_prerender_activation_checks();
  }

  // Helper method to perform a prerender activation that includes specialized
  // handling or setup on the initial prerender navigation via the
  // setup_callback parameter.
  void SetupPrerenderAndCommit(
      base::OnceCallback<void(NavigationSimulatorImpl*)> setup_callback) {
    const GURL kPrerenderingUrl("https://example.com/next");
    const FrameTreeNodeId prerender_frame_tree_node_id =
        registry().CreateAndStartHost(GeneratePrerenderAttributes(
            kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kEager,
            contents()->GetPrimaryMainFrame()));
    ASSERT_TRUE(prerender_frame_tree_node_id);
    PrerenderHost* prerender_host =
        registry().FindNonReservedHostById(prerender_frame_tree_node_id);

    // Complete the initial prerender navigation.
    FrameTreeNode* ftn =
        FrameTreeNode::From(prerender_host->GetPrerenderedMainFrameHost());
    std::unique_ptr<NavigationSimulatorImpl> sim =
        NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
    std::move(setup_callback).Run(sim.get());
    sim->Commit();
    EXPECT_TRUE(prerender_host->is_ready_for_activation());

    // Activate the prerendered page.
    contents()->ActivatePrerenderedPage(kPrerenderingUrl);
  }

  PrerenderAttributes GeneratePrerenderAttributes(
      const GURL& url,
      PreloadingTriggerType trigger_type,
      const std::string& embedder_histogram_suffix,
      std::optional<blink::mojom::SpeculationEagerness> eagerness,
      RenderFrameHostImpl* rfh) {
    switch (trigger_type) {
      case PreloadingTriggerType::kSpeculationRule:
      case PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld:
      case PreloadingTriggerType::kSpeculationRuleFromAutoSpeculationRules:
        return PrerenderAttributes(
            url, trigger_type, embedder_histogram_suffix,
            blink::mojom::SpeculationTargetHint::kNoHint, Referrer(), eagerness,
            /*no_vary_search_expected=*/std::nullopt,
            rfh->GetLastCommittedOrigin(), rfh->GetProcess()->GetID(),
            contents()->GetWeakPtr(), rfh->GetFrameToken(),
            rfh->GetFrameTreeNodeId(), rfh->GetPageUkmSourceId(),
            ui::PAGE_TRANSITION_LINK,
            /*should_warm_up_compositor=*/false,
            /*url_match_predicate=*/{},
            /*prerender_navigation_handle_callback=*/{});
      case PreloadingTriggerType::kEmbedder:
        return PrerenderAttributes(
            url, trigger_type, embedder_histogram_suffix,
            /*target_hint=*/std::nullopt, Referrer(),
            /*eagerness=*/std::nullopt,
            /*no_vary_search_expected=*/std::nullopt,
            /*initiator_origin=*/std::nullopt,
            /*initiator_process_id=*/ChildProcessHost::kInvalidUniqueID,
            contents()->GetWeakPtr(),
            /*initiator_frame_token=*/std::nullopt,
            /*initiator_frame_tree_node_id=*/
            FrameTreeNodeId(),
            /*initiator_ukm_id=*/ukm::kInvalidSourceId,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
            /*should_warm_up_compositor=*/false,
            /*url_match_predicate=*/{},
            /*prerender_navigation_handle_callback=*/{});
    }
  }

  void ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus status,
      base::HistogramBase::Count count = 1) {
    histogram_tester_.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
        status, count);
  }

  void ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus status,
      base::HistogramBase::Count count = 1) {
    histogram_tester_.ExpectBucketCount(
        "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
        status, count);
  }

  void ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus status,
      const std::string& embedder_histogram_suffix,
      base::HistogramBase::Count count = 1) {
    histogram_tester_.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_" +
            embedder_histogram_suffix,
        status, count);
  }

  void ExpectBucketCountOfEmbedderFinalStatus(
      PrerenderFinalStatus status,
      const std::string& embedder_histogram_suffix,
      base::HistogramBase::Count count = 1) {
    histogram_tester_.ExpectBucketCount(
        "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_" +
            embedder_histogram_suffix,
        status, count);
  }

  void ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch result,
      base::HistogramBase::Count count = 1) {
    histogram_tester_.ExpectUniqueSample(
        "Prerender.Experimental.ActivationNavigationParamsMatch."
        "SpeculationRule",
        result, count);
  }

  void ExpectBucketCountOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch result,
      base::HistogramBase::Count count = 1) {
    histogram_tester_.ExpectBucketCount(
        "Prerender.Experimental.ActivationNavigationParamsMatch."
        "SpeculationRule",
        result, count);
  }

  PrerenderHostRegistry& registry() {
    return *contents()->GetPrerenderHostRegistry();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  test::ScopedPrerenderFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost_SpeculationRule) {
  const GURL kPrerenderingUrl("https://example.com/next");
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(prerender_frame_tree_node_id);
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  contents()->ActivatePrerenderedPage(kPrerenderingUrl);

  // "Navigation.TimeToActivatePrerender.SpeculationRule" histogram should be
  // recorded on every prerender activation.
  histogram_tester().ExpectTotalCount(
      "Navigation.TimeToActivatePrerender.SpeculationRule", 1u);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost_Embedder_DirectURLInput) {
  const GURL kPrerenderingUrl("https://example.com/next");
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kEmbedder, "DirectURLInput",
          std::nullopt, contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(prerender_frame_tree_node_id);
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  contents()->ActivatePrerenderedPageFromAddressBar(kPrerenderingUrl);

  // "Navigation.TimeToActivatePrerender.Embedder_DirectURLInput" histogram
  // should be recorded on every prerender activation.
  histogram_tester().ExpectTotalCount(
      "Navigation.TimeToActivatePrerender.Embedder_DirectURLInput", 1u);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost_PreloadingConfigHoldback) {
  content::test::PreloadingConfigOverride preloading_config_override;
  preloading_config_override.SetHoldback(
      PreloadingType::kPrerender,
      content_preloading_predictor::kSpeculationRules, true);
  const GURL kPrerenderingUrl("https://example.com/next");
  auto* preloading_data = PreloadingData::GetOrCreateForWebContents(contents());
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(kPrerenderingUrl);
  PreloadingAttempt* preloading_attempt = preloading_data->AddPreloadingAttempt(
      content_preloading_predictor::kSpeculationRules,
      PreloadingType::kPrerender, std::move(same_url_matcher),
      /*planned_max_preloading_type=*/std::nullopt,
      contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(
          GeneratePrerenderAttributes(
              kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
              blink::mojom::SpeculationEagerness::kEager,
              contents()->GetPrimaryMainFrame()),
          preloading_attempt);
  EXPECT_TRUE(prerender_frame_tree_node_id.is_null());
}

TEST_F(PrerenderHostRegistryTest,
       CreateAndStartHost_HoldbackOverride_Holdback) {
  const GURL kPrerenderingUrl("https://example.com/next");
  auto* preloading_data = PreloadingData::GetOrCreateForWebContents(contents());
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(kPrerenderingUrl);
  PreloadingAttempt* preloading_attempt = preloading_data->AddPreloadingAttempt(
      content_preloading_predictor::kSpeculationRules,
      PreloadingType::kPrerender, std::move(same_url_matcher),
      /*planned_max_preloading_type=*/std::nullopt,
      contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  auto attributes = GeneratePrerenderAttributes(
      kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
      blink::mojom::SpeculationEagerness::kEager,
      contents()->GetPrimaryMainFrame());
  attributes.holdback_status_override = PreloadingHoldbackStatus::kHoldback;

  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(attributes, preloading_attempt);

  EXPECT_TRUE(prerender_frame_tree_node_id.is_null());
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost_HoldbackOverride_Allowed) {
  content::test::PreloadingConfigOverride preloading_config_override;
  preloading_config_override.SetHoldback(
      PreloadingType::kPrerender,
      content_preloading_predictor::kSpeculationRules, true);
  const GURL kPrerenderingUrl("https://example.com/next");
  auto* preloading_data = PreloadingData::GetOrCreateForWebContents(contents());
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(kPrerenderingUrl);
  PreloadingAttempt* preloading_attempt = preloading_data->AddPreloadingAttempt(
      content_preloading_predictor::kSpeculationRules,
      PreloadingType::kPrerender, std::move(same_url_matcher),
      /*planned_max_preloading_type=*/std::nullopt,
      contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  auto attributes = GeneratePrerenderAttributes(
      kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
      blink::mojom::SpeculationEagerness::kEager,
      contents()->GetPrimaryMainFrame());
  attributes.holdback_status_override = PreloadingHoldbackStatus::kAllowed;

  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(attributes, preloading_attempt);

  ASSERT_TRUE(prerender_frame_tree_node_id);
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  contents()->ActivatePrerenderedPage(kPrerenderingUrl);

  // "Navigation.TimeToActivatePrerender.SpeculationRule" histogram should be
  // recorded on every prerender activation.
  histogram_tester().ExpectTotalCount(
      "Navigation.TimeToActivatePrerender.SpeculationRule", 1u);
}

TEST_F(PrerenderHostRegistryTest, CreateAndStartHostForSameURL) {
  const GURL kPrerenderingUrl("https://example.com/next");

  const FrameTreeNodeId frame_tree_node_id1 =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_TRUE(frame_tree_node_id1);
  PrerenderHost* prerender_host1 =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);

  // Start the prerender host for the same URL. This second host should be
  // ignored, and the first host should still be findable.
  const FrameTreeNodeId frame_tree_node_id2 =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_TRUE(frame_tree_node_id2.is_null());
  EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl),
            prerender_host1);
  CommitPrerenderNavigation(*prerender_host1);

  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
}

class PrerenderHostRegistryLimitTest : public PrerenderHostRegistryTest {
 public:
  PrerenderHostRegistryLimitTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2NewLimitAndScheduler,
          {{"max_num_of_running_speculation_rules_eager_prerenders",
            base::NumberToString(MaxNumOfRunningSpeculationRules())}}}},
        {});
  }

  int MaxNumOfRunningSpeculationRules() { return 2; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to a specific number, and after once the prerender page was activated,
// PrerenderHostRegistry can start prerendering a new one.
TEST_F(PrerenderHostRegistryLimitTest, NumberLimit_Activation) {
  std::vector<FrameTreeNodeId> frame_tree_node_ids;
  std::vector<GURL> prerendering_ulrs;
  for (int i = 0; i < MaxNumOfRunningSpeculationRules() + 1; i++) {
    const GURL prerendering_url("https://example.com/next" +
                                base::NumberToString(i));
    FrameTreeNodeId frame_tree_node_id =
        registry().CreateAndStartHost(GeneratePrerenderAttributes(
            prerendering_url, PreloadingTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kEager,
            contents()->GetPrimaryMainFrame()));

    frame_tree_node_ids.push_back(frame_tree_node_id);
    prerendering_ulrs.push_back(prerendering_url);
  }

  // PrerenderHostRegistry should only start prerendering within the limit.
  for (int i = 0; i < MaxNumOfRunningSpeculationRules(); i++) {
    EXPECT_TRUE(frame_tree_node_ids[i]);
  }
  EXPECT_TRUE(frame_tree_node_ids[MaxNumOfRunningSpeculationRules()].is_null());
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded);

  // Activate the first prerender.
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(prerendering_ulrs[0]);
  CommitPrerenderNavigation(*prerender_host);
  contents()->ActivatePrerenderedPage(prerendering_ulrs[0]);

  // After the first prerender page was activated, PrerenderHostRegistry can
  // start prerendering a new one.
  FrameTreeNodeId frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          prerendering_ulrs[MaxNumOfRunningSpeculationRules()],
          PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_TRUE(frame_tree_node_id);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to a specific number, and new candidates can be processed after the initiator
// page navigates to a new same-origin page.
TEST_F(PrerenderHostRegistryLimitTest, NumberLimit_SameOriginNavigateAway) {
  RenderFrameHostImpl* render_frame_host = contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());

  std::vector<GURL> prerendering_urls;
  for (int i = 0; i < MaxNumOfRunningSpeculationRules() + 1; i++) {
    prerendering_urls.emplace_back("https://example.com/next" +
                                   base::NumberToString(i));
  }
  SendCandidates(prerendering_urls, remote1);

  // PrerenderHostRegistry should only start prerenderings within the limit.
  for (int i = 0; i < MaxNumOfRunningSpeculationRules(); i++) {
    ASSERT_NE(registry().FindHostByUrlForTesting(prerendering_urls[i]),
              nullptr);
  }
  ASSERT_EQ(registry().FindHostByUrlForTesting(
                prerendering_urls[MaxNumOfRunningSpeculationRules()]),
            nullptr);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded);

  // The initiator document navigates away.
  render_frame_host =
      NavigatePrimaryPage(contents(), GURL("https://example.com/elsewhere"));

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  for (int i = 0; i < MaxNumOfRunningSpeculationRules() + 1; i++) {
    EXPECT_EQ(registry().FindHostByUrlForTesting(prerendering_urls[i]),
              nullptr);
  }
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  SendCandidate(prerendering_urls[MaxNumOfRunningSpeculationRules()], remote2);

  EXPECT_NE(registry().FindHostByUrlForTesting(
                prerendering_urls[MaxNumOfRunningSpeculationRules()]),
            nullptr);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to a specific number, and new candidates can be processed after the initiator
// page navigates to a new cross-origin page.
TEST_F(PrerenderHostRegistryLimitTest, NumberLimit_CrossOriginNavigateAway) {
  RenderFrameHostImpl* render_frame_host = contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());

  std::vector<GURL> prerendering_urls;
  for (int i = 0; i < MaxNumOfRunningSpeculationRules() + 1; i++) {
    prerendering_urls.emplace_back("https://example.com/next" +
                                   base::NumberToString(i));
  }
  SendCandidates(prerendering_urls, remote1);

  // PrerenderHostRegistry should only start prerenderings within the limit.
  for (int i = 0; i < MaxNumOfRunningSpeculationRules(); i++) {
    ASSERT_NE(registry().FindHostByUrlForTesting(prerendering_urls[i]),
              nullptr);
  }
  ASSERT_EQ(registry().FindHostByUrlForTesting(
                prerendering_urls[MaxNumOfRunningSpeculationRules()]),
            nullptr);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded);

  // The initiator document navigates away to a cross-origin page.
  render_frame_host =
      NavigatePrimaryPage(contents(), GURL("https://example.org/"));

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  for (int i = 0; i < MaxNumOfRunningSpeculationRules() + 1; i++) {
    EXPECT_EQ(registry().FindHostByUrlForTesting(prerendering_urls[i]),
              nullptr);
  }
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  const GURL prerendering_url("https://example.org/next");
  SendCandidate(prerendering_url, remote2);
  EXPECT_NE(registry().FindHostByUrlForTesting(prerendering_url), nullptr);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded);
}

class PrerenderHostRegistryNewLimitAndSchedulerTest
    : public PrerenderHostRegistryTest,
      public testing::WithParamInterface<bool> {
 public:
  using PrerenderLimitGroup = PrerenderHostRegistry::PrerenderLimitGroup;

  PrerenderHostRegistryNewLimitAndSchedulerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPrerender2NewLimitAndScheduler,
          {{"max_num_of_running_speculation_rules_eager_prerenders",
            base::NumberToString(
                MaxNumOfRunningSpeculationRulesEagerPrerenders())},
           {"max_num_of_running_speculation_rules_non_eager_prerenders",
            base::NumberToString(
                MaxNumOfRunningSpeculationRulesNonEagerPrerenders())},
           {"max_num_of_running_embedder_prerenders",
            base::NumberToString(MaxNumOfRunningEmbedderPrerenders())}}}},
        {});
  }

  int MaxNumOfRunningSpeculationRulesEagerPrerenders() { return 2; }
  int MaxNumOfRunningSpeculationRulesNonEagerPrerenders() { return 2; }
  int MaxNumOfRunningEmbedderPrerenders() { return 2; }

  const std::string embedder_histogram_suffix = "EmbedderSuffixForTest";

  bool IsNewTabTrigger(PrerenderLimitGroup limit_group) {
    return GetParam() && limit_group != PrerenderLimitGroup::kEmbedder;
  }

  FrameTreeNodeId CreateAndStartHostByLimitGroup(
      PrerenderLimitGroup limit_group) {
    static int unique_id = 0;
    const GURL prerendering_url("https://example.com/next_" +
                                base::NumberToString(unique_id));
    unique_id++;
    auto prerender_attributes = [&] {
      switch (limit_group) {
        case PrerenderLimitGroup::kSpeculationRulesEager:
          return GeneratePrerenderAttributes(
              prerendering_url, PreloadingTriggerType::kSpeculationRule, "",
              blink::mojom::SpeculationEagerness::kEager,
              contents()->GetPrimaryMainFrame());
        case PrerenderLimitGroup::kSpeculationRulesNonEager:
          return GeneratePrerenderAttributes(
              prerendering_url, PreloadingTriggerType::kSpeculationRule, "",
              blink::mojom::SpeculationEagerness::kModerate,
              contents()->GetPrimaryMainFrame());
        case PrerenderLimitGroup::kEmbedder:
          return GeneratePrerenderAttributes(
              prerendering_url, PreloadingTriggerType::kEmbedder,
              embedder_histogram_suffix, std::nullopt, nullptr);
      }
    }();

    PreloadingPredictor embedder_predictor(100, "Embedder");

    PreloadingPredictor creating_predictor = [&] {
      switch (limit_group) {
        case PrerenderLimitGroup::kSpeculationRulesEager:
        case PrerenderLimitGroup::kSpeculationRulesNonEager:
          return content_preloading_predictor::kSpeculationRules;
        case PrerenderLimitGroup::kEmbedder:
          return embedder_predictor;
      }
    }();
    PreloadingPredictor enacting_predictor = [&] {
      switch (limit_group) {
        case PrerenderLimitGroup::kSpeculationRulesEager:
          return content_preloading_predictor::kSpeculationRules;
        case PrerenderLimitGroup::kSpeculationRulesNonEager:
          // Arbitrarily chosen non-eager predictor.
          return preloading_predictor::kUrlPointerDownOnAnchor;
        case PrerenderLimitGroup::kEmbedder:
          return embedder_predictor;
      }
    }();

    return IsNewTabTrigger(limit_group)
               ? registry().CreateAndStartHostForNewTab(
                     prerender_attributes, creating_predictor,
                     enacting_predictor, PreloadingConfidence{100})
               : registry().CreateAndStartHost(prerender_attributes);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PrerenderHostRegistryNewLimitAndSchedulerTest,
                         testing::Bool());

// Tests the behavior of eager prerenders with the new limit and scheduler.
TEST_P(PrerenderHostRegistryNewLimitAndSchedulerTest,
       NewLimitAndScheduler_Eager) {
  // Starts the eager prerenders as many times as the specific limit.
  for (int i = 0; i < MaxNumOfRunningSpeculationRulesEagerPrerenders(); i++) {
    FrameTreeNodeId frame_tree_node_id = CreateAndStartHostByLimitGroup(
        PrerenderLimitGroup::kSpeculationRulesEager);
    EXPECT_TRUE(frame_tree_node_id);
  }

  // If we try to start eager prerenders after reaching the limit, that should
  // be canceled with kMaxNumOfRunningEagerPrerendersExceeded.
  FrameTreeNodeId frame_tree_node_id_eager_exceeded =
      CreateAndStartHostByLimitGroup(
          PrerenderLimitGroup::kSpeculationRulesEager);
  EXPECT_TRUE(frame_tree_node_id_eager_exceeded.is_null());
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded, 1);

  // On the other hand, prerenders belonging to different limit
  // group(non-eager, embedder) can still be started.
  FrameTreeNodeId frame_tree_node_id_non_eager = CreateAndStartHostByLimitGroup(
      PrerenderLimitGroup::kSpeculationRulesNonEager);
  FrameTreeNodeId frame_tree_node_id_embedder =
      CreateAndStartHostByLimitGroup(PrerenderLimitGroup::kEmbedder);
  EXPECT_TRUE(frame_tree_node_id_non_eager);
  EXPECT_TRUE(frame_tree_node_id_embedder);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded, 1);
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded,
      embedder_histogram_suffix, 0);
}

// Tests the behavior of non-eager prerenders with the new limit and scheduler.
TEST_P(PrerenderHostRegistryNewLimitAndSchedulerTest,
       NewLimitAndScheduler_NonEager) {
  std::vector<FrameTreeNodeId> started_prerender_ids;

  // Starts the non-eager prerenders as many times as the specific limit.
  for (int i = 0; i < MaxNumOfRunningSpeculationRulesNonEagerPrerenders();
       i++) {
    FrameTreeNodeId frame_tree_node_id = CreateAndStartHostByLimitGroup(
        PrerenderLimitGroup::kSpeculationRulesNonEager);
    started_prerender_ids.push_back(frame_tree_node_id);
    EXPECT_TRUE(frame_tree_node_id);
  }

  // Even after the limit of non-eager speculation rules is reached, it is
  // permissible to start a new prerender. Instead, the oldest prerender will be
  // canceled with kMaxNumOfRunningNonEagerPrerendersExceeded to make room for a
  // new one.
  FrameTreeNodeId frame_tree_node_id_non_eager_exceeded =
      CreateAndStartHostByLimitGroup(
          PrerenderLimitGroup::kSpeculationRulesNonEager);
  ASSERT_TRUE(frame_tree_node_id_non_eager_exceeded);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded, 1);

  for (auto id : started_prerender_ids) {
    auto* web_contents_impl =
        static_cast<WebContentsImpl*>(WebContents::FromFrameTreeNodeId(id));
    PrerenderHost* prerender_host = nullptr;
    if (web_contents_impl) {
      prerender_host = web_contents_impl->GetPrerenderHostRegistry()
                           ->FindNonReservedHostById(id);
    }
    if (id == started_prerender_ids[0]) {
      // The oldest prerender has been canceled.
      EXPECT_EQ(prerender_host, nullptr);
    } else {
      EXPECT_NE(prerender_host, nullptr);
    }
  }

  // On the other hand, prerenders belonging to different limit group(eager,
  // embedder) can still be started and not invoke cancellation, as these limits
  // are separated.
  FrameTreeNodeId frame_tree_node_id_eager = CreateAndStartHostByLimitGroup(
      PrerenderLimitGroup::kSpeculationRulesEager);
  FrameTreeNodeId frame_tree_node_id_embedder =
      CreateAndStartHostByLimitGroup(PrerenderLimitGroup::kEmbedder);
  EXPECT_TRUE(frame_tree_node_id_eager);
  EXPECT_TRUE(frame_tree_node_id_embedder);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded, 1);
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded,
      embedder_histogram_suffix, 0);
}

// Tests the behavior of embedder prerenders with the limit.
TEST_P(PrerenderHostRegistryNewLimitAndSchedulerTest,
       NewLimitAndScheduler_Embedder) {
  // Starts the embedder prerenders as many times as the specific limit.
  for (int i = 0; i < MaxNumOfRunningEmbedderPrerenders(); i++) {
    FrameTreeNodeId frame_tree_node_id =
        CreateAndStartHostByLimitGroup(PrerenderLimitGroup::kEmbedder);
    EXPECT_TRUE(frame_tree_node_id);
  }

  // If we try to start embedder prerenders after reaching the limit, that
  // should be canceled with kMaxNumOfRunningEmbedderPrerendersExceeded.
  FrameTreeNodeId frame_tree_node_id_embedder_exceeded =
      CreateAndStartHostByLimitGroup(PrerenderLimitGroup::kEmbedder);
  EXPECT_TRUE(frame_tree_node_id_embedder_exceeded.is_null());
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded,
      embedder_histogram_suffix, 1);

  // On the other hand, prerenders belonging to different limit group(eager,
  // non-egaer) can still be started.
  FrameTreeNodeId frame_tree_node_id_eager = CreateAndStartHostByLimitGroup(
      PrerenderLimitGroup::kSpeculationRulesEager);
  FrameTreeNodeId frame_tree_node_id_non_eager = CreateAndStartHostByLimitGroup(
      PrerenderLimitGroup::kSpeculationRulesNonEager);
  EXPECT_TRUE(frame_tree_node_id_eager);
  EXPECT_TRUE(frame_tree_node_id_non_eager);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEagerPrerendersExceeded, 0);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningNonEagerPrerendersExceeded, 0);
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded,
      embedder_histogram_suffix, 1);
}

TEST_F(PrerenderHostRegistryTest,
       ReserveHostToActivateBeforeReadyForActivation) {
  const GURL original_url = contents()->GetLastCommittedURL();
  const GURL kPrerenderingUrl("https://example.com/next");

  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(prerender_frame_tree_node_id);
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);
  FrameTreeNode* ftn =
      FrameTreeNode::From(prerender_host->GetPrerenderedMainFrameHost());
  std::unique_ptr<NavigationSimulatorImpl> sim =
      NavigationSimulatorImpl::CreateFromPendingInFrame(ftn);
  // Ensure that navigation in prerendering frame tree does not commit and
  // PrerenderHost doesn't become ready for activation.
  sim->SetAutoAdvance(false);

  EXPECT_FALSE(prerender_host->is_ready_for_activation());

  test::PrerenderHostObserver prerender_host_observer(*contents(),
                                                      kPrerenderingUrl);

  // Start activation.
  std::unique_ptr<NavigationSimulatorImpl> navigation =
      CreateActivation(kPrerenderingUrl, *contents());
  navigation->Start();

  // Wait until PrerenderCommitDeferringCondition runs.
  // TODO(nhiroki): Avoid using base::RunUntilIdle() and instead use some
  // explicit signal of the running condition.
  base::RunLoop().RunUntilIdle();

  // The activation should be deferred by PrerenderCommitDeferringCondition
  // until the main frame navigation in the prerendering frame tree finishes.
  NavigationRequest* navigation_request = navigation->GetNavigationHandle();
  EXPECT_TRUE(
      navigation_request->IsCommitDeferringConditionDeferredForTesting());
  EXPECT_FALSE(prerender_host_observer.was_activated());
  EXPECT_EQ(contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
            original_url);

  // Finish the main frame navigation.
  sim->Commit();

  // Finish the activation.
  prerender_host_observer.WaitForDestroyed();
  EXPECT_TRUE(prerender_host_observer.was_activated());
  EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  EXPECT_EQ(contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

TEST_F(PrerenderHostRegistryTest, CancelHost) {
  const GURL kPrerenderingUrl("https://example.com/next");
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_NE(registry().FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

  registry().CancelHost(prerender_frame_tree_node_id,
                        PrerenderFinalStatus::kDestroyed);
  EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

// Test cancelling a prerender while a CommitDeferringCondition is running.
// This activation should fall back to a regular navigation.
TEST_F(PrerenderHostRegistryTest,
       CancelHostWhileCommitDeferringConditionIsRunning) {
  const GURL original_url = contents()->GetLastCommittedURL();

  // Start prerendering.
  const GURL kPrerenderingUrl("https://example.com/next");
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(prerender_frame_tree_node_id);
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  test::PrerenderHostObserver prerender_host_observer(*contents(),
                                                      kPrerenderingUrl);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. Use a CommitDeferringCondition to pause activation
  // before it completes.
  std::unique_ptr<NavigationSimulatorImpl> navigation;

  {
    MockCommitDeferringConditionInstaller installer(
        kPrerenderingUrl, CommitDeferringCondition::Result::kDefer);
    // Start trying to activate the prerendered page.
    navigation = CreateActivation(kPrerenderingUrl, *contents());
    navigation->Start();

    // Wait for the condition to pause the activation.
    installer.WaitUntilInstalled();
    installer.condition().WaitUntilInvoked();

    // The request should be deferred by the condition.
    auto* navigation_request =
        static_cast<NavigationRequest*>(navigation->GetNavigationHandle());
    EXPECT_TRUE(
        navigation_request->IsCommitDeferringConditionDeferredForTesting());

    // The primary page should still be the original page.
    EXPECT_EQ(contents()->GetLastCommittedURL(), original_url);

    // Cancel the prerender while the CommitDeferringCondition is running.
    registry().CancelHost(prerender_frame_tree_node_id,
                          PrerenderFinalStatus::kDestroyed);
    prerender_host_observer.WaitForDestroyed();
    EXPECT_FALSE(prerender_host_observer.was_activated());
    EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl), nullptr);

    // Resume the activation. This should fall back to a regular navigation.
    installer.condition().CallResumeClosure();
  }

  navigation->Commit();
  EXPECT_EQ(contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);
}

// Test cancelling a prerender and then starting a new prerender for the same
// URL while a CommitDeferringCondition is running. This activation should not
// reserve the second prerender and should fall back to a regular navigation.
TEST_F(PrerenderHostRegistryTest,
       CancelAndStartHostWhileCommitDeferringConditionIsRunning) {
  const GURL original_url = contents()->GetLastCommittedURL();
  const GURL kPrerenderingUrl("https://example.com/next");

  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(prerender_frame_tree_node_id);
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  test::PrerenderHostObserver prerender_host_observer(*contents(),
                                                      kPrerenderingUrl);

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender. Use a CommitDeferringCondition to pause activation
  // before it completes.
  std::unique_ptr<NavigationSimulatorImpl> navigation;
  base::OnceClosure resume_navigation;

  {
    MockCommitDeferringConditionInstaller installer(
        kPrerenderingUrl, CommitDeferringCondition::Result::kDefer);
    // Start trying to activate the prerendered page.
    navigation = CreateActivation(kPrerenderingUrl, *contents());
    navigation->Start();

    // Wait for the condition to pause the activation.
    installer.WaitUntilInstalled();
    installer.condition().WaitUntilInvoked();
    resume_navigation = installer.condition().TakeResumeClosure();

    // The request should be deferred by the condition.
    auto* navigation_request =
        static_cast<NavigationRequest*>(navigation->GetNavigationHandle());
    EXPECT_TRUE(
        navigation_request->IsCommitDeferringConditionDeferredForTesting());

    // The primary page should still be the original page.
    EXPECT_EQ(contents()->GetLastCommittedURL(), original_url);

    // Cancel the prerender while the CommitDeferringCondition is running.
    registry().CancelHost(prerender_frame_tree_node_id,
                          PrerenderFinalStatus::kDestroyed);
    prerender_host_observer.WaitForDestroyed();
    EXPECT_FALSE(prerender_host_observer.was_activated());
    EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
  }

  {
    // Start the second prerender for the same URL.
    const FrameTreeNodeId prerender_frame_tree_node_id2 =
        registry().CreateAndStartHost(GeneratePrerenderAttributes(
            kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kEager,
            contents()->GetPrimaryMainFrame()));
    ASSERT_TRUE(prerender_frame_tree_node_id2);
    PrerenderHost* prerender_host2 =
        registry().FindHostByUrlForTesting(kPrerenderingUrl);
    CommitPrerenderNavigation(*prerender_host2);

    EXPECT_NE(prerender_frame_tree_node_id, prerender_frame_tree_node_id2);
  }

  // Resume the initial activation. This should not reserve the second
  // prerender and should fall back to a regular navigation.
  std::move(resume_navigation).Run();
  navigation->Commit();
  EXPECT_EQ(contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
            kPrerenderingUrl);

  // The second prerender should still exist.
  EXPECT_NE(registry().FindHostByUrlForTesting(kPrerenderingUrl), nullptr);
}

// Tests that prerendering should be canceled if the trigger is in the
// background and its type is kEmbedder.
// For the case where the trigger type is speculation rules,
// browsertests `TestSequentialPrerenderingInBackground` covers it.
TEST_F(PrerenderHostRegistryTest,
       DontStartPrerenderWhenEmbedderTriggerIsAlreadyHidden) {
  // The visibility state to be HIDDEN will cause prerendering not started when
  // trigger type is kEmbedder.
  contents()->WasHidden();

  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  RenderFrameHostImpl* initiator_rfh = contents()->GetPrimaryMainFrame();
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kEmbedder, "DirectURLInput",
          std::nullopt, initiator_rfh));
  EXPECT_TRUE(prerender_frame_tree_node_id.is_null());
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  EXPECT_EQ(prerender_host, nullptr);
  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_DirectURLInput",
      PrerenderFinalStatus::kTriggerBackgrounded, 1u);
}

// -------------------------------------------------
// Activation navigation parameter matching unit tests.
// These tests change a parameter to differentiate the activation request from
// the prerendering request.

// A positive test to show that if the navigation params are equal then the
// prerender host is selected for activation.
TEST_F(PrerenderHostRegistryTest, SameInitialAndActivationParams) {
  EXPECT_TRUE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        // Do not change any params, so activation happens.
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kOk);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_InitiatorFrameToken) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const GURL kOriginalUrl("https://example.com/");
        navigation->SetInitiatorFrame(nullptr);
        navigation->set_initiator_origin(url::Origin::Create(kOriginalUrl));
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kInitiatorFrameToken);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_Headers) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_request_headers("User-Agent: Test");
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kHttpRequestHeader);
}

// Tests that the Purpose header is ignored when comparing request headers.
TEST_F(PrerenderHostRegistryTest, PurposeHeaderIsIgnoredForParamMatching) {
  EXPECT_TRUE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_request_headers("Purpose: Test");
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kOk);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_LoadFlags) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_ONLY_FROM_CACHE);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kLoadFlags);

  // If the potential activation request requires validation or bypass of the
  // browser cache, the prerendered page should not be activated.
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_VALIDATE_CACHE);
      })));
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_BYPASS_CACHE);
      })));
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_load_flags(net::LOAD_DISABLE_CACHE);
      })));
  ExpectBucketCountOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kCacheLoadFlags, 3);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SkipServiceWorker) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_skip_service_worker(true);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kSkipServiceWorker);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_MixedContentContextType) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_mixed_content_context_type(
            blink::mojom::MixedContentContextType::kNotMixedContent);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kMixedContentContextType);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_IsFormSubmission) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetIsFormSubmission(true);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kIsFormSubmission);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SearchableFormUrl) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const GURL kOriginalUrl("https://example.com/");
        navigation->set_searchable_form_url(kOriginalUrl);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kSearchableFormUrl);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationBeginParams_SearchableFormEncoding) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_searchable_form_encoding("Test encoding");
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kSearchableFormEncoding);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_InitiatorOrigin) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_initiator_origin(url::Origin());
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kInitiatorOrigin);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_ShouldNotCheckMainWorldCSP) {
  // Initial navigation blocked by the main world CSP cancels prerendering.
  // So, it's safe to match the page for CSP bypassing requests from isolated
  // worlds (e.g., extensions).
  EXPECT_TRUE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_should_check_main_world_csp(
            network::mojom::CSPDisposition::DO_NOT_CHECK);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kOk);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_Method) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetMethod("POST");
      })));
  // The method parameter change is detected as a HTTP request header change.
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kHttpRequestHeader);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_HrefTranslate) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_href_translate("test");
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kHrefTranslate);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_Transition) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->SetTransition(ui::PAGE_TRANSITION_FORM_SUBMIT);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kTransition);

  histogram_tester().ExpectUniqueSample(
      "Prerender.Experimental.ActivationTransitionMismatch.SpeculationRule",
      ui::PAGE_TRANSITION_FORM_SUBMIT, 1);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_RequestContextType) {
  EXPECT_FALSE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_request_context_type(
            blink::mojom::RequestContextType::AUDIO);
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kRequestContextType);
}

TEST_F(PrerenderHostRegistryTest,
       CompareInitialAndActivationCommonParams_ReferrerPolicy) {
  EXPECT_TRUE(CheckIsActivatedForParams(
      base::BindLambdaForTesting([&](NavigationSimulatorImpl* navigation) {
        navigation->SetReferrer(blink::mojom::Referrer::New(
            contents()->GetPrimaryMainFrame()->GetLastCommittedURL(),
            network::mojom::ReferrerPolicy::kAlways));
      })));
  ExpectUniqueSampleOfActivationNavigationParamsMatch(
      PrerenderHost::ActivationNavigationParamsMatch::kOk);
}

// End navigation parameter matching tests ---------

// Begin replication state matching tests ----------

TEST_F(PrerenderHostRegistryTest, InsecureRequestPolicyIsSetWhilePrerendering) {
  SetupPrerenderAndCommit(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_insecure_request_policy(
            blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent);
      }));
  EXPECT_EQ(contents()
                ->GetPrimaryMainFrame()
                ->frame_tree_node()
                ->current_replication_state()
                .insecure_request_policy,
            blink::mojom::InsecureRequestPolicy::kBlockAllMixedContent);
}

TEST_F(PrerenderHostRegistryTest,
       InsecureNavigationsSetIsSetWhilePrerendering) {
  SetupPrerenderAndCommit(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        const std::vector<uint32_t> insecure_navigations = {1, 2};
        navigation->set_insecure_navigations_set(insecure_navigations);
      }));
  const std::vector<uint32_t> insecure_navigations = {1, 2};
  EXPECT_EQ(contents()
                ->GetPrimaryMainFrame()
                ->frame_tree_node()
                ->current_replication_state()
                .insecure_navigations_set,
            insecure_navigations);
}

TEST_F(PrerenderHostRegistryTest,
       HasPotentiallyTrustworthyUniqueOriginIsSetWhilePrerendering) {
  SetupPrerenderAndCommit(
      base::BindLambdaForTesting([](NavigationSimulatorImpl* navigation) {
        navigation->set_has_potentially_trustworthy_unique_origin(true);
      }));
  EXPECT_TRUE(contents()
                  ->GetPrimaryMainFrame()
                  ->frame_tree_node()
                  ->current_replication_state()
                  .has_potentially_trustworthy_unique_origin);
}

// End replication state matching tests ------------

TEST_F(PrerenderHostRegistryTest, OneTaskToDeleteAllHosts) {
  std::vector<FrameTreeNodeId> frame_tree_node_ids;
  std::vector<std::unique_ptr<test::PrerenderHostObserver>>
      prerender_host_observers;

  for (int i = 0; i < 2; i++) {
    const GURL prerendering_url("https://example.com/next" +
                                base::NumberToString(i));
    FrameTreeNodeId frame_tree_node_id =
        registry().CreateAndStartHost(GeneratePrerenderAttributes(
            prerendering_url, PreloadingTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kEager,
            contents()->GetPrimaryMainFrame()));

    prerender_host_observers.emplace_back(
        std::make_unique<test::PrerenderHostObserver>(*contents(),
                                                      frame_tree_node_id));
    frame_tree_node_ids.push_back(frame_tree_node_id);
  }
  int pending_task_before_posting_abandon_task =
      task_environment()->GetPendingMainThreadTaskCount();
  registry().CancelHosts(
      frame_tree_node_ids,
      PrerenderCancellationReason(PrerenderFinalStatus::kDestroyed));
  int pending_task_after_posting_abandon_task =
      task_environment()->GetPendingMainThreadTaskCount();
  // Only one task was posted.
  EXPECT_EQ(pending_task_before_posting_abandon_task + 1,
            pending_task_after_posting_abandon_task);
  for (auto& observer : prerender_host_observers) {
    // All PrerenderHosts were deleted, so it should not timeout.
    observer->WaitForDestroyed();
  }
}

TEST_F(PrerenderHostRegistryTest, DisallowPageHavingEffectiveUrl_TriggerUrl) {
  const GURL original_url = contents()->GetLastCommittedURL();
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  // Let the trigger's URL have the effective URL.
  EffectiveURLContentBrowserClient modified_client(
      original_url, kModifiedSiteUrl,
      /*requires_dedicated_process=*/false);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&modified_client);

  // Start prerendering. This should fail as the initiator's URL has the
  // effective URL.
  const GURL kPrerenderingUrl("https://example.com/empty.html");
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_TRUE(prerender_frame_tree_node_id.is_null());
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  EXPECT_EQ(prerender_host, nullptr);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kTriggerUrlHasEffectiveUrl);

  SetBrowserClientForTesting(old_client);
}

TEST_F(PrerenderHostRegistryTest,
       DisallowPageHavingEffectiveUrl_PrerenderingUrl) {
  const GURL original_url = contents()->GetLastCommittedURL();
  const GURL kPrerenderingUrl("https://example.com/empty.html");
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  // Let the prerendering URL have the effective URL.
  EffectiveURLContentBrowserClient modified_client(
      kPrerenderingUrl, kModifiedSiteUrl,
      /*requires_dedicated_process=*/false);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&modified_client);

  // Start prerendering. This should fail as the prerendering URL has the
  // effective URL.
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_TRUE(prerender_frame_tree_node_id.is_null());
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  EXPECT_EQ(prerender_host, nullptr);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kPrerenderingUrlHasEffectiveUrl);

  SetBrowserClientForTesting(old_client);
}

TEST_F(PrerenderHostRegistryTest,
       DisallowPageHavingEffectiveUrl_ActivationUrl) {
  const GURL original_url = contents()->GetLastCommittedURL();
  const GURL kPrerenderingUrl("https://example.com/empty.html");
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  // Start prerendering.
  const FrameTreeNodeId prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PreloadingTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_TRUE(prerender_frame_tree_node_id);
  PrerenderHost* prerender_host =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);
  CommitPrerenderNavigation(*prerender_host);

  // Let the prerendering URL have the effective URL after prerendering.
  EffectiveURLContentBrowserClient modified_client(
      kPrerenderingUrl, kModifiedSiteUrl,
      /*requires_dedicated_process=*/false);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&modified_client);

  // Navigate the primary page to the prerendering URL that has the effective
  // URL. This should fail to activate the prerendered page.
  contents()->NavigateAndCommit(kPrerenderingUrl);

  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kActivationUrlHasEffectiveUrl);

  SetBrowserClientForTesting(old_client);
}

}  // namespace
}  // namespace content
