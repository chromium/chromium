// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetcher.h"

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"

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
  if (base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor)) {
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            &render_frame_host());
    return prefetch_document_manager->IsPrefetchAttemptFailedOrDiscarded(url);
  }

  // TODO(isaboori): Implement |IsPrefetchAttemptFailed| for the delegate case.
  return true;
}

void Prefetcher::OnStartSinglePrefetch(
    const std::string& request_id,
    const network::ResourceRequest& request) {
  auto* ftn = render_frame_host_impl()->frame_tree_node();
  devtools_instrumentation::OnPrefetchRequestWillBeSent(
      ftn, request_id, render_frame_host().GetLastCommittedURL(), request);
}

void Prefetcher::OnPrefetchResponseReceived(
    const GURL& url,
    const std::string& request_id,
    const network::mojom::URLResponseHead& response) {
  auto* ftn = render_frame_host_impl()->frame_tree_node();
  devtools_instrumentation::OnPrefetchResponseReceived(ftn, request_id, url,
                                                       response);
}

void Prefetcher::OnPrefetchRequestComplete(
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  auto* ftn = render_frame_host_impl()->frame_tree_node();
  devtools_instrumentation::OnPrefetchRequestComplete(ftn, request_id, status);
}

void Prefetcher::OnPrefetchBodyDataReceived(const std::string& request_id,
                                            const std::string& body,
                                            bool is_base64_encoded) {
  auto* ftn = render_frame_host_impl()->frame_tree_node();
  devtools_instrumentation::OnPrefetchBodyDataReceived(ftn, request_id, body,
                                                       is_base64_encoded);
}

mojo::PendingRemote<network::mojom::DevToolsObserver>
Prefetcher::MakeSelfOwnedNetworkServiceDevToolsObserver() {
  auto* ftn = render_frame_host_impl()->frame_tree_node();
  return NetworkServiceDevToolsObserver::MakeSelfOwned(ftn);
}

void Prefetcher::ProcessCandidatesForPrefetch(
    const absl::optional<base::UnguessableToken>&
        initiator_devtools_navigation_token,
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  if (base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor)) {
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            &render_frame_host());

    prefetch_document_manager->ProcessCandidates(
        initiator_devtools_navigation_token, candidates,
        weak_ptr_factory_.GetWeakPtr());
  }

  // Let `delegate_` process the candidates that it is interested in.
  if (delegate_)
    delegate_->ProcessCandidates(candidates);
}

}  // namespace content
