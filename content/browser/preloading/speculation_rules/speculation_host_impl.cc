// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/speculation_rules/speculation_host_impl.h"
#include <functional>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading//preloading.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

namespace {

bool CandidatesAreValid(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  for (const auto& candidate : candidates) {
    // These non-http candidates should be filtered out in Blink and
    // SpeculationHostImpl should not see them. If SpeculationHostImpl receives
    // non-http candidates, it may mean the renderer process has a bug
    // or is compromised.
    if (!candidate->url.SchemeIsHTTPOrHTTPS()) {
      mojo::ReportBadMessage("SH_NON_HTTP");
      return false;
    }
  }
  return true;
}

}  // namespace

struct SpeculationHostImpl::PrerenderInfo {
  GURL url;
  Referrer referrer;
  int prerender_host_id;
};

// static
void SpeculationHostImpl::Bind(
    RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver) {
  CHECK(frame_host);
  // TODO(crbug.com/1190338): Allow SpeculationHostDelegate to participate in
  // this feature check.
  if (!base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesPrefetchProxy) &&
      !blink::features::IsPrerender2Enabled()) {
    mojo::ReportBadMessage(
        "Speculation rules must be enabled to bind to "
        "blink.mojom.SpeculationHost in the browser.");
    return;
  }

  // DocumentService will destroy this on pipe closure or frame destruction.
  new SpeculationHostImpl(*frame_host, std::move(receiver));
}

SpeculationHostImpl::SpeculationHostImpl(
    RenderFrameHost& frame_host,
    mojo::PendingReceiver<blink::mojom::SpeculationHost> receiver)
    : DocumentService(frame_host, std::move(receiver)),
      WebContentsObserver(WebContents::FromRenderFrameHost(&frame_host)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = GetContentClient()->browser()->CreateSpeculationHostDelegate(
      render_frame_host());
  if (blink::features::IsPrerender2Enabled()) {
    auto& rfhi = static_cast<RenderFrameHostImpl&>(frame_host);
    registry_ = rfhi.delegate()->GetPrerenderHostRegistry()->GetWeakPtr();
  }
}

SpeculationHostImpl::~SpeculationHostImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CancelStartedPrerenders();
}

void SpeculationHostImpl::PrimaryPageChanged(Page& page) {
  // Listen to the change of the primary page. Since only the primary page can
  // trigger speculationrules, the change of the primary page indicates that the
  // trigger associated with this host is destroyed, so the browser should
  // cancel the prerenders that are initiated by it.
  // We cannot do it in the destructor only, because DocumentService can be
  // deleted asynchronously, but we want to make sure to cancel prerendering
  // before the next primary page swaps in so that the next page can trigger a
  // new prerender without hitting the max number of running prerenders.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CancelStartedPrerenders();
}

void SpeculationHostImpl::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr> candidates) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!CandidatesAreValid(candidates))
    return;

  // Only handle messages from an active main frame.
  if (!render_frame_host().IsActive())
    return;
  if (render_frame_host().GetParent())
    return;

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());

  for (const auto& candidate : candidates) {
    // Create new PreloadingPrediction class and pass all fields for all
    // candidates.

    // In case of speculation rules, the confidence is set as 100 as the URL
    // was not predicted and confidence in this case is not defined.
    int64_t confidence = 100;
    PreloadingURLMatchCallback same_url_matcher =
        PreloadingData::GetSameURLMatcher(candidate->url);

    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents);
    // TODO(crbug.com/1341019): Pass the action requested by speculation rules
    // to PreloadingPrediction.
    preloading_data->AddPreloadingPrediction(
        ToPreloadingPredictor(ContentPreloadingPredictor::kSpeculationRules),
        confidence, std::move(same_url_matcher));
  }

  if (base::FeatureList::IsEnabled(features::kPrefetchUseContentRefactor)) {
    PrefetchDocumentManager* prefetch_document_manager =
        PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            &render_frame_host());

    prefetch_document_manager->ProcessCandidates(
        candidates, weak_ptr_factory_.GetWeakPtr());
  }

  // Let `delegate_` process the candidates that it is interested in.
  if (delegate_)
    delegate_->ProcessCandidates(candidates, weak_ptr_factory_.GetWeakPtr());

  ProcessCandidatesForPrerender(candidates);
}

