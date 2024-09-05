// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/origin.h"

namespace content {

namespace {
static PrefetchService* g_prefetch_service_for_testing = nullptr;

std::tuple<GURL,
           PrefetchType,
           blink::mojom::Referrer,
           network::mojom::NoVarySearchPtr>
SpeculationCandidateToPrefetchUrlParams(
    const blink::mojom::SpeculationCandidatePtr& candidate) {
  PrefetchType prefetch_type(
      PreloadingTriggerTypeFromSpeculationInjectionType(
          candidate->injection_type),
      /*use_prefetch_proxy=*/
      candidate->requires_anonymous_client_ip_when_cross_origin,
      candidate->eagerness);
  const GURL& prefetch_url = candidate->url;

  if (prefetch_type.IsProxyRequiredWhenCrossOrigin() &&
      ShouldPrefetchBypassProxyForTestHost(prefetch_url.host())) {
    // TODO(crbug.com/40942006): Remove SetProxyBypassedForTest, since it is the
    // only mutator of the PrefetchType.
    prefetch_type.SetProxyBypassedForTest();  // IN-TEST
  }

  return std::make_tuple(prefetch_url, prefetch_type, *candidate->referrer,
                         candidate->no_vary_search_hint.Clone());
}

}  // namespace

PrefetchDocumentManager::PrefetchDocumentManager(RenderFrameHost* rfh)
    : DocumentUserData(rfh),
      document_token_(
          static_cast<RenderFrameHostImpl*>(rfh)->GetDocumentToken()),
      prefetch_destruction_callback_(base::DoNothing()) {}

PrefetchDocumentManager::~PrefetchDocumentManager() {
  PrefetchService* prefetch_service = GetPrefetchService();
  if (!prefetch_service)
    return;

  // Invalidate weak pointers to `this` a little earlier to avoid callbacks to
  // `this` (especially `PrefetchWillBeDestroyed()`) during `ResetPrefetch()`
  // below.
  weak_method_factory_.InvalidateWeakPtrs();

  // On destruction, removes any prefetches that not yet start prefetching from
  // |PrefetchService|. Other already started prefetches associated by |this|
  // can still remain and be used after the destruction of |this|.
  for (const auto& prefetch_iter : all_prefetches_) {
    if (prefetch_iter.second) {
      switch (prefetch_iter.second->GetLoadState()) {
        case PrefetchContainer::LoadState::kNotStarted:
        case PrefetchContainer::LoadState::kEligible:
        case PrefetchContainer::LoadState::kFailedIneligible:
        case PrefetchContainer::LoadState::kFailedHeldback:
          prefetch_service->ResetPrefetch(prefetch_iter.second);
          break;
        case PrefetchContainer::LoadState::kStarted:
          break;
      }
    }
  }
}

// static
PrefetchDocumentManager* PrefetchDocumentManager::FromDocumentToken(
    int process_id,
    const blink::DocumentToken& document_token) {
  if (auto* rfh =
          RenderFrameHostImpl::FromDocumentToken(process_id, document_token)) {
    if (auto* prefetch_document_manager = GetForCurrentDocument(rfh)) {
      // A RenderFrameHost can have multiple Documents/PrefetchDocumentManagers
      // and the Document of `document_token` might be pending deletion or
      // bfcached, so check `document_token_` to confirm we get the correct
      // `PrefetchDocumentManager`.
      // TODO(crbug.com/40615943): clean this up once RenderDocument ships.
      if (prefetch_document_manager->document_token_ == document_token) {
        return prefetch_document_manager;
      }
    }
  }
  return nullptr;
}

void PrefetchDocumentManager::ProcessCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates,
    base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer) {
  // Filter out candidates that can be handled by |PrefetchService| and
  // determine the type of prefetch required.
  // TODO(crbug.com/40215782): Once this code becomes enabled by default
  // to handle all prefetches and the prefetch proxy code in chrome/browser/ is
  // removed, then we can move the logic of which speculation candidates this
  // code can handle up a layer to |SpeculationHostImpl|.
  std::vector<std::tuple<GURL, PrefetchType, blink::mojom::Referrer,
                         network::mojom::NoVarySearchPtr>>
      prefetches;

  // Evicts an existing prefetch if there is no longer a matching speculation
  // candidate for it. Note: A matching candidate is not necessarily the
  // candidate that originally triggered the prefetch, but is any prefetch
  // candidate that has the same URL.
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
    all_prefetches_.erase(prefetch->GetURL());
    switch (prefetch->GetLoadState()) {
      case PrefetchContainer::LoadState::kNotStarted:
      case PrefetchContainer::LoadState::kEligible:
      case PrefetchContainer::LoadState::kFailedIneligible:
      case PrefetchContainer::LoadState::kFailedHeldback:
        break;
      case PrefetchContainer::LoadState::kStarted:
        prefetch->SetPrefetchStatus(
            PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved);
        break;
    }
    GetPrefetchService()->ResetPrefetch(prefetch);
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

  std::erase_if(candidates, should_process_entry);

  for (auto& [prefetch_url, prefetch_type, referrer, no_vary_search_expected] :
       prefetches) {
    // Eager candidates are enacted by the same predictor that creates them.
    const PreloadingPredictor enacting_predictor =
        GetPredictorForPreloadingTriggerType(prefetch_type.trigger_type());
    PrefetchUrl(prefetch_url, prefetch_type, enacting_predictor,
                /*planned_max_preloading_type=*/PreloadingType::kPrefetch,
                referrer, no_vary_search_expected, devtools_observer);
  }

  if (PrefetchService* prefetch_service = GetPrefetchService()) {
    prefetch_service->OnCandidatesUpdated();
  }
}

