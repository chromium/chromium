// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerenderer.h"

#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_navigation_utils.h"
#include "content/browser/preloading/prerender/prerender_new_tab_handle.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/web_contents.h"

namespace content {

class Prerenderer::PrerenderHostObserver : public PrerenderHost::Observer {
 public:
  explicit PrerenderHostObserver(PrerenderHost* prerender_host);
  ~PrerenderHostObserver() override;

  // PrerenderHost::Observer implementation:
  void OnHostDestroyed(PrerenderFinalStatus final_status) override;

  bool destroyed_by_memory_limit_exceeded() const {
    return destroyed_by_memory_limit_exceeded_;
  }

 private:
  bool destroyed_by_memory_limit_exceeded_ = false;
  base::ScopedObservation<PrerenderHost, PrerenderHost::Observer> observation_{
      this};
};

Prerenderer::PrerenderHostObserver::PrerenderHostObserver(
    PrerenderHost* prerender_host) {
  if (prerender_host)
    observation_.Observe(prerender_host);
}
Prerenderer::PrerenderHostObserver::~PrerenderHostObserver() = default;

void Prerenderer::PrerenderHostObserver::OnHostDestroyed(
    PrerenderFinalStatus final_status) {
  observation_.Reset();
  if (final_status == PrerenderFinalStatus::kMemoryLimitExceeded)
    destroyed_by_memory_limit_exceeded_ = true;
}

struct Prerenderer::PrerenderInfo {
  GURL url;
  Referrer referrer;
  int prerender_host_id;
};

Prerenderer::Prerenderer(content::RenderFrameHost& render_frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(&render_frame_host)),
      render_frame_host_(render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto& rfhi = static_cast<RenderFrameHostImpl&>(render_frame_host);
  registry_ = rfhi.delegate()->GetPrerenderHostRegistry()->GetWeakPtr();
}
Prerenderer::~Prerenderer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CancelStartedPrerenders();
}

void Prerenderer::PrimaryPageChanged(Page& page) {
  // Listen to the change of the primary page. Since only the primary page can
  // trigger speculationrules, the change of the primary page indicates that the
  // trigger associated with this host is destroyed, so the browser should
  // cancel the prerenders that are initiated by it.
  // We cannot do it in the destructor only, because DocumentService can be
  // deleted asynchronously, but we want to make sure to cancel prerendering
  // before the next primary page swaps in so that the next page can trigger a
  // new prerender without hitting the max number of running prerenders.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CancelStartedPrerenders();
}

