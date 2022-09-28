// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"

#include <algorithm>
#include <tuple>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/prefetch_metrics.h"
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

PrefetchDocumentManager::~PrefetchDocumentManager() {
  // On destruction, removes any owned prefetches from |PrefetchService|. Other
  // prefetches associated by |this| are owned by |PrefetchService| and can
  // still be used after the destruction of |this|.
  PrefetchService* prefetch_service = GetPrefetchService();
  if (!prefetch_service)
    return;

  for (const auto& prefetch_iter : owned_prefetches_) {
    prefetch_service->RemovePrefetch(
        prefetch_iter.second->GetPrefetchContainerKey());
  }
}

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

  // Create |PrefetchServingPageMetricsContainer| for potential navigation that
  // might use a prefetch, and update it with metrics from the page load
  // associated with |this|.
  PrefetchServingPageMetricsContainer* serving_page_metrics_container =
      PrefetchServingPageMetricsContainer::GetOrCreateForNavigationHandle(
          *navigation_handle);

  // Currently, prefetches can only be used with a navigation from the referring
  // page and in the same tab. Eventually we will support other types of
  // navigations where the prefetch is used in a different tab.
  serving_page_metrics_container->SetSameTabAsPrefetchingTab(true);

  // Get the prefetch for the URL being navigated to. If there is no prefetch
  // for that URL, then stop.
  auto prefetch_iter = all_prefetches_.find(navigation_handle->GetURL());
  if (prefetch_iter == all_prefetches_.end() || !prefetch_iter->second)
    return;

  // If this prefetch has already been used with another navigation then stop.
  if (prefetch_iter->second->HasPrefetchBeenConsideredToServe())
    return;

  serving_page_metrics_container->SetRequiredPrivatePrefetchProxy(
      prefetch_iter->second->GetPrefetchType().IsProxyRequired());
  serving_page_metrics_container->SetPrefetchHeaderLatency(
      prefetch_iter->second->GetPrefetchHeaderLatency());
  if (prefetch_iter->second->HasPrefetchStatus()) {
    serving_page_metrics_container->SetPrefetchStatus(
        prefetch_iter->second->GetPrefetchStatus());
  }

  // Inform |PrefetchService| of the navigation to the prefetch.
  GetPrefetchService()->PrepareToServe(prefetch_iter->second);
}

void PrefetchDocumentManager::ProcessCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates,
    base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer) {
  // Filter out candidates that can be handled by |PrefetchService| and
  // determine the type of prefetch required.
  // TODO(https://crbug.com/1299059): Once this code becomes enabled by default
  // to handle all prefetches and the prefetch proxy code in chrome/browser/ is
  // removed, then we can move the logic of which speculation candidates this
  // code can handle up a layer to |SpeculationHostImpl|.
  const url::Origin& referring_origin =
      render_frame_host().GetLastCommittedOrigin();

  std::vector<std::tuple<GURL, PrefetchType, blink::mojom::Referrer>>
      prefetches;

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
              PrefetchType(use_isolated_network_context, use_prefetch_proxy),
              *candidate->referrer);
          return true;
        }
        return false;
      };

  base::EraseIf(candidates, should_process_entry);

  if (const auto& host_to_bypass = PrefetchBypassProxyForHost()) {
    for (auto& [prefetch_url, prefetch_type, referrer] : prefetches) {
      if (prefetch_type.IsProxyRequired() &&
          prefetch_url.host() == *host_to_bypass)
        prefetch_type.SetProxyBypassedForTest();
    }
  }

  for (const auto& [prefetch_url, prefetch_type, referrer] : prefetches) {
    PrefetchUrl(prefetch_url, prefetch_type, referrer, devtools_observer);
  }
}

void PrefetchDocumentManager::PrefetchUrl(
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer) {
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
  auto container = std::make_unique<PrefetchContainer>(
      render_frame_host().GetGlobalId(), url, prefetch_type, referrer,
      weak_method_factory_.GetWeakPtr());
  container->SetDevToolsObserver(std::move(devtools_observer));
  base::WeakPtr<PrefetchContainer> weak_container = container->GetWeakPtr();
  owned_prefetches_[url] = std::move(container);
  all_prefetches_[url] = weak_container;

  referring_page_metrics_.prefetch_attempted_count++;

  // Send a reference of the new |PrefetchContainer| to |PrefetchService| to
  // start the prefetch process.
  GetPrefetchService()->PrefetchUrl(weak_container);
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

void PrefetchDocumentManager::OnEligibilityCheckComplete(bool is_eligible) {
  if (is_eligible)
    referring_page_metrics_.prefetch_eligible_count++;
}

void PrefetchDocumentManager::OnPrefetchSuccessful() {
  referring_page_metrics_.prefetch_successful_count++;
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