bool PrefetchDocumentManager::MaybePrefetch(
    blink::mojom::SpeculationCandidatePtr candidate,
    const PreloadingPredictor& enacting_predictor,
    base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer) {
  if (candidate->action != blink::mojom::SpeculationAction::kPrefetch) {
    return false;
  }

  auto [prefetch_url, prefetch_type, referrer, no_vary_search_expected] =
      SpeculationCandidateToPrefetchUrlParams(candidate);
  PrefetchUrl(prefetch_url, prefetch_type, enacting_predictor,
              /*planned_max_preloading_type=*/PreloadingType::kPrefetch,
              referrer, no_vary_search_expected, devtools_observer);
  return true;
}

void PrefetchDocumentManager::PrefetchAheadOfPrerender(
    blink::mojom::SpeculationCandidatePtr candidate,
    const PreloadingPredictor& enacting_predictor) {
  auto [prefetch_url, prefetch_type, referrer, no_vary_search_expected] =
      SpeculationCandidateToPrefetchUrlParams(candidate);
  PrefetchUrl(prefetch_url, prefetch_type, enacting_predictor,
              /*planned_max_preloading_type=*/PreloadingType::kPrerender,
              referrer, no_vary_search_expected,
              // TODO(crbug.com/342537094): Emit CDP events for prefetch
              // ahead of prerender.
              /*devtools_observer=*/nullptr);
}

void PrefetchDocumentManager::PrefetchUrl(
    const GURL& url,
    const PrefetchType& prefetch_type,
    const PreloadingPredictor& enacting_predictor,
    PreloadingType planned_max_preloading_type,
    const blink::mojom::Referrer& referrer,
    const network::mojom::NoVarySearchPtr& mojo_no_vary_search_expected,
    base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer) {
  // Skip any prefetches that have already been requested.
  auto prefetch_container_iter = all_prefetches_.find(url);
  if (prefetch_container_iter != all_prefetches_.end() &&
      prefetch_container_iter->second != nullptr) {
    if (prefetch_container_iter->second->GetPrefetchType() != prefetch_type) {
      // TODO(crbug.com/40215782): Handle changing the PrefetchType of an
      // existing prefetch.
    }

    return;
  }

  // Log that a prefetch is occurring. Paths that reach this point go through
  // speculation rules in some form or another.
  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      &render_frame_host(),
      blink::mojom::WebFeature::kSpeculationRulesPrefetch);

  std::optional<net::HttpNoVarySearchData> no_vary_search_expected;
  if (mojo_no_vary_search_expected) {
    no_vary_search_expected =
        no_vary_search::ParseHttpNoVarySearchDataFromMojom(
            mojo_no_vary_search_expected);
  }
  PrefetchService* prefetch_service = GetPrefetchService();
  if (!prefetch_service) {
    return;
  }

  auto* web_contents = WebContents::FromRenderFrameHost(&render_frame_host());
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(web_contents);

  const PreloadingPredictor creating_predictor =
      GetPredictorForPreloadingTriggerType(prefetch_type.trigger_type());
  PreloadingURLMatchCallback matcher =
      PreloadingDataImpl::GetPrefetchServiceMatcher(
          *prefetch_service, PrefetchContainer::Key(document_token_, url));

  auto* attempt =
      static_cast<PreloadingAttemptImpl*>(preloading_data->AddPreloadingAttempt(
          creating_predictor, enacting_predictor, PreloadingType::kPrefetch,
          std::move(matcher), planned_max_preloading_type,
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()));

  attempt->SetSpeculationEagerness(prefetch_type.GetEagerness());
  CHECK(prefetch_type.GetEagerness() !=
            blink::mojom::SpeculationEagerness::kEager ||
        creating_predictor == enacting_predictor);

  // `PreloadingPrediction` is added in `PreloadingDecider`.

  // Create a new |PrefetchContainer| and take ownership of it
  auto container = std::make_unique<PrefetchContainer>(
      static_cast<RenderFrameHostImpl&>(render_frame_host()), document_token_,
      url, prefetch_type, referrer, std::move(no_vary_search_expected),
      weak_method_factory_.GetWeakPtr(), attempt->GetWeakPtr());
  container->SetDevToolsObserver(std::move(devtools_observer));
  DVLOG(1) << *container << ": created";
  all_prefetches_[url] = container->GetWeakPtr();

  referring_page_metrics_.prefetch_attempted_count++;

  // Send a reference of the new |PrefetchContainer| to |PrefetchService| to
  // start the prefetch process.
  prefetch_service->AddPrefetchContainer(std::move(container));
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
    case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
    case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchIneligibleExistingProxy:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchAllowed:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
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

