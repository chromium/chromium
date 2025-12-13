// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_document_manager.h"

#include <tuple>

#include "base/containers/contains.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_handle_impl.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/network/public/mojom/no_vary_search.mojom.h"

namespace content {

namespace {
static PrefetchService* g_prefetch_service_for_testing = nullptr;

struct PrefetchUrlParams {
  explicit PrefetchUrlParams(
      const blink::mojom::SpeculationCandidatePtr& candidate)
      : prefetch_url(candidate->url),
        prefetch_type(PreloadingTriggerTypeFromSpeculationInjectionType(
                          candidate->injection_type),
                      /*use_prefetch_proxy=*/
                      candidate->requires_anonymous_client_ip_when_cross_origin,
                      candidate->eagerness),
        referrer(*candidate->referrer),
        no_vary_search_hint(candidate->no_vary_search_hint.Clone()),
        tags(candidate->tags.empty() ? std::nullopt
                                     : std::make_optional(candidate->tags)) {
    if (prefetch_type.IsProxyRequiredWhenCrossOrigin() &&
        ShouldPrefetchBypassProxyForTestHost(prefetch_url.GetHost())) {
      // TODO(crbug.com/40942006): Remove SetProxyBypassedForTest, since it is
      // the only mutator of the PrefetchType.
      prefetch_type.SetProxyBypassedForTest();  // IN-TEST
    }
  }

  GURL prefetch_url;
  PrefetchType prefetch_type;
  blink::mojom::Referrer referrer;
  network::mojom::NoVarySearchPtr no_vary_search_hint;
  std::optional<SpeculationRulesTags> tags;
};

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
  // `this` (especially `PrefetchWillBeDestroyed()`) during
  // `MayReleasePrefetch()` below.
  weak_method_factory_.InvalidateWeakPtrs();
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
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  // Filter out candidates that can be handled by |PrefetchService| and
  // determine the type of prefetch required.
  // TODO(crbug.com/40215782): Once this code becomes enabled by default
  // to handle all prefetches and the prefetch proxy code in chrome/browser/ is
  // removed, then we can move the logic of which speculation candidates this
  // code can handle up a layer to |SpeculationHostImpl|.
  std::vector<PrefetchUrlParams> prefetches;

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
  std::vector<std::pair<GURL, PreloadingType>> prefetches_to_evict;
  for (const auto& [all_prefetches_key, prefetch] : all_prefetches_) {
    const auto& [url, planned_max_preloading_type] = all_prefetches_key;

    // Don't evict prefetch ahead of prerender, which is initiated by
    // `PrerenderImpl`, as `PrefetchDocumentManager::ProcessCandidates()` is
    // only called for prefeches managed by `Prefetcher`.
    if (planned_max_preloading_type != PreloadingType::kPrefetch) {
      continue;
    }

    if (!base::Contains(url_set, url)) {
      static_cast<PrefetchHandleImpl*>(prefetch.get())
          ->SetPrefetchStatusOnReleaseStartedPrefetch(
              PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved);
      prefetches_to_evict.push_back(all_prefetches_key);
    }
  }
  for (const auto& all_prefetches_key : prefetches_to_evict) {
    all_prefetches_.erase(all_prefetches_key);
  }

  auto should_process_entry =
      [&](const blink::mojom::SpeculationCandidatePtr& candidate) {
        // This code doesn't not support speculation candidates with the action
        // of |blink::mojom::SpeculationAction::kPrefetchWithSubresources|. See
        // https://crbug.com/1296309.
        if (candidate->action != blink::mojom::SpeculationAction::kPrefetch) {
          return false;
        }

        prefetches.emplace_back(candidate);
        return true;
      };

  std::erase_if(candidates, should_process_entry);

  for (auto& [prefetch_url, prefetch_type, referrer, no_vary_search_hint,
              tags] : prefetches) {
    // Immediate candidates are enacted by the same predictor that creates them.
    const PreloadingPredictor enacting_predictor =
        GetPredictorForPreloadingTriggerType(prefetch_type.trigger_type());
    PrefetchUrl(prefetch_url, prefetch_type, enacting_predictor, referrer,
                std::move(tags), no_vary_search_hint,
                PreloadPipelineInfo::Create(
                    /*planned_max_preloading_type=*/PreloadingType::kPrefetch));
  }

