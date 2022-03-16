// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"

#include <algorithm>
#include <vector>

#include "content/browser/browser_context_impl.h"
#include "content/browser/speculation_rules/prefetch/prefetch_container.h"
#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

namespace content {

PrefetchDocumentManager::PrefetchDocumentManager(RenderFrameHost* rfh)
    : DocumentUserData(rfh) {}

PrefetchDocumentManager::~PrefetchDocumentManager() = default;

void PrefetchDocumentManager::ProcessCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  // Filter out candidates that can be handled by |PrefetchService| and
  // determine the type of prefetch required.
  // TODO(https://crbug.com/1299059): Once this code becomes enabled by default
  // to handle all prefetches and the prefetch proxy code in chrome/browser/ is
  // removed, then we can move the logic of which speculation candidates this
  // code can handle up a layer to |SpeculationHostImpl|.
  const url::Origin& referring_origin =
      render_frame_host().GetLastCommittedOrigin();

  std::vector<std::pair<GURL, PrefetchType>> prefetches;

  auto should_process_entry =
      [&](const blink::mojom::SpeculationCandidatePtr& candidate) {
        bool is_same_origin = referring_origin.IsSameOriginWith(candidate->url);
        bool private_prefetch =
            candidate->requires_anonymous_client_ip_when_cross_origin &&
            !is_same_origin;

        // This code doesn't not support speculation candidates with the action
        // of |blink::mojom::SpeculationAction::kPrefetchWithSubresources|. See
        // https://crbug.com/1296309.

        if (candidate->action == blink::mojom::SpeculationAction::kPrefetch) {
          bool use_isolated_network_context = !is_same_origin;
          bool use_prefetch_proxy = !is_same_origin && private_prefetch;
          prefetches.emplace_back(
              candidate->url,
              PrefetchType(use_isolated_network_context, use_prefetch_proxy));
          return true;
        }
        return false;
      };

  auto new_end = std::remove_if(candidates.begin(), candidates.end(),
                                should_process_entry);
  candidates.erase(new_end, candidates.end());

  for (const auto& prefetch : prefetches) {
    PrefetchUrl(prefetch.first, prefetch.second);
  }
}

void PrefetchDocumentManager::PrefetchUrl(const GURL& url,
                                          const PrefetchType& prefetch_type) {
  // Skip any prefetches that have already been requested.
  auto prefetch_container_iter = all_prefetches_.find(url);
  if (prefetch_container_iter != all_prefetches_.end() &&
      prefetch_container_iter->second != nullptr) {
    if (prefetch_container_iter->second->GetPrefetchType() != prefetch_type) {
      // TODO(https://crbug.com/1299059): Handle changing the PrefetchType of an
      // existing prefetch.
    }

    return;
  }

  // Create a new |PrefetchContainer| and take ownership of it
  owned_prefetches_[url] = std::make_unique<PrefetchContainer>(
      render_frame_host().GetGlobalId(), url, prefetch_type);
  all_prefetches_[url] = owned_prefetches_[url]->GetWeakPtr();

  // Send a reference of the new |PrefetchContainer| to |PrefetchService| to
  // start the prefetch process.
  DCHECK(BrowserContextImpl::From(render_frame_host().GetBrowserContext())
             ->GetPrefetchService());
  BrowserContextImpl::From(render_frame_host().GetBrowserContext())
      ->GetPrefetchService()
      ->PrefetchUrl(owned_prefetches_[url]->GetWeakPtr());

  // TODO(https://crbug.com/1299059): Track metrics about the prefetches.
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
