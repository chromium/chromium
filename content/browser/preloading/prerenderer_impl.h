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
  explicit PrerendererImpl(content::RenderFrameHost& render_frame_host);
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

  // Iterates started prerenders and counts how many of them were canceled
  // due to the excessive memory usage.
  int GetNumberOfDestroyedByMemoryExceeded();

  // TODO(https://crbug.com/1197133): Cancel started prerenders when candidates
  // are updated.
  // This is kept sorted by URL.
  struct PrerenderInfo;

  // Counts the historical non-new-tab prerenders.
  // TODO(crbug.com/1350676): Observe PrerenderHost created for
  // prerendering in a new tab so that this counter can take new-tab prerender
  // into account.
  int count_started_same_tab_prerenders_ = 0;
  std::vector<PrerenderInfo> started_prerenders_;

  base::WeakPtr<PrerenderHostRegistry> registry_;

  class PrerenderHostObserver;
  std::vector<std::unique_ptr<PrerenderHostObserver>> observers_;

  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_` safely.
  const raw_ref<content::RenderFrameHost> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDERER_IMPL_H_
