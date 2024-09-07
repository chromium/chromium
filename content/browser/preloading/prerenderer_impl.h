// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_

#include <tuple>

#include "base/scoped_observation.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class PrerenderHostRegistry;
class Page;

// Handles speculation-rules based prerenders.
class CONTENT_EXPORT PrerendererImpl : public Prerenderer,
                                       WebContentsObserver,
                                       PrerenderHostRegistry::Observer {
 public:
  explicit PrerendererImpl(RenderFrameHost& render_frame_host);
  ~PrerendererImpl() override;

  // WebContentsObserver implementation:
  void PrimaryPageChanged(Page& page) override;

  void ProcessCandidatesForPrerender(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates)
      override;

  bool MaybePrerender(const blink::mojom::SpeculationCandidatePtr& candidate,
                      const PreloadingPredictor& enacting_predictor,
                      PreloadingConfidence confidence) override;
  void OnLCPPredicted() override;

  bool ShouldWaitForPrerenderResult(const GURL& url) override;

  // Sets a callback from PreloadingDecider to notify the cancellation of
  // prerender to it.
  void SetPrerenderCancellationCallback(
      PrerenderCancellationCallback callback) override;

  // PrerenderHostRegistry::Observer implementations:
  void OnCancel(FrameTreeNodeId host_frame_tree_node_id,
                const PrerenderCancellationReason& reason) override;
  void OnRegistryDestroyed() override;

 private:
  struct PrerenderInfo;

  void CancelStartedPrerenders();

  // Used only for metric that counts received prerenders per
  // primary page changed.
  void RecordReceivedPrerendersCountToMetrics();
  void ResetReceivedPrerendersCountForMetrics();
  void IncrementReceivedPrerendersCountForMetrics(
      PreloadingTriggerType trigger_type,
      blink::mojom::SpeculationEagerness eagerness);

  // Kept sorted by URL.
  std::vector<PrerenderInfo> started_prerenders_;

  // Used only for metric that counts received prerenders per
  // primary page changed.
  base::flat_map<PreloadingTriggerType,
                 std::array<int,
                            static_cast<size_t>(
                                blink::mojom::SpeculationEagerness::kMaxValue) +
                                1>>
      received_prerenders_by_eagerness_;

  // Used to notify cancellation from PrerendererImpl to PreloadingDecider.
  // This is invoked in OnCancel, which is called when receiving a cancellation
  // notification from PrerenderHostRegistry.
  PrerenderCancellationCallback prerender_cancellation_callback_ =
      base::DoNothing();

  base::ScopedObservation<PrerenderHostRegistry,
                          PrerenderHostRegistry::Observer>
      observation_{this};

  base::WeakPtr<PrerenderHostRegistry> registry_;

  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_` safely.
  const raw_ref<RenderFrameHost> render_frame_host_;

  // Below two fields are used to defer starting prerenders until LCP timing
  // and are only used under LCPTimingPredictorPrerender2.
  bool blocked_ = false;
  using BlockedCandidateInfo =
      std::tuple<blink::mojom::SpeculationCandidatePtr /*candidate*/,
                 PreloadingPredictor /*enacting_predictor*/,
                 PreloadingConfidence /*confidence*/>;
  std::vector<BlockedCandidateInfo> blocked_candidates_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_
