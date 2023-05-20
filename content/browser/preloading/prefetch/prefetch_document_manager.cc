// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"

#include <algorithm>
#include <memory>
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
}  // namespace

PrefetchDocumentManager::PrefetchDocumentManager(RenderFrameHost* rfh)
    : DocumentUserData(rfh),
      WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
      no_vary_search_helper_(base::MakeRefCounted<NoVarySearchHelper>()) {}

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

void PrefetchDocumentManager::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Ignore navigations for a different RenderFrameHost.
  if (render_frame_host().GetGlobalId() !=
      navigation_handle->GetPreviousRenderFrameHostId()) {
    DVLOG(1) << "PrefetchDocumentManager::DidStartNavigation() for "
             << navigation_handle->GetURL()
             << ": skipped (different RenderFrameHost)";
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

  // Get the prefetch for the URL being navigated to. If there is no prefetch
  // for that URL, then check if there is an equivalent prefetch using
  // No-Vary-Search equivalence. If there is not then stop.
  auto prefetch_iter = all_prefetches_.find(navigation_handle->GetURL());
  if (prefetch_iter == all_prefetches_.end() || !prefetch_iter->second) {
    if (no_vary_search_support_enabled_ &&
        base::FeatureList::IsEnabled(
            network::features::kPrefetchNoVarySearch)) {
      const auto no_vary_search_match_url =
          GetNoVarySearchHelper().MatchUrl(navigation_handle->GetURL());
      if (no_vary_search_match_url.has_value()) {
        // Find the prefetched url matching |navigation_handle->GetURL()| based
        // on No-Vary-Search in |all_prefetches_|.
        prefetch_iter = all_prefetches_.find(no_vary_search_match_url.value());
      }
    }
  }
  if (prefetch_iter == all_prefetches_.end() || !prefetch_iter->second) {
    DVLOG(1) << "PrefetchDocumentManager::DidStartNavigation() for "
             << navigation_handle->GetURL()
             << ": skipped (PrefetchContainer not found)";
    SetMetricsForPossibleNoVarySearchHintMatches(
        all_prefetches_, navigation_handle->GetURL(),
        *serving_page_metrics_container);
    return;
  }

  // If this prefetch has already been used with another navigation then stop.
  if (prefetch_iter->second->HasPrefetchBeenConsideredToServe()) {
    DVLOG(1) << "PrefetchDocumentManager::DidStartNavigation() for "
             << *prefetch_iter->second
             << ": skipped (already used for another navigation)";
    SetMetricsForPossibleNoVarySearchHintMatches(
        all_prefetches_, navigation_handle->GetURL(),
        *serving_page_metrics_container);
    return;
  }

  prefetch_iter->second->SetServingPageMetrics(
      serving_page_metrics_container->GetWeakPtr());
  prefetch_iter->second->UpdateServingPageMetrics();

  // Inform |PrefetchService| of the navigation to the prefetch.
  // |navigation_handle->GetURL()| and |prefetched_iter->second->GetURL()|
  // might be different but be equivalent under No-Vary-Search.
  PrefetchService* prefetch_service = GetPrefetchService();
  if (prefetch_service) {
    prefetch_service->PrepareToServe(navigation_handle->GetURL(),
                                     prefetch_iter->second);
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
  net::SchemefulSite referring_site(
      render_frame_host().GetLastCommittedOrigin());

  std::vector<std::tuple<GURL, PrefetchType, blink::mojom::Referrer,
                         network::mojom::NoVarySearchPtr,
                         blink::mojom::SpeculationInjectionWorld>>
      prefetches;

  auto should_process_entry =
      [&](const blink::mojom::SpeculationCandidatePtr& candidate) {
        // This code doesn't not support speculation candidates with the action
        // of |blink::mojom::SpeculationAction::kPrefetchWithSubresources|. See
        // https://crbug.com/1296309.
        if (candidate->action != blink::mojom::SpeculationAction::kPrefetch) {
          return false;
        }

        net::SchemefulSite prefetch_site(candidate->url);

        prefetches.emplace_back(
            candidate->url,
            PrefetchType(
                /*use_prefetch_proxy=*/
                candidate->requires_anonymous_client_ip_when_cross_origin,
                candidate->eagerness),
            *candidate->referrer, candidate->no_vary_search_hint.Clone(),
            candidate->injection_world);
        return true;
      };

  base::EraseIf(candidates, should_process_entry);

  if (const auto& host_to_bypass = PrefetchBypassProxyForHost()) {
    for (auto& [prefetch_url, prefetch_type, referrer, no_vary_search_expected,
                world] : prefetches) {
      if (prefetch_type.IsProxyRequiredWhenCrossOrigin() &&
          prefetch_url.host() == *host_to_bypass) {
        prefetch_type.SetProxyBypassedForTest();
      }
    }
  }

  for (auto& [prefetch_url, prefetch_type, referrer, no_vary_search_expected,
              world] : prefetches) {
    PrefetchUrl(prefetch_url, prefetch_type, referrer, no_vary_search_expected,
                world, devtools_observer);
  }
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
        NoVarySearchHelper::ParseHttpNoVarySearchDataFromMojom(
            mojo_no_vary_search_expected);
  }
  // Create a new |PrefetchContainer| and take ownership of it
  auto container = std::make_unique<PrefetchContainer>(
      render_frame_host().GetGlobalId(), url, prefetch_type, referrer,
      std::move(no_vary_search_expected), world,
      weak_method_factory_.GetWeakPtr());
  container->SetDevToolsObserver(std::move(devtools_observer));
  if (base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)) {
    container->SetNoVarySearchHelper(no_vary_search_helper_);
  }
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

const NoVarySearchHelper& PrefetchDocumentManager::GetNoVarySearchHelper()
    const {
  return *no_vary_search_helper_.get();
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
  no_vary_search_helper_->MaybeSendErrorsToConsole(url, *head,
                                                   render_frame_host());
  no_vary_search_helper_->AddUrl(url, *head);
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
    GetPrefetchService()->EvictPrefetch(
        oldest_prefetch->GetPrefetchContainerKey());
    completed_non_eager_prefetches_.pop_front();
    return true;
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
