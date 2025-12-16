// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCHER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCHER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-forward.h"

class GURL;

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
  // PreloadingDecider, which inherits DocumentUserData, owns `this`, so `this`
  // can access `render_frame_host_` safely.
  const raw_ref<RenderFrameHost> render_frame_host_;

  // PreloadingDecider, which inherits DocumentUserData, owns `this`, so `this`
  // can access `render_frame_host_impl_` safely.
  const raw_ptr<RenderFrameHostImpl> render_frame_host_impl_;

  base::WeakPtrFactory<Prefetcher> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCHER_H_
