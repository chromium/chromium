// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

namespace content {

namespace {
static PrefetchService* g_prefetch_service_for_testing = nullptr;

// Sets ServingPageMetrics for all prefetches that might match under
// No-Vary-Search hint.
void SetMetricsForPossibleNoVarySearchHintMatches(
    const std::map<GURL, base::WeakPtr<PrefetchContainer>>& all_prefetches,
    const GURL& nav_url,
    PrefetchServingPageMetricsContainer& serving_page_metrics_container) {
  for (const auto& itr : all_prefetches) {
    if (!itr.second) {
      continue;
    }
    if (!itr.second->HasPrefetchBeenConsideredToServe() &&
        itr.second->GetNoVarySearchHint() &&
        itr.second->GetNoVarySearchHint()->AreEquivalent(
            nav_url, itr.second->GetURL())) {
      // In this case we need to set serving page metrics in case we end up
      // using the prefetch after No-Vary-Search header is received.
      itr.second->SetServingPageMetrics(
          serving_page_metrics_container.GetWeakPtr());
      itr.second->UpdateServingPageMetrics();
    }
  }
}

std::tuple<GURL,
           PrefetchType,
           blink::mojom::Referrer,
           network::mojom::NoVarySearchPtr,
           blink::mojom::SpeculationInjectionWorld>
SpeculationCandidateToPrefetchUrlParams(
    const blink::mojom::SpeculationCandidatePtr& candidate) {
  PrefetchType prefetch_type = PrefetchType(
      /*use_prefetch_proxy=*/
      candidate->requires_anonymous_client_ip_when_cross_origin,
      candidate->eagerness);
  const GURL& prefetch_url = candidate->url;

  if (const auto& host_to_bypass = PrefetchBypassProxyForHost()) {
    if (prefetch_type.IsProxyRequiredWhenCrossOrigin() &&
        prefetch_url.host() == *host_to_bypass) {
      prefetch_type.SetProxyBypassedForTest();  // IN-TEST
    }
  }

  return std::make_tuple(prefetch_url, prefetch_type, *candidate->referrer,
                         candidate->no_vary_search_hint.Clone(),
                         candidate->injection_world);
}

}  // namespace

PrefetchDocumentManager::PrefetchDocumentManager(RenderFrameHost* rfh)
    : DocumentUserData(rfh),
      WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
      prefetch_destruction_callback_(base::DoNothing()) {}

PrefetchDocumentManager::~PrefetchDocumentManager() {
  // On destruction, removes any owned prefetches from |PrefetchService|. Other
  // prefetches associated by |this| are owned by |PrefetchService| and can
  // still be used after the destruction of |this|.
  PrefetchService* prefetch_service = GetPrefetchService();
  if (!prefetch_service)
    return;

  for (const auto& prefetch_iter : owned_prefetches_) {
    DCHECK(prefetch_iter.second);
    prefetch_service->RemovePrefetch(
        prefetch_iter.second->GetPrefetchContainerKey());
  }
}

base::WeakPtr<PrefetchContainer> PrefetchDocumentManager::MatchUrl(
    const GURL& url) const {
  return no_vary_search::MatchUrl(url, all_prefetches_);
}

std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
PrefetchDocumentManager::GetAllForUrlWithoutRefAndQueryForTesting(
    const GURL& url) const {
  return no_vary_search::GetAllForUrlWithoutRefAndQueryForTesting(
      url, all_prefetches_);
}

void PrefetchDocumentManager::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Ignore navigations for a different LocalFrameToken.
  // TODO(crbug.com/1431804, crbug.com/1431387): LocalFrameToken is used here
  // for scoping while RenderFrameHost's ID is used elsewhere. In the long term
  // we should fix this inconsistency, but the current code is at least not
  // worse than checking RenderFrameHostId here.
  if (render_frame_host().GetFrameToken() !=
      navigation_handle->GetInitiatorFrameToken()) {
    DVLOG(1) << "PrefetchDocumentManager::DidStartNavigation() for "
             << navigation_handle->GetURL()
             << ": skipped (different LocalFrameToken)";
    return;
  }

  // Ignores any same document navigations since we can't use prefetches to
  // speed them up.
  if (navigation_handle->IsSameDocument()) {
    DVLOG(1) << "PrefetchDocumentManager::DidStartNavigation() for "
             << navigation_handle->GetURL() << ": skipped (same document)";
    return;
  }

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

  base::WeakPtr<PrefetchContainer> prefetch_container = nullptr;

  // Get the prefetch for the URL being navigated to. If there is no prefetch
  // for that URL, then check if there is an equivalent prefetch using
  // No-Vary-Search equivalence. If there is not then stop.
  auto prefetch_iter = all_prefetches_.find(navigation_handle->GetURL());
  if (prefetch_iter != all_prefetches_.end()) {
    prefetch_container = prefetch_iter->second;
  }

