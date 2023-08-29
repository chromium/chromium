// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_host_registry.h"

#include <cstdint>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_config.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
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

// This definition is needed because this constant is odr-used in gtest macros.
// https://en.cppreference.com/w/cpp/language/static#Constant_static_members
const int kNoFrameTreeNodeId = RenderFrameHost::kNoFrameTreeNodeId;

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
  PrerenderHostRegistryTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // TODO(crbug.com/1273341): remove the limitation and run tests with
        // multiple prerenders.
        {{blink::features::kPrerender2,
          {{"max_num_of_running_speculation_rules", "1"}}}},
        // Disable the memory requirement of Prerender2 so the test can run on
        // any bot.
        {blink::features::kPrerender2MemoryControls});
  }
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
        kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
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
    // Use is_potentially_prerendered_page_activation_for_testing() instead of
    // IsPrerenderedPageActivation() because the NavigationSimulator does not
    // proceed past CommitDeferringConditions on potential activations,
    // so IsPrerenderedPageActivation() will fail with a CHECK because
    // prerender_frame_tree_node_id_ is not populated.
    // TODO(https://crbug.com/1239220): Fix NavigationSimulator to wait for
    // commit deferring conditions as it does throttles.
    return navigation_request
        ->is_potentially_prerendered_page_activation_for_testing();
  }

  // Helper method to perform a prerender activation that includes specialized
  // handling or setup on the initial prerender navigation via the
  // setup_callback parameter.
  void SetupPrerenderAndCommit(
      base::OnceCallback<void(NavigationSimulatorImpl*)> setup_callback) {
    const GURL kPrerenderingUrl("https://example.com/next");
    const int prerender_frame_tree_node_id =
        registry().CreateAndStartHost(GeneratePrerenderAttributes(
            kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kEager,
            contents()->GetPrimaryMainFrame()));
    ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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
      PrerenderTriggerType trigger_type,
      const std::string& embedder_histogram_suffix,
      absl::optional<blink::mojom::SpeculationEagerness> eagerness,
      RenderFrameHostImpl* rfh) {
    switch (trigger_type) {
      case PrerenderTriggerType::kSpeculationRule:
      case PrerenderTriggerType::kSpeculationRuleFromIsolatedWorld:
        return PrerenderAttributes(
            url, trigger_type, embedder_histogram_suffix, Referrer(), eagerness,
            rfh->GetLastCommittedOrigin(), rfh->GetProcess()->GetID(),
            contents()->GetWeakPtr(), rfh->GetFrameToken(),
            rfh->GetFrameTreeNodeId(), rfh->GetPageUkmSourceId(),
            ui::PAGE_TRANSITION_LINK,
            /*url_match_predicate=*/absl::nullopt);
      case PrerenderTriggerType::kEmbedder:
        return PrerenderAttributes(
            url, trigger_type, embedder_histogram_suffix, Referrer(),
            /*eagerness=*/absl::nullopt, /*initiator_origin=*/absl::nullopt,
            /*initiator_process_id=*/ChildProcessHost::kInvalidUniqueID,
            contents()->GetWeakPtr(),
            /*initiator_frame_token=*/absl::nullopt,
            /*initiator_frame_tree_node_id=*/
            RenderFrameHost::kNoFrameTreeNodeId,
            /*initiator_ukm_id=*/ukm::kInvalidSourceId,
            ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
            /*url_match_predicate=*/absl::nullopt);
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
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<test::ScopedPrerenderWebContentsDelegate>
      web_contents_delegate_;
};

TEST_F(PrerenderHostRegistryTest, CreateAndStartHost_SpeculationRule) {
  const GURL kPrerenderingUrl("https://example.com/next");
  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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
  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kEmbedder, "DirectURLInput",
          absl::nullopt, contents()->GetPrimaryMainFrame()));
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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
      PreloadingType::kPrerender, std::move(same_url_matcher));
  const int prerender_frame_tree_node_id = registry().CreateAndStartHost(
      GeneratePrerenderAttributes(kPrerenderingUrl,
                                  PrerenderTriggerType::kSpeculationRule, "",
                                  blink::mojom::SpeculationEagerness::kEager,
                                  contents()->GetPrimaryMainFrame()),
      preloading_attempt);
  EXPECT_EQ(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
}

