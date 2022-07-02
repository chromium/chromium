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
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

namespace content {

namespace {
static PrefetchService* g_prefetch_service_for_testing = nullptr;
}  // namespace

PrefetchDocumentManager::PrefetchDocumentManager(RenderFrameHost* rfh)
    : DocumentUserData(rfh),
      WebContentsObserver(WebContents::FromRenderFrameHost(rfh)) {}

PrefetchDocumentManager::~PrefetchDocumentManager() = default;

void PrefetchDocumentManager::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Ignore navigations for a different RenderFrameHost.
  if (render_frame_host().GetGlobalId() !=
      navigation_handle->GetPreviousRenderFrameHostId())
    return;

  // Ignores any same document navigations since we can't use prefetches to
  // speed them up.
  if (navigation_handle->IsSameDocument())
    return;

  // Get the prefetch for the URL being navigated to. If there is no prefetch
  // for that URL, then stop.
  auto prefetch_iter = all_prefetches_.find(navigation_handle->GetURL());
  if (prefetch_iter == all_prefetches_.end())
    return;

  // Inform |PrefetchService| of the navigation to the prefetch.
  GetPrefetchService()->PrepareToServe(prefetch_iter->second);
}

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
      render_frame_host().GetGlobalId(), url, prefetch_type,
      weak_method_factory_.GetWeakPtr());
  all_prefetches_[url] = owned_prefetches_[url]->GetWeakPtr();

  // Send a reference of the new |PrefetchContainer| to |PrefetchService| to
  // start the prefetch process.
  GetPrefetchService()->PrefetchUrl(owned_prefetches_[url]->GetWeakPtr());

  // TODO(https://crbug.com/1299059): Track metrics about the prefetches.
}

std::unique_ptr<PrefetchContainer>
PrefetchDocumentManager::ReleasePrefetchContainer(const GURL& url) {
  DCHECK(owned_prefetches_.find(url) != owned_prefetches_.end());
  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::move(owned_prefetches_[url]);
  owned_prefetches_.erase(url);
  return prefetch_container;
}

// static
void PrefetchDocumentManager::SetPrefetchServiceForTesting(
    PrefetchService* prefetch_service) {
  g_prefetch_service_for_testing = prefetch_service;
}

PrefetchService* PrefetchDocumentManager::GetPrefetchService() const {
  if (g_prefetch_service_for_testing) {
    return g_prefetch_service_for_testing;
  }

  DCHECK(BrowserContextImpl::From(render_frame_host().GetBrowserContext())
             ->GetPrefetchService());
  return BrowserContextImpl::From(render_frame_host().GetBrowserContext())
      ->GetPrefetchService();
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
