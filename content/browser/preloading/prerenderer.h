// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDERER_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDERER_H_

#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"

namespace content {

class PrerenderHostRegistry;
class Page;

// Handles speculation-rules based prerenders.
class CONTENT_EXPORT Prerenderer : public WebContentsObserver {
 public:
  explicit Prerenderer(content::RenderFrameHost& render_frame_host);
  ~Prerenderer() override;

  // WebContentsObserver implementation:
  void PrimaryPageChanged(Page& page) override;

  RenderFrameHost& render_frame_host() const { return *render_frame_host_; }

  void ProcessCandidatesForPrerender(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

 private:
  void CancelStartedPrerenders();

  // Iterates started prerenders and counts how many of them were canceled
  // due to the excessive memory usage.
  int GetNumberOfDestroyedByMemoryExceeded();

  // TODO(https://crbug.com/1197133): Cancel started prerenders when candidates
  // are updated.
  // This is kept sorted by URL.
  struct PrerenderInfo;
  std::vector<PrerenderInfo> started_prerenders_;

  base::WeakPtr<PrerenderHostRegistry> registry_;

  class PrerenderHostObserver;
  std::vector<std::unique_ptr<PrerenderHostObserver>> observers_;

  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_` safely.
  const raw_ref<content::RenderFrameHost> render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDERER_H_