TEST_F(PrerenderHostRegistryTest,
       CreateAndStartHost_HoldbackOverride_Holdback) {
  const GURL kPrerenderingUrl("https://example.com/next");
  auto* preloading_data = PreloadingData::GetOrCreateForWebContents(contents());
  PreloadingURLMatchCallback same_url_matcher =
      PreloadingData::GetSameURLMatcher(kPrerenderingUrl);
  PreloadingAttempt* preloading_attempt = preloading_data->AddPreloadingAttempt(
      content_preloading_predictor::kSpeculationRules,
      PreloadingType::kPrerender, std::move(same_url_matcher));

  auto attributes = GeneratePrerenderAttributes(
      kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
      blink::mojom::SpeculationEagerness::kEager,
      contents()->GetPrimaryMainFrame());
  attributes.holdback_status_override = PreloadingHoldbackStatus::kHoldback;

  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(attributes, preloading_attempt);

  EXPECT_EQ(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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
      PreloadingType::kPrerender, std::move(same_url_matcher));

  auto attributes = GeneratePrerenderAttributes(
      kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
      blink::mojom::SpeculationEagerness::kEager,
      contents()->GetPrimaryMainFrame());
  attributes.holdback_status_override = PreloadingHoldbackStatus::kAllowed;

  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(attributes, preloading_attempt);

  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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

  const int frame_tree_node_id1 =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_NE(frame_tree_node_id1, RenderFrameHost::kNoFrameTreeNodeId);
  PrerenderHost* prerender_host1 =
      registry().FindHostByUrlForTesting(kPrerenderingUrl);

  // Start the prerender host for the same URL. This second host should be
  // ignored, and the first host should still be findable.
  const int frame_tree_node_id2 =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_EQ(frame_tree_node_id2, RenderFrameHost::kNoFrameTreeNodeId);
  EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl),
            prerender_host1);
  CommitPrerenderNavigation(*prerender_host1);

  contents()->ActivatePrerenderedPage(kPrerenderingUrl);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1.
TEST_F(PrerenderHostRegistryTest, NumberLimit_Activation) {
  // After the first prerender page was activated, PrerenderHostRegistry can
  // start prerendering a new one.
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");

  int frame_tree_node_id1 =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl1, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  int frame_tree_node_id2 =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl2, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  EXPECT_NE(frame_tree_node_id1, kNoFrameTreeNodeId);
  EXPECT_EQ(frame_tree_node_id2, kNoFrameTreeNodeId);

  // Activate the first prerender.
  PrerenderHost* prerender_host1 =
      registry().FindHostByUrlForTesting(kPrerenderingUrl1);
  CommitPrerenderNavigation(*prerender_host1);
  contents()->ActivatePrerenderedPage(kPrerenderingUrl1);

  // After the first prerender page was activated, PrerenderHostRegistry can
  // start prerendering a new one.
  frame_tree_node_id2 =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl2, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_NE(frame_tree_node_id2, kNoFrameTreeNodeId);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1, and new candidates can be processed after the initiator page navigates
// to a new same-origin page.
TEST_F(PrerenderHostRegistryTest, NumberLimit_SameOriginNavigateAway) {
  RenderFrameHostImpl* render_frame_host = contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");
  SendCandidates({kPrerenderingUrl1, kPrerenderingUrl2}, remote1);

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  ASSERT_NE(registry().FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  ASSERT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);

  // The initiator document navigates away.
  render_frame_host =
      NavigatePrimaryPage(contents(), GURL("https://example.com/elsewhere"));
  EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  SendCandidate(kPrerenderingUrl2, remote2);

  EXPECT_NE(registry().FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);
}

// Tests that PrerenderHostRegistry limits the number of started prerenders
// to 1, and new candidates can be processed after the initiator page navigates
// to a new cross-origin page.
TEST_F(PrerenderHostRegistryTest, NumberLimit_CrossOriginNavigateAway) {
  RenderFrameHostImpl* render_frame_host = contents()->GetPrimaryMainFrame();
  ASSERT_TRUE(render_frame_host);

  mojo::Remote<blink::mojom::SpeculationHost> remote1;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote1.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(remote1.is_connected());
  const GURL kPrerenderingUrl1("https://example.com/next1");
  const GURL kPrerenderingUrl2("https://example.com/next2");
  SendCandidates({kPrerenderingUrl1, kPrerenderingUrl2}, remote1);

  // PrerenderHostRegistry should only start prerendering for kPrerenderingUrl1.
  ASSERT_NE(registry().FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);
  ASSERT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl2), nullptr);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);

  // The initiator document navigates away to a cross-origin page.
  render_frame_host =
      NavigatePrimaryPage(contents(), GURL("https://example.org/"));
  EXPECT_EQ(registry().FindHostByUrlForTesting(kPrerenderingUrl1), nullptr);

  // After the initiator page navigates away, the started prerendering should be
  // cancelled, and PrerenderHostRegistry can start prerendering a new one.
  mojo::Remote<blink::mojom::SpeculationHost> remote2;
  SpeculationHostImpl::Bind(render_frame_host,
                            remote2.BindNewPipeAndPassReceiver());
  const GURL kPrerenderingUrl3("https://example.org/next1");
  SendCandidate(kPrerenderingUrl3, remote2);
  EXPECT_NE(registry().FindHostByUrlForTesting(kPrerenderingUrl3), nullptr);
  ExpectBucketCountOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded);
}

