// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_

#include "base/scoped_observation.h"
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

  bool MaybePrerender(
      const blink::mojom::SpeculationCandidatePtr& candidate) override;

  bool ShouldWaitForPrerenderResult(const GURL& url) override;

  // Sets a callback from PreloadingDecider to notify the cancellation of
  // prerender to it.
  void SetPrerenderCancellationCallback(
      PrerenderCancellationCallback callback) override;

  // PrerenderHostRegistry::Observer implementations:
  void OnCancel(int host_frame_tree_node_id,
                const PrerenderCancellationReason& reason) override;
  void OnRegistryDestroyed() override;

 private:
  void CancelStartedPrerenders();

  // This is only used for metrics that count those prerenders per
  // primary page changed.
  void RecordReceivedPrerendersCountToMetrics();

  // This is kept sorted by URL.
  struct PrerenderInfo;
  std::vector<PrerenderInfo> started_prerenders_;

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_
