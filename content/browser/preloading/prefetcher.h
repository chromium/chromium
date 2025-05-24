// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCHER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCHER_H_

#include "content/public/browser/speculation_host_delegate.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;
class PreloadingPredictor;

// Handles speculation-rules bases prefetches.
class CONTENT_EXPORT Prefetcher {
 public:
  Prefetcher() = delete;
  explicit Prefetcher(RenderFrameHost& render_frame_host);
  ~Prefetcher();

  RenderFrameHost& render_frame_host() const { return *render_frame_host_; }

  RenderFrameHostImpl* render_frame_host_impl() const {
    return render_frame_host_impl_;
  }

  void ProcessCandidatesForPrefetch(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  bool MaybePrefetch(blink::mojom::SpeculationCandidatePtr candidate,
                     const PreloadingPredictor& enacting_predictor);

  // Whether the prefetch attempt for target |url| failed or discarded.
  bool IsPrefetchAttemptFailedOrDiscarded(const GURL& url);

 private:
  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_` safely.
  const raw_ref<content::RenderFrameHost> render_frame_host_;

  // content::PreloadingDecider, which inherits content::DocumentUserData, owns
  // `this`, so `this` can access `render_frame_host_impl_` safely.
  const raw_ptr<content::RenderFrameHostImpl> render_frame_host_impl_;

  std::unique_ptr<SpeculationHostDelegate> delegate_;

  base::WeakPtrFactory<Prefetcher> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCHER_H_