class PrerenderHostRegistryNewLimitAndSchedulerTest
    : public PrerenderHostRegistryTest {
 public:
  PrerenderHostRegistryNewLimitAndSchedulerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kPrerender2NewLimitAndScheduler,
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

  int CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup limit_group) {
    static int unique_id = 0;
    const GURL prerendering_url("https://example.com/next_" +
                                base::NumberToString(unique_id));
    unique_id++;
    switch (limit_group) {
      case PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesEager:
        return registry().CreateAndStartHost(GeneratePrerenderAttributes(
            prerendering_url, PrerenderTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kEager,
            contents()->GetPrimaryMainFrame()));
      case PrerenderHostRegistry::PrerenderLimitGroup::
          kSpeculationRulesNonEager:
        return registry().CreateAndStartHost(GeneratePrerenderAttributes(
            prerendering_url, PrerenderTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kModerate,
            contents()->GetPrimaryMainFrame()));
      case PrerenderHostRegistry::PrerenderLimitGroup::kEmbedder:
        return registry().CreateAndStartHost(GeneratePrerenderAttributes(
            prerendering_url, PrerenderTriggerType::kEmbedder,
            embedder_histogram_suffix, absl::nullopt, nullptr));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the behavior of eager prerenders with the new limit and scheduler.
TEST_F(PrerenderHostRegistryNewLimitAndSchedulerTest,
       NewLimitAndScheduler_Eager) {
  // Starts the eager prerenders as many times as the specific limit.
  for (int i = 0; i < MaxNumOfRunningSpeculationRulesEagerPrerenders(); i++) {
    int frame_tree_node_id = CreateAndStartHostByLimitGroup(
        PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesEager);
    EXPECT_NE(frame_tree_node_id, kNoFrameTreeNodeId);
  }

  // If we try to start eager prerenders after reaching the limit, that should
  // be canceled with kMaxNumOfRunningPrerendersExceeded.
  int frame_tree_node_id_eager_exceeded = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesEager);
  EXPECT_EQ(frame_tree_node_id_eager_exceeded, kNoFrameTreeNodeId);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);

  // On the other hand, prerenders belonging to different limit
  // group(non-eager, embedder) can still be started.
  int frame_tree_node_id_non_eager = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesNonEager);
  int frame_tree_node_id_embedder = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kEmbedder);
  EXPECT_NE(frame_tree_node_id_non_eager, kNoFrameTreeNodeId);
  EXPECT_NE(frame_tree_node_id_embedder, kNoFrameTreeNodeId);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded,
      embedder_histogram_suffix, 0);
}

// Tests the behavior of non-eager prerenders with the new limit and scheduler.
TEST_F(PrerenderHostRegistryNewLimitAndSchedulerTest,
       NewLimitAndScheduler_NonEager) {
  std::vector<int> started_prerender_ids;

  // Starts the non-eager prerenders as many times as the specific limit.
  for (int i = 0; i < MaxNumOfRunningSpeculationRulesNonEagerPrerenders();
       i++) {
    int frame_tree_node_id = CreateAndStartHostByLimitGroup(
        PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesNonEager);
    started_prerender_ids.push_back(frame_tree_node_id);
    EXPECT_NE(frame_tree_node_id, kNoFrameTreeNodeId);
  }

  // Even after the limit of non-eager speculation rules is reached, it is
  // permissible to start a new prerender. Instead, the oldest prerender will be
  // canceled with kMaxNumOfRunningPrerendersExceeded to make room for a new
  // one.
  int frame_tree_node_id_non_eager_exceeded = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesNonEager);
  ASSERT_NE(frame_tree_node_id_non_eager_exceeded, kNoFrameTreeNodeId);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
  for (auto id : started_prerender_ids) {
    PrerenderHost* prerender_host = registry().FindNonReservedHostById(id);
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
  int frame_tree_node_id_eager = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesEager);
  int frame_tree_node_id_embedder = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kEmbedder);
  EXPECT_NE(frame_tree_node_id_eager, kNoFrameTreeNodeId);
  EXPECT_NE(frame_tree_node_id_embedder, kNoFrameTreeNodeId);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 1);
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded,
      embedder_histogram_suffix, 0);
}

