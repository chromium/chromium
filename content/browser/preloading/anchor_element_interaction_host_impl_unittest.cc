// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/anchor_element_interaction_host_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_decider.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-forward.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

class ScopedPreloadingDeciderObserver
    : public PreloadingDeciderObserverForTesting {
 public:
  explicit ScopedPreloadingDeciderObserver(RenderFrameHostImpl* rfh)
      : rfh_(rfh) {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(rfh_);
    old_observer_ = preloading_decider->SetObserverForTesting(this);
  }
  ~ScopedPreloadingDeciderObserver() override {
    auto* preloading_decider =
        PreloadingDecider::GetOrCreateForCurrentDocument(rfh_);
    EXPECT_EQ(this, preloading_decider->SetObserverForTesting(old_observer_));
  }

  void UpdateSpeculationCandidates(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates)
      override {}
  void OnPointerDown(const GURL& url) override { on_pointer_down_url_ = url; }
  void OnPointerHover(const GURL& url) override { on_pointer_hover_url_ = url; }

  std::optional<GURL> on_pointer_down_url_;
  std::optional<GURL> on_pointer_hover_url_;

 private:
  raw_ptr<RenderFrameHostImpl> rfh_;
  raw_ptr<PreloadingDeciderObserverForTesting> old_observer_;
};

using AnchorElementInteractionHostImplTest = RenderViewHostTestHarness;

TEST_F(AnchorElementInteractionHostImplTest, OnPointerEvents) {
  auto* render_frame_host = static_cast<RenderFrameHostImpl*>(main_rfh());

  mojo::Remote<blink::mojom::AnchorElementInteractionHost> remote;
  AnchorElementInteractionHostImpl::Create(render_frame_host,
                                           remote.BindNewPipeAndPassReceiver());

  ScopedPreloadingDeciderObserver observer(render_frame_host);
  observer.on_pointer_down_url_.reset();
  observer.on_pointer_hover_url_.reset();
  const auto pointer_down_url = GURL("www.example.com/page1.html");
  remote->OnPointerDown(pointer_down_url);
  remote.FlushForTesting();
  EXPECT_EQ(pointer_down_url, observer.on_pointer_down_url_);
  EXPECT_FALSE(observer.on_pointer_hover_url_.has_value());

  observer.on_pointer_down_url_.reset();
  observer.on_pointer_hover_url_.reset();
  const auto pointer_hover_url = GURL("www.example.com/page2.html");
  remote->OnPointerHover(
      pointer_hover_url,
      blink::mojom::AnchorElementPointerData::New(false, 0.0, 0.0));
  remote.FlushForTesting();
  EXPECT_FALSE(observer.on_pointer_down_url_.has_value());
  EXPECT_EQ(pointer_hover_url, observer.on_pointer_hover_url_);
}

TEST_F(AnchorElementInteractionHostImplTest, OnViewportHeuristicTriggered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kPreloadingViewportHeuristics);

  base::HistogramTester histogram_tester;
  auto* render_frame_host = static_cast<RenderFrameHostImpl*>(main_rfh());

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  PreloadingDecider::GetOrCreateForCurrentDocument(render_frame_host)
      ->UpdateSpeculationCandidates(candidates);

  mojo::Remote<blink::mojom::AnchorElementInteractionHost> remote;
  AnchorElementInteractionHostImpl::Create(render_frame_host,
                                           remote.BindNewPipeAndPassReceiver());

  const GURL url("https://example.com");
  remote->OnViewportHeuristicTriggered(url);
  remote.FlushForTesting();

  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(web_contents());
  EXPECT_EQ(preloading_data->GetPredictionsSizeForTesting(), 1u);

  std::unique_ptr<NavigationSimulator> navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  navigation_simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation_simulator->Start();

  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.ViewportHeuristic.Precision",
      PredictorConfusionMatrix::kTruePositive, 1);
  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.ViewportHeuristic.Recall",
      PredictorConfusionMatrix::kTruePositive, 1);
}

TEST_F(AnchorElementInteractionHostImplTest,
       RecallRecordedWhenViewportHeuristicIsNotTriggered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kPreloadingViewportHeuristics);

  base::HistogramTester histogram_tester;

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;
  PreloadingDecider::GetOrCreateForCurrentDocument(main_rfh())
      ->UpdateSpeculationCandidates(candidates);

  std::unique_ptr<NavigationSimulator> navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(GURL("https://example.com"),
                                                   main_rfh());
  navigation_simulator->SetTransition(ui::PAGE_TRANSITION_LINK);
  navigation_simulator->Start();

  histogram_tester.ExpectUniqueSample(
      "Preloading.Predictor.ViewportHeuristic.Recall",
      PredictorConfusionMatrix::kFalseNegative, 1);
}

}  // namespace
}  // namespace content