  if (!prefetch_container && no_vary_search_support_enabled_ &&
      base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)) {
    prefetch_container = MatchUrl(navigation_handle->GetURL());
  }

  if (!prefetch_container) {
    DVLOG(1) << "PrefetchDocumentManager::DidStartNavigation() for "
             << navigation_handle->GetURL()
             << ": skipped (PrefetchContainer not found)";
    SetMetricsForPossibleNoVarySearchHintMatches(
        all_prefetches_, navigation_handle->GetURL(),
        *serving_page_metrics_container);
    return;
  }

  prefetch_container->SetServingPageMetrics(
      serving_page_metrics_container->GetWeakPtr());
  prefetch_container->UpdateServingPageMetrics();

  // Inform |PrefetchService| of the navigation to the prefetch.
  // |navigation_handle->GetURL()| and |prefetched_iter->second->GetURL()|
  // might be different but be equivalent under No-Vary-Search.
  PrefetchService* prefetch_service = GetPrefetchService();
  if (prefetch_service) {
    prefetch_service->PrepareToServe(navigation_handle->GetURL(),
                                     std::move(prefetch_container));
  }
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
  std::vector<std::tuple<GURL, PrefetchType, blink::mojom::Referrer,
                         network::mojom::NoVarySearchPtr,
                         blink::mojom::SpeculationInjectionWorld>>
      prefetches;

  // Evicts an existing prefetch if there is no longer a matching speculation
  // candidate for it. Note: A matching candidate is not necessarily the
  // candidate that originally triggered the prefetch, but is any prefetch
  // candidate that has the same URL.
  if (PrefetchNewLimitsEnabled()) {
    std::vector<GURL> urls_from_candidates;
    urls_from_candidates.reserve(candidates.size());
    for (const auto& candidate_ptr : candidates) {
      if (candidate_ptr->action == blink::mojom::SpeculationAction::kPrefetch) {
        urls_from_candidates.push_back(candidate_ptr->url);
      }
    }
    base::flat_set<GURL> url_set(std::move(urls_from_candidates));
    std::vector<base::WeakPtr<PrefetchContainer>> prefetches_to_evict;
    for (const auto& [url, prefetch] : all_prefetches_) {
      if (prefetch && !base::Contains(url_set, url)) {
        prefetches_to_evict.push_back(prefetch);
      }
    }
    for (const auto& prefetch : prefetches_to_evict) {
      EvictPrefetch(prefetch);
    }
  }

  auto should_process_entry =
      [&](const blink::mojom::SpeculationCandidatePtr& candidate) {
        // This code doesn't not support speculation candidates with the action
        // of |blink::mojom::SpeculationAction::kPrefetchWithSubresources|. See
        // https://crbug.com/1296309.
        if (candidate->action != blink::mojom::SpeculationAction::kPrefetch) {
          return false;
        }

        prefetches.push_back(
            SpeculationCandidateToPrefetchUrlParams(candidate));
        return true;
      };

  base::EraseIf(candidates, should_process_entry);

  for (auto& [prefetch_url, prefetch_type, referrer, no_vary_search_expected,
              world] : prefetches) {
    PrefetchUrl(prefetch_url, prefetch_type, referrer, no_vary_search_expected,
                world, devtools_observer);
  }
}

bool PrefetchDocumentManager::MaybePrefetch(
    blink::mojom::SpeculationCandidatePtr candidate,
    base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer) {
  if (candidate->action != blink::mojom::SpeculationAction::kPrefetch) {
    return false;
  }

  auto [prefetch_url, prefetch_type, referrer, no_vary_search_expected, world] =
      SpeculationCandidateToPrefetchUrlParams(candidate);
  PrefetchUrl(prefetch_url, prefetch_type, referrer, no_vary_search_expected,
              world, devtools_observer);
  return true;
}

void PrefetchDocumentManager::PrefetchUrl(
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    const network::mojom::NoVarySearchPtr& mojo_no_vary_search_expected,
    blink::mojom::SpeculationInjectionWorld world,
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

  absl::optional<net::HttpNoVarySearchData> no_vary_search_expected;
  if (mojo_no_vary_search_expected) {
    no_vary_search_expected =
        no_vary_search::ParseHttpNoVarySearchDataFromMojom(
            mojo_no_vary_search_expected);
  }
  // Create a new |PrefetchContainer| and take ownership of it
  auto container = std::make_unique<PrefetchContainer>(
      render_frame_host().GetGlobalId(), url, prefetch_type, referrer,
      std::move(no_vary_search_expected), world,
      weak_method_factory_.GetWeakPtr());
  container->SetDevToolsObserver(std::move(devtools_observer));
  DVLOG(1) << *container << ": created";
  base::WeakPtr<PrefetchContainer> weak_container = container->GetWeakPtr();
  owned_prefetches_[url] = std::move(container);
  all_prefetches_[url] = weak_container;

  referring_page_metrics_.prefetch_attempted_count++;

  // Send a reference of the new |PrefetchContainer| to |PrefetchService| to
  // start the prefetch process.
  PrefetchService* prefetch_service = GetPrefetchService();
  if (prefetch_service) {
    prefetch_service->PrefetchUrl(weak_container);
  }
}

