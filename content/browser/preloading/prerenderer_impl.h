// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_

#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class PrerenderHostRegistry;
class Page;

// Handles speculation-rules based prerenders.
class CONTENT_EXPORT PrerendererImpl : public Prerenderer, WebContentsObserver {
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

 private:
  void CancelStartedPrerenders();

  // This is kept sorted by URL.
  struct PrerenderInfo;
  std::vector<PrerenderInfo> started_prerenders_;

  base::WeakPtr<PrerenderHostRegistry> registry_;

  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_` safely.
  const raw_ref<RenderFrameHost> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_