  if (PrefetchService* prefetch_service = GetPrefetchService()) {
    prefetch_service->OnCandidatesUpdated();
  }
}

bool PrefetchDocumentManager::MaybePrefetch(
    blink::mojom::SpeculationCandidatePtr candidate,
    const PreloadingPredictor& enacting_predictor) {
  if (candidate->action != blink::mojom::SpeculationAction::kPrefetch) {
    return false;
  }

  PrefetchUrlParams params(candidate);
  PrefetchUrl(params.prefetch_url, params.prefetch_type, enacting_predictor,
              params.referrer, std::move(params.tags),
              params.no_vary_search_hint,
              PreloadPipelineInfo::Create(
                  /*planned_max_preloading_type=*/PreloadingType::kPrefetch));
  return true;
}

void PrefetchDocumentManager::PrefetchAheadOfPrerender(
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    blink::mojom::SpeculationCandidatePtr candidate,
    const PreloadingPredictor& enacting_predictor) {
  PrefetchUrlParams params(candidate);
  PrefetchUrl(params.prefetch_url, params.prefetch_type, enacting_predictor,
              params.referrer, std::move(params.tags),
              params.no_vary_search_hint, std::move(preload_pipeline_info));
}

void PrefetchDocumentManager::PrefetchUrl(
    const GURL& url,
    const PrefetchType& prefetch_type,
    const PreloadingPredictor& enacting_predictor,
    const blink::mojom::Referrer& referrer,
    std::optional<SpeculationRulesTags> speculation_rules_tags,
    const network::mojom::NoVarySearchPtr& mojo_no_vary_search_hint,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info) {
  const std::pair<GURL, PreloadingType> all_prefetches_key =
      std::make_pair(url, PreloadPipelineInfoImpl::From(*preload_pipeline_info)
                              .planned_max_preloading_type());

  // Skip prefetches that have already been requested.
  auto prefetch_container_iter = all_prefetches_.find(all_prefetches_key);
  if (prefetch_container_iter != all_prefetches_.end() &&
      static_cast<PrefetchHandleImpl*>(prefetch_container_iter->second.get())
          ->IsAlive()) {
    return;
  }

  // Log that a prefetch is occurring. Paths that reach this point go through
  // speculation rules in some form or another.
  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      &render_frame_host(),
      blink::mojom::WebFeature::kSpeculationRulesPrefetch);

  std::optional<net::HttpNoVarySearchData> no_vary_search_hint;
  if (mojo_no_vary_search_hint) {
    no_vary_search_hint = no_vary_search::ParseHttpNoVarySearchDataFromMojom(
        mojo_no_vary_search_hint);
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
          *prefetch_service, PrefetchKey(document_token_, url));

  auto* attempt =
      static_cast<PreloadingAttemptImpl*>(preloading_data->AddPreloadingAttempt(
          creating_predictor, enacting_predictor, PreloadingType::kPrefetch,
          std::move(matcher),
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()));

  attempt->SetSpeculationEagerness(prefetch_type.GetEagerness());
  CHECK(!IsImmediateSpeculationEagerness(prefetch_type.GetEagerness()) ||
        creating_predictor == enacting_predictor);

  // `PreloadingPrediction` is added in `PreloadingDecider`.

  auto request = PrefetchRequest::CreateRendererInitiated(
      static_cast<RenderFrameHostImpl&>(render_frame_host()), document_token_,
      url, prefetch_type, referrer, std::move(speculation_rules_tags),
      std::move(no_vary_search_hint), /*priority=*/std::nullopt,
      weak_method_factory_.GetWeakPtr(), std::move(preload_pipeline_info),
      attempt->GetWeakPtr());

  referring_page_metrics_.prefetch_attempted_count++;

  all_prefetches_[all_prefetches_key] =
      prefetch_service->AddPrefetchRequestWithHandle(std::move(request));
}

bool PrefetchDocumentManager::IsPrefetchAttemptFailedOrDiscarded(
    const GURL& url) {
  PrefetchService* prefetch_service = GetPrefetchService();
  if (!prefetch_service) {
    return true;
  }

  return prefetch_service->IsPrefetchAttemptFailedOrDiscardedInternal(
      base::PassKey<PrefetchDocumentManager>(),
      PrefetchKey(document_token_, url));
}

// static
void PrefetchDocumentManager::SetPrefetchServiceForTesting(
    PrefetchService* prefetch_service) {
  g_prefetch_service_for_testing = prefetch_service;
}

void PrefetchDocumentManager::ResetPrefetchAheadOfPrerenderIfExist(
    PreloadingType preloading_type,
    const GURL& url) {
  auto it = all_prefetches_.find(std::make_pair(url, preloading_type));
  if (it == all_prefetches_.end()) {
    return;
  }

  static_cast<PrefetchHandleImpl*>(it->second.get())
      ->SetPrefetchStatusOnReleaseStartedPrefetch(
          PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved);
  all_prefetches_.erase(it);
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
  if (IsImmediateSpeculationEagerness(
          prefetch->request().prefetch_type().GetEagerness())) {
    completed_immediate_prefetches_.push_back(prefetch->GetWeakPtr());
  } else {
    completed_non_immediate_prefetches_.push_back(prefetch->GetWeakPtr());
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
  if (IsImmediateSpeculationEagerness(
          prefetch->request().prefetch_type().GetEagerness())) {
    return std::make_tuple(completed_immediate_prefetches_.size() <
                               kMaxNumberOfImmediatePrefetchesPerPage,
                           nullptr);
  } else {
    if (completed_non_immediate_prefetches_.size() <
        kMaxNumberOfNonImmediatePrefetchesPerPage) {
      return std::make_tuple(true, nullptr);
    }
    // We are at capacity, and now need to evict the oldest non-immediate
    // prefetch to make space for a new one.
    DCHECK(GetPrefetchService());
    base::WeakPtr<PrefetchContainer> oldest_prefetch =
        completed_non_immediate_prefetches_.front();
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
      IsImmediateSpeculationEagerness(
          prefetch->request().prefetch_type().GetEagerness())
          ? completed_immediate_prefetches_
          : completed_non_immediate_prefetches_;
  auto it = std::ranges::find(completed_prefetches, prefetch->key(),
                              [&](const auto& p) { return p->key(); });
  if (it != completed_prefetches.end()) {
    completed_prefetches.erase(it);
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(PrefetchDocumentManager);

}  // namespace content