void PrefetchDocumentManager::OnPrefetchSuccessful(
    PrefetchContainer* prefetch) {
  referring_page_metrics_.prefetch_successful_count++;
  if (prefetch->GetPrefetchType().GetEagerness() ==
      blink::mojom::SpeculationEagerness::kEager) {
    completed_eager_prefetches_.push_back(prefetch->GetWeakPtr());
  } else {
    completed_non_eager_prefetches_.push_back(prefetch->GetWeakPtr());
  }
}

std::tuple<bool, base::WeakPtr<PrefetchContainer>>
PrefetchDocumentManager::CanPrefetchNow(PrefetchContainer* prefetch) {
  RenderFrameHost* rfh = &render_frame_host();
  // The document needs to be active, primary and in a visible WebContents for
  // the prefetch to be eligible.
  if (!rfh->IsActive() || !rfh->GetPage().IsPrimary() ||
      WebContents::FromRenderFrameHost(rfh)->GetVisibility() !=
          Visibility::VISIBLE) {
    return std::make_tuple(false, nullptr);
  }
  if (prefetch->GetPrefetchType().GetEagerness() ==
      blink::mojom::SpeculationEagerness::kEager) {
    return std::make_tuple(completed_eager_prefetches_.size() <
                               MaxNumberOfEagerPrefetchesPerPage(),
                           nullptr);
  } else {
    if (completed_non_eager_prefetches_.size() <
        MaxNumberOfNonEagerPrefetchesPerPage()) {
      return std::make_tuple(true, nullptr);
    }
    // We are at capacity, and now need to evict the oldest non-eager prefetch
    // to make space for a new one.
    DCHECK(GetPrefetchService());
    base::WeakPtr<PrefetchContainer> oldest_prefetch =
        completed_non_eager_prefetches_.front();
    // TODO(crbug.com/40064525): We should also be checking if the prefetch is
    // currently being used to serve a navigation. In that scenario, evicting
    // doesn't make sense.
    return std::make_tuple(true, oldest_prefetch);
  }
}

void PrefetchDocumentManager::SetPrefetchDestructionCallback(
    PrefetchDestructionCallback callback) {
  prefetch_destruction_callback_ = std::move(callback);
}

void PrefetchDocumentManager::PrefetchWillBeDestroyed(
    PrefetchContainer* prefetch) {
  prefetch_destruction_callback_.Run(prefetch->GetURL());

  std::vector<base::WeakPtr<PrefetchContainer>>& completed_prefetches =
      prefetch->GetPrefetchType().GetEagerness() ==
              blink::mojom::SpeculationEagerness::kEager
          ? completed_eager_prefetches_
          : completed_non_eager_prefetches_;
  auto it = base::ranges::find(completed_prefetches, prefetch->key(),
                               [&](const auto& p) { return p->key(); });
  if (it != completed_prefetches.end()) {
    completed_prefetches.erase(it);
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