void SpeculationHostImpl::ProcessCandidatesForPrerender(
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  if (!registry_)
    return;
  DCHECK(blink::features::IsPrerender2Enabled());

  // Extract only the candidates which apply to prerender, and sort them by URL
  // so we can efficiently compare them to `started_prerenders_`.
  std::vector<blink::mojom::SpeculationCandidatePtr> prerender_candidates;
  for (const auto& candidate : candidates) {
    if (candidate->action == blink::mojom::SpeculationAction::kPrerender)
      prerender_candidates.push_back(candidate.Clone());
  }
  base::ranges::sort(prerender_candidates, std::less<>(),
                     &blink::mojom::SpeculationCandidate::url);
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates_to_start;

  // Compare the sorted candidate and started prerender lists to one another.
  // Since they are sorted, we process the lexicographically earlier of the two
  // URLs pointed at by the iterators, and compare the range of entries in each
  // that match that URL.
  //
  // URLs which are present in the prerender list but not the candidate list can
  // no longer proceed and are cancelled.
  //
  // URLs which are present in the candidate list but not the prerender list
  // could be started and are gathered in `candidates_to_start`.
  auto candidate_it = prerender_candidates.begin();
  auto started_it = started_prerenders_.begin();
  while (candidate_it != prerender_candidates.end() ||
         started_it != started_prerenders_.end()) {
    // Select the lesser of the two URLs to diff.
    GURL url;
    if (started_it == started_prerenders_.end())
      url = (*candidate_it)->url;
    else if (candidate_it == prerender_candidates.end())
      url = started_it->url;
    else
      url = std::min((*candidate_it)->url, started_it->url);

    // Select the ranges from both that match the URL in question.
    auto equal_prerender_end = base::ranges::find_if(
        started_it, started_prerenders_.end(),
        [&](const auto& started) { return started.url != url; });
    base::span<PrerenderInfo> matching_prerenders(started_it,
                                                  equal_prerender_end);
    auto equal_candidate_end = base::ranges::find_if(
        candidate_it, prerender_candidates.end(),
        [&](const auto& candidate) { return candidate->url != url; });
    base::span<blink::mojom::SpeculationCandidatePtr> matching_candidates(
        candidate_it, equal_candidate_end);

    // Decide what started prerenders to cancel.
    for (PrerenderInfo& prerender : matching_prerenders) {
      if (prerender.prerender_host_id == RenderFrameHost::kNoFrameTreeNodeId)
        continue;
      // TODO(jbroman): This doesn't currently care about other aspects, like
      // the referrer. This doesn't presently matter, but in the future we might
      // want to cancel if there are candidates which match by URL but none of
      // which permit this prerender.
      if (matching_candidates.empty()) {
        registry_->OnTriggerDestroyed(prerender.prerender_host_id);
        prerender.prerender_host_id = RenderFrameHost::kNoFrameTreeNodeId;
      }
    }

    // Decide what new candidates to start.
    // For now, start the first candidate for a URL only if there are no
    // matching prerenders. We could be cleverer in the future.
    if (matching_prerenders.empty()) {
      DCHECK_GT(matching_candidates.size(), 0u);
      candidates_to_start.push_back(std::move(matching_candidates[0]));
    }

    // Advance the iterators past all matching entries.
    candidate_it = equal_candidate_end;
    started_it = equal_prerender_end;
  }

  // Actually start the candidates once the diffing is done.
  auto& rfhi = static_cast<RenderFrameHostImpl&>(render_frame_host());
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  for (const auto& it : candidates_to_start) {
    DCHECK_EQ(it->action, blink::mojom::SpeculationAction::kPrerender);

    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents);

    // Create new PreloadingAttempt and pass all the values corresponding to
    // this prerendering attempt.
    PreloadingURLMatchCallback same_url_matcher =
        PreloadingData::GetSameURLMatcher(it->url);
    PreloadingAttempt* preloading_attempt =
        preloading_data->AddPreloadingAttempt(
            ToPreloadingPredictor(
                ContentPreloadingPredictor::kSpeculationRules),
            PreloadingType::kPrerender, std::move(same_url_matcher));

    auto [begin, end] = base::ranges::equal_range(
        started_prerenders_.begin(), started_prerenders_.end(), it->url,
        std::less<>(), &PrerenderInfo::url);
    DCHECK(begin == end)
        << "cannot currently start a second prerender with the same URL";

    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &rfhi, blink::mojom::WebFeature::kSpeculationRulesPrerender);

    // TODO(crbug.com/1176054): Remove it after supporting cross-origin
    // prerender.
    if (!rfhi.GetLastCommittedOrigin().IsSameOriginWith(it->url)) {
      rfhi.AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          base::StringPrintf(
              "The SpeculationRules API does not support cross-origin "
              "prerender yet. (initiator origin: %s, prerender origin: %s). "
              "https://crbug.com/1176054 tracks cross-origin support.",
              rfhi.GetLastCommittedOrigin().Serialize().c_str(),
              url::Origin::Create(it->url).Serialize().c_str()));
    }

    Referrer referrer(*(it->referrer));
    int prerender_host_id = registry_->CreateAndStartHost(
        PrerenderAttributes(it->url, PrerenderTriggerType::kSpeculationRule,
                            /*embedder_histogram_suffix=*/"", referrer,
                            rfhi.GetLastCommittedOrigin(),
                            rfhi.GetLastCommittedURL(),
                            rfhi.GetProcess()->GetID(), rfhi.GetFrameToken(),
                            rfhi.GetFrameTreeNodeId(),
                            rfhi.GetPageUkmSourceId(), ui::PAGE_TRANSITION_LINK,
                            /*url_match_predicate=*/absl::nullopt),
        *web_contents, /*preloading_attempt=*/preloading_attempt);
    started_prerenders_.insert(end, {.url = it->url,
                                     .referrer = referrer,
                                     .prerender_host_id = prerender_host_id});
  }
}

void SpeculationHostImpl::CancelStartedPrerenders() {
  if (registry_) {
    for (const auto& prerender : started_prerenders_) {
      int host_id = prerender.prerender_host_id;
      if (host_id != RenderFrameHost::kNoFrameTreeNodeId)
        registry_->OnTriggerDestroyed(host_id);
    }
    started_prerenders_.clear();
  }
}

void SpeculationHostImpl::OnStartSinglePrefetch(
    const std::string& request_id,
    const network::ResourceRequest& request) {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  devtools_instrumentation::OnPrefetchRequestWillBeSent(
      ftn, request_id, render_frame_host().GetLastCommittedURL(), request);
}

void SpeculationHostImpl::OnPrefetchResponseReceived(
    const GURL& url,
    const std::string& request_id,
    const network::mojom::URLResponseHead& response) {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  devtools_instrumentation::OnPrefetchResponseReceived(ftn, request_id, url,
                                                       response);
}

void SpeculationHostImpl::OnPrefetchRequestComplete(
    const std::string& request_id,
    const network::URLLoaderCompletionStatus& status) {
  auto* ftn = static_cast<RenderFrameHostImpl*>(&render_frame_host())
                  ->frame_tree_node();
  devtools_instrumentation::OnPrefetchRequestComplete(ftn, request_id, status);
}

}  // namespace content