// TODO(isaboori) Part of the logic in |ProcessCandidatesForPrerender| method is
// about making preloading decisions and could be moved to PreloadingDecider
// class.
void Prerenderer::ProcessCandidatesForPrerender(
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  if (!registry_)
    return;

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

  // Collects the host ids corresponding to the URLs that are removed from the
  // speculation rules. These hosts are cancelled later.
  std::vector<int> removed_prerender_rules;

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
        removed_prerender_rules.push_back(prerender.prerender_host_id);
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

  registry_->CancelHosts(
      removed_prerender_rules,
      PrerenderCancellationReason(PrerenderFinalStatus::kTriggerDestroyed));

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

    // TODO(crbug.com/1176054): Remove it after supporting cross-site
    // prerender.
    if (blink::features::
            IsSameSiteCrossOriginForSpeculationRulesPrerender2Enabled()) {
      if (!prerender_navigation_utils::IsSameSite(
              it->url, rfhi.GetLastCommittedOrigin())) {
        rfhi.AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kWarning,
            base::StringPrintf(
                "The SpeculationRules API does not support cross-site "
                "prerender yet "
                "(kSameSiteCrossOriginForSpeculationRulesPrerender2 is "
                "enabled). (initiator origin: %s, prerender origin: %s). "
                "https://crbug.com/1176054 tracks cross-site support.",
                rfhi.GetLastCommittedOrigin().Serialize().c_str(),
                url::Origin::Create(it->url).Serialize().c_str()));
      }
    } else {
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
    }

    Referrer referrer(*(it->referrer));
    PrerenderAttributes attributes(
        it->url, PrerenderTriggerType::kSpeculationRule,
        /*embedder_histogram_suffix=*/"", referrer,
        rfhi.GetLastCommittedOrigin(), rfhi.GetLastCommittedURL(),
        rfhi.GetProcess()->GetID(), rfhi.GetFrameToken(),
        rfhi.GetFrameTreeNodeId(), rfhi.GetPageUkmSourceId(),
        ui::PAGE_TRANSITION_LINK,
        /*url_match_predicate=*/absl::nullopt);

    // TODO(crbug.com/1354049): Handle the case where multiple speculation rules
    // have the same URL but its `target_browsing_context_name_hint` is
    // different. In the current implementation, only the first rule is
    // triggered.
    switch (it->target_browsing_context_name_hint) {
      case blink::mojom::SpeculationTargetHint::kBlank: {
        if (base::FeatureList::IsEnabled(
                blink::features::kPrerender2InNewTab)) {
          // `preloading_attempt` is not available for prerendering in a new tab
          // as it's associated with the current WebContents.
          // TODO(crbug.com/1350676): Create new PreloadAttempt associated with
          // WebContents for prerendering.
          int prerender_host_id =
              registry_->CreateAndStartHostForNewTab(attributes);
          started_prerenders_.insert(end,
                                     {.url = it->url,
                                      .referrer = referrer,
                                      .prerender_host_id = prerender_host_id});

          // TODO(crbug.com/1350676): Observe PrerenderHost created for
          // prerendering in a new tab like the kNoHint and kSelf cases.
          break;
        }
        // Handle the rule as kNoHint if the prerender-in-new-tab is not
        // enabled.
        [[fallthrough]];
      }
      case blink::mojom::SpeculationTargetHint::kNoHint:
      case blink::mojom::SpeculationTargetHint::kSelf: {
        int prerender_host_id = registry_->CreateAndStartHost(
            attributes, /*preloading_attempt=*/preloading_attempt);
        started_prerenders_.insert(end,
                                   {.url = it->url,
                                    .referrer = referrer,
                                    .prerender_host_id = prerender_host_id});

        // Start to observe PrerenderHost to get the information about
        // FinalStatus.
        observers_.push_back(std::make_unique<PrerenderHostObserver>(
            registry_->FindNonReservedHostById(prerender_host_id)));
        break;
      }
    }
  }
}

void Prerenderer::CancelStartedPrerenders() {
  // This function can be called twice and the histogram should be recorded in
  // the first call. Also, skip recording the histogram when no prerendering
  // starts.
  if (started_prerenders_.empty()) {
    DCHECK(observers_.empty());
    return;
  }

  // Record the percentage of destroyed prerenders due to the excessive memory
  // usage. `started_prerenders_` can include destroyed prerenders by other
  // reasons.
  // The closer the value is to 0, the less prerenders are cancelled by
  // FinalStatus::kMemoryLimitExceeded. The result depends on Finch params
  // `max_num_of_running_speculation_rules` and
  // `acceptable_percent_of_system_memory`.
  base::UmaHistogramPercentage(
      "Prerender.Experimental.CancellationPercentageByExcessiveMemoryUsage."
      "SpeculationRule",
      GetNumberOfDestroyedByMemoryExceeded() * 100 /
          started_prerenders_.size());

  if (registry_) {
    std::vector<int> started_prerender_ids;
    for (auto& prerender_info : started_prerenders_) {
      started_prerender_ids.push_back(prerender_info.prerender_host_id);
    }
    registry_->CancelHosts(
        started_prerender_ids,
        PrerenderCancellationReason(PrerenderFinalStatus::kTriggerDestroyed));
  }

  started_prerenders_.clear();
  observers_.clear();
}

int Prerenderer::GetNumberOfDestroyedByMemoryExceeded() {
  int destroyed_prerenders_by_memory_limit_exceeded = 0;
  for (auto& observer : observers_) {
    if (observer->destroyed_by_memory_limit_exceeded())
      destroyed_prerenders_by_memory_limit_exceeded++;
  }
  return destroyed_prerenders_by_memory_limit_exceeded;
}

}  // namespace content