// Tests the behavior of embedder prerenders with the limit.
TEST_F(PrerenderHostRegistryNewLimitAndSchedulerTest,
       NewLimitAndScheduler_Embedder) {
  // Starts the embedder prerenders as many times as the specific limit.
  for (int i = 0; i < MaxNumOfRunningEmbedderPrerenders(); i++) {
    int frame_tree_node_id = CreateAndStartHostByLimitGroup(
        PrerenderHostRegistry::PrerenderLimitGroup::kEmbedder);
    EXPECT_NE(frame_tree_node_id, kNoFrameTreeNodeId);
  }

  // If we try to start embedder prerenders after reaching the limit, that
  // should be canceled with kMaxNumOfRunningPrerendersExceeded.
  int frame_tree_node_id_embedder_exceeded = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kEmbedder);
  EXPECT_EQ(frame_tree_node_id_embedder_exceeded, kNoFrameTreeNodeId);
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded,
      embedder_histogram_suffix, 1);

  // On the other hand, prerenders belonging to different limit group(eager,
  // non-egaer) can still be started.
  int frame_tree_node_id_eager = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesEager);
  int frame_tree_node_id_non_eager = CreateAndStartHostByLimitGroup(
      PrerenderHostRegistry::PrerenderLimitGroup::kSpeculationRulesNonEager);
  EXPECT_NE(frame_tree_node_id_eager, kNoFrameTreeNodeId);
  EXPECT_NE(frame_tree_node_id_non_eager, kNoFrameTreeNodeId);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded, 0);
  ExpectUniqueSampleOfEmbedderFinalStatus(
      PrerenderFinalStatus::kMaxNumOfRunningPrerendersExceeded,
      embedder_histogram_suffix, 1);
}

TEST_F(PrerenderHostRegistryTest,
       ReserveHostToActivateBeforeReadyForActivation) {
  const GURL original_url = contents()->GetLastCommittedURL();
  const GURL kPrerenderingUrl("https://example.com/next");

  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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
  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
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
  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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

  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  ASSERT_NE(prerender_frame_tree_node_id, kNoFrameTreeNodeId);
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
    const int prerender_frame_tree_node_id2 =
        registry().CreateAndStartHost(GeneratePrerenderAttributes(
            kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
            blink::mojom::SpeculationEagerness::kEager,
            contents()->GetPrimaryMainFrame()));
    ASSERT_NE(prerender_frame_tree_node_id2, kNoFrameTreeNodeId);
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
  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kEmbedder, "DirectURLInput",
          absl::nullopt, initiator_rfh));
  EXPECT_EQ(prerender_frame_tree_node_id, RenderFrameHost::kNoFrameTreeNodeId);
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

TEST_F(PrerenderHostRegistryTest, DisallowPageHavingEffectiveUrl) {
  const GURL original_url = contents()->GetLastCommittedURL();
  const GURL kModifiedSiteUrl("custom-scheme://custom");

  EffectiveURLContentBrowserClient modified_client(
      original_url, kModifiedSiteUrl,
      /* requires_dedicated_process */ false);
  ContentBrowserClient* old_client =
      SetBrowserClientForTesting(&modified_client);

  const GURL kPrerenderingUrl = GURL("https://example.com/empty.html");
  const int prerender_frame_tree_node_id =
      registry().CreateAndStartHost(GeneratePrerenderAttributes(
          kPrerenderingUrl, PrerenderTriggerType::kSpeculationRule, "",
          blink::mojom::SpeculationEagerness::kEager,
          contents()->GetPrimaryMainFrame()));
  EXPECT_EQ(prerender_frame_tree_node_id, RenderFrameHost::kNoFrameTreeNodeId);
  PrerenderHost* prerender_host =
      registry().FindNonReservedHostById(prerender_frame_tree_node_id);
  EXPECT_EQ(prerender_host, nullptr);
  ExpectUniqueSampleOfSpeculationRuleFinalStatus(
      PrerenderFinalStatus::kHasEffectiveUrl);

  SetBrowserClientForTesting(old_client);
}

// End replication state matching tests ------------

}  // namespace
}  // namespace content