std::unique_ptr<PrefetchContainer>
PrefetchDocumentManager::ReleasePrefetchContainer(const GURL& url) {
  DCHECK(owned_prefetches_.find(url) != owned_prefetches_.end());
  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::move(owned_prefetches_[url]);
  owned_prefetches_.erase(url);
  return prefetch_container;
}

bool PrefetchDocumentManager::IsPrefetchAttemptFailedOrDiscarded(
    const GURL& url) {
  auto it = all_prefetches_.find(url);
  if (it == all_prefetches_.end() || !it->second)
    return true;

  const auto& container = it->second;
  if (!container->HasPrefetchStatus())
    return false;  // the container is not processed yet

  switch (container->GetPrefetchStatus()) {
    case PrefetchStatus::kPrefetchSuccessful:
    case PrefetchStatus::kPrefetchResponseUsed:
      return false;
    case PrefetchStatus::kPrefetchNotEligibleUserHasCookies:
    case PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchProxyNotAvailable:
    case PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchNotEligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchNotEligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchNotEligibleExistingProxy:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord:
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchAllowed:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::kPrefetchFailedPerPageLimitExceeded:
    case PrefetchStatus::
        kPrefetchNotEligibleSameSiteCrossOriginPrefetchRequiredProxy:
    case PrefetchStatus::kPrefetchEvicted:
      return true;
  }
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

void PrefetchDocumentManager::OnPrefetchedHeadReceived(const GURL& url) {
  if (!no_vary_search_support_enabled_ ||
      !base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)) {
    return;
  }
  // Find the PrefetchContainer associated with |url|.
  const auto it = all_prefetches_.find(url);
  if (it == all_prefetches_.end() || !it->second) {
    return;
  }

  const auto* head = it->second->GetHead();
  DCHECK(head);
  no_vary_search::MaybeSendErrorsToConsole(url, *head, render_frame_host());
  no_vary_search::SetNoVarySearchData(it->second);
}

void PrefetchDocumentManager::OnPrefetchSuccessful(
    PrefetchContainer* prefetch) {
  referring_page_metrics_.prefetch_successful_count++;
  if (prefetch->GetPrefetchType().GetEagerness() ==
      blink::mojom::SpeculationEagerness::kEager) {
    number_eager_prefetches_completed_++;
  } else {
    completed_non_eager_prefetches_.push_back(prefetch->GetWeakPtr());
  }
}

void PrefetchDocumentManager::EnableNoVarySearchSupport() {
  no_vary_search_support_enabled_ = true;
}

bool PrefetchDocumentManager::CanPrefetchNow(PrefetchContainer* prefetch) {
  DCHECK(PrefetchNewLimitsEnabled());
  if (prefetch->GetPrefetchType().GetEagerness() ==
      blink::mojom::SpeculationEagerness::kEager) {
    // TODO(crbug.com/1445086): Implement eviction policies.
    return number_eager_prefetches_completed_ <
           MaxNumberOfEagerPrefetchesPerPageForPrefetchNewLimits();
  } else {
    base::EraseIf(completed_non_eager_prefetches_,
                  [&](const base::WeakPtr<PrefetchContainer>& prefetch) {
                    return !prefetch;
                  });
    if (completed_non_eager_prefetches_.size() <
        MaxNumberOfNonEagerPrefetchesPerPageForPrefetchNewLimits()) {
      return true;
    }
    // We are at capacity, and now need to evict the oldest non-eager prefetch
    // to make space for a new one.
    DCHECK(GetPrefetchService());
    base::WeakPtr<PrefetchContainer> oldest_prefetch =
        completed_non_eager_prefetches_.front();
    // TODO(crbug.com/1445086): We should also be checking if the prefetch is
    // currently being used to serve a navigation. In that scenario, evicting
    // doesn't make sense.
    EvictPrefetch(oldest_prefetch);
    completed_non_eager_prefetches_.pop_front();
    return true;
  }
}

void PrefetchDocumentManager::SetPrefetchDestructionCallback(
    PrefetchDestructionCallback callback) {
  prefetch_destruction_callback_ = std::move(callback);
}

void PrefetchDocumentManager::PrefetchWillBeDestroyed(
    PrefetchContainer* prefetch) {
  prefetch_destruction_callback_.Run(prefetch->GetURL());
}

void PrefetchDocumentManager::EvictPrefetch(
    base::WeakPtr<PrefetchContainer> prefetch) {
  DCHECK(prefetch);
  const GURL url = prefetch->GetURL();
  if (auto it = owned_prefetches_.find(url); it != owned_prefetches_.end()) {
    owned_prefetches_.erase(it);
  } else {
    DCHECK(GetPrefetchService());
    GetPrefetchService()->EvictPrefetch(prefetch->GetPrefetchContainerKey());
  }
  all_prefetches_.erase(url);
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
