// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetcher.h"

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"

namespace content {

Prefetcher::Prefetcher(RenderFrameHost& render_frame_host)
    : render_frame_host_(render_frame_host),
      render_frame_host_impl_(
          static_cast<RenderFrameHostImpl*>(&render_frame_host)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_ = GetContentClient()->browser()->CreateSpeculationHostDelegate(
      render_frame_host);
}
Prefetcher::~Prefetcher() = default;

bool Prefetcher::IsPrefetchAttemptFailedOrDiscarded(const GURL& url) {
  PrefetchDocumentManager* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &render_frame_host());
  // TODO(isaboori): Implement |IsPrefetchAttemptFailed| for the delegate case.
  return prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(url);
}

void Prefetcher::ProcessCandidatesForPrefetch(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  PrefetchDocumentManager* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &render_frame_host());

  prefetch_document_manager->ProcessCandidates(candidates);

  // Let `delegate_` process the candidates that it is interested in.
  if (delegate_)
    delegate_->ProcessCandidates(candidates);
}

bool Prefetcher::MaybePrefetch(blink::mojom::SpeculationCandidatePtr candidate,
                               const PreloadingPredictor& enacting_predictor) {
  PrefetchDocumentManager* prefetch_document_manager =
      PrefetchDocumentManager::GetOrCreateForCurrentDocument(
          &render_frame_host());

  return prefetch_document_manager->MaybePrefetch(std::move(candidate),
                                                  enacting_predictor);
}

}  // namespace content
