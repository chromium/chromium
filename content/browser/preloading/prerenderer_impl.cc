// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerenderer_impl.h"

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_metrics.h"
#include "content/browser/preloading/prerender/prerender_navigation_utils.h"
#include "content/browser/preloading/prerender/prerender_new_tab_handle.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_util.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
PreloadingType ConvertSpeculationActionToPreloadingType(
    blink::mojom::SpeculationAction action) {
  switch (action) {
    case blink::mojom::SpeculationAction::kPrerender:
      return PreloadingType::kPrerender;
    case blink::mojom::SpeculationAction::kPrerenderUntilScript:
      return PreloadingType::kPrerenderUntilScript;
    case blink::mojom::SpeculationAction::kPrefetch:
    case blink::mojom::SpeculationAction::kPrefetchWithSubresources:
      NOTREACHED();
  }
}

}  // namespace

// TODO(crbug.com/428500219): We should allow prerender-until-script to be
// upgraded to prerender.
struct PrerendererImpl::PrerenderInfo {
  blink::mojom::SpeculationInjectionType injection_type;
  blink::mojom::SpeculationEagerness eagerness;
  blink::mojom::SpeculationAction action;
  bool is_target_blank;
  FrameTreeNodeId prerender_host_id;
  GURL url;

  PrerenderInfo() = default;
  explicit PrerenderInfo(
      const blink::mojom::SpeculationCandidatePtr& candidate);

  static bool PrerenderInfoComparator(const PrerenderInfo& p1,
                                      const PrerenderInfo& p2);

  bool operator<(const PrerenderInfo& p) const {
    return PrerenderInfoComparator(*this, p);
  }

  bool operator==(const PrerenderInfo& p) const {
    return !PrerenderInfoComparator(*this, p) &&
           !PrerenderInfoComparator(p, *this);
  }
};

bool PrerendererImpl::PrerenderInfo::PrerenderInfoComparator(
    const PrerenderInfo& p1,
    const PrerenderInfo& p2) {
  if (p1.url != p2.url) {
    return p1.url < p2.url;
  }

  return p1.is_target_blank < p2.is_target_blank;
}

// `prerender_host_id` is not provided by `SpeculationCandidatePtr`, so
// FrameTreeNodeId() is assigned instead. The value should be updated once it is
// available.
PrerendererImpl::PrerenderInfo::PrerenderInfo(
    const blink::mojom::SpeculationCandidatePtr& candidate)
    : injection_type(candidate->injection_type),
      eagerness(candidate->eagerness),
      action(candidate->action),
      is_target_blank(candidate->target_browsing_context_name_hint ==
                      blink::mojom::SpeculationTargetHint::kBlank),
      url(candidate->url) {}

PrerendererImpl::PrerendererImpl(RenderFrameHost& render_frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(&render_frame_host)),
      render_frame_host_(render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& rfhi = static_cast<RenderFrameHostImpl&>(render_frame_host);
  registry_ = rfhi.delegate()->GetPrerenderHostRegistry()->GetWeakPtr();
  if (registry_) {
    observation_.Observe(registry_.get());
  }
  ResetReceivedPrerendersCountForMetrics();
  if (base::FeatureList::IsEnabled(
          blink::features::kLCPTimingPredictorPrerender2)) {
    blocked_ = true;
  }
}

PrerendererImpl::~PrerendererImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CancelStartedPrerenders();
  RecordReceivedPrerendersCountToMetrics();
  ResetReceivedPrerendersCountForMetrics();
}

void PrerendererImpl::PrimaryPageChanged(Page& page) {
  // Listen to the change of the primary page. Since only the primary page can
  // trigger speculationrules, the change of the primary page indicates that the
  // trigger associated with this host is destroyed, so the browser should
  // cancel the prerenders that are initiated by it.
  // We cannot do it in the destructor only, because DocumentService can be
  // deleted asynchronously, but we want to make sure to cancel prerendering
  // before the next primary page swaps in so that the next page can trigger a
  // new prerender without hitting the max number of running prerenders.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CancelStartedPrerenders();
  RecordReceivedPrerendersCountToMetrics();
  ResetReceivedPrerendersCountForMetrics();
}

// TODO(isaboori) Part of the logic in |ProcessCandidatesForPrerender| method is
// about making preloading decisions and could be moved to PreloadingDecider
// class.
void PrerendererImpl::ProcessCandidatesForPrerender(
    const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates,
    bool enable_cross_origin_prerender_iframes) {
  if (!registry_)
    return;

  // Extract only the candidates which apply to prerender, and sort them by URL
  // so we can efficiently compare them to `started_prerenders_`.
  // TODO(https://crbug.com/428500219): Add warning message if prerender and
  // prerender-until-script are applied to the same URL.
  std::vector<std::pair<size_t, blink::mojom::SpeculationCandidatePtr>>
      prerender_candidates;
  for (const auto& candidate : candidates) {
    if (candidate->action == blink::mojom::SpeculationAction::kPrerender ||
        candidate->action ==
            blink::mojom::SpeculationAction::kPrerenderUntilScript) {
      prerender_candidates.emplace_back(prerender_candidates.size(),
                                        candidate.Clone());
    }
  }
  enable_cross_origin_prerender_iframes_ |=
      enable_cross_origin_prerender_iframes;

  std::ranges::stable_sort(
      prerender_candidates, std::less<>(),
      [](const auto& p) { return PrerenderInfo(p.second); });
  std::vector<std::pair<size_t, blink::mojom::SpeculationCandidatePtr>>
      candidates_to_start;

  // Collects the host ids corresponding to the URLs that are removed from the
  // speculation rules. These hosts are cancelled later.
  std::vector<FrameTreeNodeId> removed_prerender_rules;

  // Compare the sorted candidate and started prerender lists to one another.
  // Since they are sorted, we process the lexicographically earlier of the two
  // PrerenderInfos pointed at by the iterators, and compare the range of
  // entries in each that match that PrerenderInfo.
  //
  // PrerenderInfos which are present in the prerender list but not the
  // candidate list can no longer proceed and are cancelled.
  //
  // PrerenderInfos which are present in the candidate list but not the
  // prerender list could be started and are gathered in `candidates_to_start`.
  auto candidate_it = prerender_candidates.begin();
  auto started_it = started_prerenders_.begin();
  while (candidate_it != prerender_candidates.end() ||
         started_it != started_prerenders_.end()) {
    // Select the lesser of the two PrerenderInfos to diff.
    PrerenderInfo prerender_info;
    if (started_it == started_prerenders_.end()) {
      prerender_info = PrerenderInfo(candidate_it->second);
    } else if (candidate_it == prerender_candidates.end()) {
      prerender_info = *started_it;
    } else {
      prerender_info =
          std::min(PrerenderInfo(candidate_it->second), *started_it);
    }

    // Select the ranges from both that match the PrerenderInfo in question.
    auto equal_prerender_end = std::ranges::find_if(
        started_it, started_prerenders_.end(),
        [&](const auto& started) { return started != prerender_info; });
    base::span<PrerenderInfo> UNSAFE_TODO(
        matching_prerenders(started_it, equal_prerender_end));
    auto equal_candidate_end = std::ranges::find_if(
        candidate_it, prerender_candidates.end(), [&](const auto& candidate) {
          return PrerenderInfo(candidate.second) != prerender_info;
        });
    base::span<std::pair<size_t, blink::mojom::SpeculationCandidatePtr>>
        UNSAFE_TODO(matching_candidates(candidate_it, equal_candidate_end));

    // Decide what started prerenders to cancel.
    for (PrerenderInfo& prerender : matching_prerenders) {
      if (prerender.prerender_host_id.is_null()) {
        continue;
      }
      // TODO(jbroman): This doesn't currently care about other aspects, like
      // the referrer. This doesn't presently matter, but in the future we might
      // want to cancel if there are candidates which match by PrerenderInfo but
      // none of which permit this prerender.
      if (matching_candidates.empty()) {
        removed_prerender_rules.push_back(prerender.prerender_host_id);
      }
    }

    // Decide what new candidates to start.
    // For now, start one candidate per target hint for a URL only if there are
    // no matching prerenders. We could be cleverer in the future.
    if (matching_prerenders.empty()) {
      CHECK(!matching_candidates.empty());

      std::set<PrerenderInfo> processed_prerender_info;

      for (auto& matching_candidate : matching_candidates) {
        PrerenderInfo matching_candidate_prerender_info =
            PrerenderInfo(matching_candidate.second);
        if (processed_prerender_info
                .insert(std::move(matching_candidate_prerender_info))
                .second) {
          candidates_to_start.push_back(std::move(matching_candidate));
        }
      }
    }

    // Advance the iterators past all matching entries.
    candidate_it = equal_candidate_end;
    started_it = equal_prerender_end;
  }

  std::vector<std::pair<GURL, PreloadingType>> to_be_cancelled_prerender_list;
  for (auto ftn_id : removed_prerender_rules) {
    if (PrerenderHost* prerender_host =
            registry_->FindNonReservedHostById(ftn_id)) {
      to_be_cancelled_prerender_list.emplace_back(
          prerender_host->GetInitialUrl(),
          ConvertSpeculationActionToPreloadingType(
              prerender_host->speculation_action()));
    }
  }
  std::set<FrameTreeNodeId> canceled_prerender_rules_set =
      registry_->CancelHosts(
          removed_prerender_rules,
          PrerenderCancellationReason(
              PrerenderFinalStatus::kSpeculationRuleRemoved));
  if (base::FeatureList::IsEnabled(
          features::kPrerender2FallbackPrefetchSpecRules)) {
    WebContents* web_contents =
        WebContents::FromRenderFrameHost(&render_frame_host_.get());
    auto* prefetch_document_manager =
        content::PrefetchDocumentManager::GetOrCreateForCurrentDocument(
            web_contents->GetPrimaryMainFrame());
    for (const auto& [url, preloading_type] : to_be_cancelled_prerender_list) {
      prefetch_document_manager->ResetPrefetchAheadOfPrerenderIfExist(
          preloading_type, url);
    }
  }

  // Canceled prerenders by kSpeculationRuleRemoved should have already been
  // removed from `started_prerenders_` via `OnCancel`.
  CHECK(std::find_if(started_prerenders_.begin(), started_prerenders_.end(),
                     [&](const PrerenderInfo& x) {
                       return base::Contains(canceled_prerender_rules_set,
                                             x.prerender_host_id);
                     }) == started_prerenders_.end());

  // Actually start the candidates in their original order once the diffing is
  // done.
  std::ranges::sort(candidates_to_start, std::less<>(),
                    [](const auto& p) { return p.first; });
  for (const auto& [_, candidate] : candidates_to_start) {
    PreloadingTriggerType trigger_type =
        PreloadingTriggerTypeFromSpeculationInjectionType(
            candidate->injection_type);
    // Immediate candidates are enacted by the same predictor that creates them.
    PreloadingPredictor enacting_predictor =
        GetPredictorForPreloadingTriggerType(trigger_type);
    MaybePrerender(candidate, enacting_predictor, PreloadingConfidence{100});
  }
}

void PrerendererImpl::OnLCPPredicted() {
  blocked_ = false;
  for (auto& [candidate, enacting_predictor, confidence] :
       std::move(blocked_candidates_)) {
    MaybePrerender(candidate, enacting_predictor, confidence);
  }
}

bool PrerendererImpl::MaybePrerender(
    const blink::mojom::SpeculationCandidatePtr& candidate,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence) {
  // Check actions. Only Prerender and PrerenderUntilScript are allowed.
  switch (candidate->action) {
    case blink::mojom::SpeculationAction::kPrerender:
    case blink::mojom::SpeculationAction::kPrerenderUntilScript:
      break;
    default:
      NOTREACHED();
  }

  // Prerendering is not allowed in fenced frames.
  if (render_frame_host_->IsNestedWithinFencedFrame()) {
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "The SpeculationRules API does not support prerendering in fenced "
        "frames.");
    return false;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host_.get());
  static_cast<PreloadingDataImpl*>(
      PreloadingData::GetOrCreateForWebContents(web_contents))
      ->SetHasSpeculationRulesPrerender();
  if (blocked_) {
    blocked_candidates_.emplace_back(candidate->Clone(), enacting_predictor,
                                     confidence);
    return false;
  }

  // Prerendering frames should not trigger any prerender request.
  CHECK(!render_frame_host_->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));

  if (!registry_)
    return false;

  auto& rfhi = static_cast<RenderFrameHostImpl&>(render_frame_host_.get());

  // `prerender_host_id` is not available yet.
  PrerenderInfo prerender_info(candidate);

  auto [begin, end] = std::ranges::equal_range(
      started_prerenders_.begin(), started_prerenders_.end(), prerender_info,
      PrerenderInfo::PrerenderInfoComparator);
  // cannot currently start a second prerender with the same URL and target_hint
  if (begin != end) {
    return false;
  }

  switch (candidate->action) {
    case blink::mojom::SpeculationAction::kPrerender:
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          &rfhi, blink::mojom::WebFeature::kSpeculationRulesPrerender);
      break;
    case blink::mojom::SpeculationAction::kPrerenderUntilScript:
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          &rfhi,
          blink::mojom::WebFeature::kSpeculationRulesPrerenderUntilScript);
      break;
    default:
      NOTREACHED();
  }

  IncrementReceivedPrerendersCountForMetrics(
      PreloadingTriggerTypeFromSpeculationInjectionType(
          candidate->injection_type),
      candidate->eagerness);

  // TODO(crbug.com/40168192): Remove it after supporting cross-site
  // prerender.
  if (!prerender_navigation_utils::IsSameSite(candidate->url,
                                              rfhi.GetLastCommittedOrigin())) {
    rfhi.AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "The SpeculationRules API does not support cross-site prerender "
            "yet (initiator origin: %s, prerender origin: %s). "
            "https://crbug.com/1176054 tracks cross-site support.",
            rfhi.GetLastCommittedOrigin().Serialize().c_str(),
            url::Origin::Create(candidate->url).Serialize().c_str()));
  }

  std::optional<net::HttpNoVarySearchData> no_vary_search_hint;
  if (candidate->no_vary_search_hint) {
    no_vary_search_hint = no_vary_search::ParseHttpNoVarySearchDataFromMojom(
        candidate->no_vary_search_hint);
  }

  const bool should_warm_up_compositor = base::FeatureList::IsEnabled(
      IsImmediateSpeculationEagerness(candidate->eagerness)
          ? features::kPrerender2WarmUpCompositorForImmediate
          : features::kPrerender2WarmUpCompositorForNonImmediate);

  PrerenderAttributes attributes(
      candidate->url,
      PreloadingTriggerTypeFromSpeculationInjectionType(
          candidate->injection_type),
      /*embedder_histogram_suffix=*/"",
      SpeculationRulesParams(candidate->target_browsing_context_name_hint,
                             candidate->eagerness,
                             SpeculationRulesTags(candidate->tags)),
      Referrer{*candidate->referrer}, no_vary_search_hint, &rfhi,
      web_contents->GetWeakPtr(), ui::PAGE_TRANSITION_LINK,
      should_warm_up_compositor,
      /*should_prepare_paint_tree=*/false, candidate->action,
      /*url_match_predicate=*/{},
      /*prerender_navigation_handle_callback=*/{},
      PreloadPipelineInfoImpl::Create(
          /*planned_max_preloading_type=*/
          ConvertSpeculationActionToPreloadingType(candidate->action)),
      /*allow_reuse=*/false, candidate->form_submission);
  attributes.enable_cross_origin_prerender_iframes =
      enable_cross_origin_prerender_iframes_;

  PreloadingTriggerType trigger_type =
      PreloadingTriggerTypeFromSpeculationInjectionType(
          candidate->injection_type);
  PreloadingPredictor creating_predictor =
      GetPredictorForPreloadingTriggerType(trigger_type);
  prerender_info.prerender_host_id = [&] {
    // TODO(crbug.com/40235424): Handle the case where multiple speculation
    // rules have the same URL but its `target_browsing_context_name_hint` is
    // different. In the current implementation, only the first rule is
    // triggered.
    switch (candidate->target_browsing_context_name_hint) {
      case blink::mojom::SpeculationTargetHint::kBlank: {
        GetContentClient()->browser()->LogWebFeatureForCurrentPage(
            &rfhi, blink::mojom::WebFeature::kSpeculationRulesTargetHintBlank);
        // For the prerender-in-new-tab, PreloadingAttempt will be managed by a
        // prerender WebContents to be created later.
        return registry_->CreateAndStartHostForNewTab(
            attributes, creating_predictor, enacting_predictor, confidence);
      }
      case blink::mojom::SpeculationTargetHint::kNoHint:
      case blink::mojom::SpeculationTargetHint::kSelf: {
        if (base::FeatureList::IsEnabled(
                features::kPrerender2FallbackPrefetchSpecRules)) {
          auto* prefetch_document_manager =
              content::PrefetchDocumentManager::GetOrCreateForCurrentDocument(
                  web_contents->GetPrimaryMainFrame());
          prefetch_document_manager->PrefetchAheadOfPrerender(
              attributes.preload_pipeline_info, candidate.Clone(),
              enacting_predictor);
        }

        // Create new PreloadingAttempt and pass all the values corresponding to
        // this prerendering attempt.
        auto* preloading_data =
            PreloadingDataImpl::GetOrCreateForWebContents(web_contents);
        PreloadingURLMatchCallback same_url_matcher =
            PreloadingData::GetSameURLMatcher(candidate->url);

        auto* preloading_attempt = static_cast<PreloadingAttemptImpl*>(
            preloading_data->AddPreloadingAttempt(
                creating_predictor, enacting_predictor,
                ConvertSpeculationActionToPreloadingType(candidate->action),
                std::move(same_url_matcher),
                web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()));
        preloading_attempt->SetSpeculationEagerness(candidate->eagerness);
        return registry_->CreateAndStartHost(attributes, preloading_attempt);
      }
    }
  }();

  // An existing prerender may be canceled to start a new prerender, and
  // `started_prerenders_` may be modified through this cancellation. Therefore,
  // it is needed to re-calculate the right place here on `started_prerenders_`
  // for new candidates.
  end = std::ranges::upper_bound(started_prerenders_.begin(),
                                 started_prerenders_.end(), prerender_info,
                                 PrerenderInfo::PrerenderInfoComparator);

  started_prerenders_.insert(end, std::move(prerender_info));

  return true;
}

bool PrerendererImpl::ShouldWaitForPrerenderResult(const GURL& url) {
  // This function is used to check whetehr a prerender is started to avoid
  // starting prefetch in OnPointerDown, OnPointerHover or other heuristic
  // methods which don't take target_hint into consideration. So unlike other
  // functions in this file, this part uses `url` only instead of
  // `PrerenderInfo` which consists of target_hint information.
  auto [begin, end] = std::ranges::equal_range(
      started_prerenders_.begin(), started_prerenders_.end(), url,
      std::less<>(), &PrerenderInfo::url);
  for (auto it = begin; it != end; ++it) {
    if (it->prerender_host_id.is_null()) {
      return false;
    }
  }
  return begin != end;
}

void PrerendererImpl::OnCancel(FrameTreeNodeId host_frame_tree_node_id,
                               const PrerenderCancellationReason& reason) {
  switch (reason.final_status()) {
    // TODO(crbug.com/40275452): Support other final status cases.
    case PrerenderFinalStatus::kTimeoutBackgrounded:
    case PrerenderFinalStatus::kMaxNumOfRunningNonImmediatePrerendersExceeded:
    case PrerenderFinalStatus::kSpeculationRuleRemoved: {
      auto erasing_prerender_it = std::find_if(
          started_prerenders_.begin(), started_prerenders_.end(),
          [&](const PrerenderInfo& prerender_info) {
            return prerender_info.prerender_host_id == host_frame_tree_node_id;
          });

      if (erasing_prerender_it != started_prerenders_.end()) {
        auto url = erasing_prerender_it->url;
        blink::mojom::SpeculationAction action = erasing_prerender_it->action;
        started_prerenders_.erase(erasing_prerender_it);

        // Notify PreloadingDecider.
        prerender_cancellation_callback_.Run(url, action);
      }
      break;
    }
    default:
      break;
  }
}

void PrerendererImpl::OnRegistryDestroyed() {
  observation_.Reset();
}

void PrerendererImpl::SetPrerenderCancellationCallback(
    PrerenderCancellationCallback callback) {
  prerender_cancellation_callback_ = std::move(callback);
}

void PrerendererImpl::CancelStartedPrerenders() {
  if (registry_) {
    std::vector<FrameTreeNodeId> started_prerender_ids;
    for (auto& prerender_info : started_prerenders_) {
      started_prerender_ids.push_back(prerender_info.prerender_host_id);
    }
    registry_->CancelHosts(
        started_prerender_ids,
        PrerenderCancellationReason(PrerenderFinalStatus::kTriggerDestroyed));
  }

  started_prerenders_.clear();
}

void PrerendererImpl::CancelStartedPrerendersForTesting() {
  CancelStartedPrerenders();
}

void PrerendererImpl::ResetReceivedPrerendersCountForMetrics() {
  for (auto trigger_type :
       {PreloadingTriggerType::kSpeculationRule,
        PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld}) {
    received_prerenders_by_eagerness_[trigger_type].fill({});
  }
}

void PrerendererImpl::IncrementReceivedPrerendersCountForMetrics(
    PreloadingTriggerType trigger_type,
    blink::mojom::SpeculationEagerness eagerness) {
  received_prerenders_by_eagerness_[trigger_type]
                                   [static_cast<size_t>(eagerness)]++;
}

void PrerendererImpl::RecordReceivedPrerendersCountToMetrics() {
  for (auto trigger_type :
       {PreloadingTriggerType::kSpeculationRule,
        PreloadingTriggerType::kSpeculationRuleFromIsolatedWorld}) {
    int conservative =
        received_prerenders_by_eagerness_[trigger_type][static_cast<size_t>(
            blink::mojom::SpeculationEagerness::kConservative)];
    int moderate =
        received_prerenders_by_eagerness_[trigger_type][static_cast<size_t>(
            blink::mojom::SpeculationEagerness::kModerate)];
    int eager =
        received_prerenders_by_eagerness_[trigger_type][static_cast<size_t>(
            blink::mojom::SpeculationEagerness::kEager)];
    int immediate =
        received_prerenders_by_eagerness_[trigger_type][static_cast<size_t>(
            blink::mojom::SpeculationEagerness::kImmediate)];

    // This will record zero when
    //  1) there are no started prerenders eventually. Also noted that if
    //     there is no rule set, PreloadingDecider won't be created (which means
    //     PrerenderImpl also won't be created), so it cannot be reached this
    //     code path at the first place.
    //  2) when the corresponding RFH lives but is inactive (such as the case in
    //     BFCache) after once PrimaryPageChanged was called and the recorded
    //     number was reset (As long as PreloadingDecider (which has the same
    //     lifetime with a document) that owns this (PrerenderImpl) lives, this
    //     function will be called per PrimaryPageChanged).
    //
    // Avoids recording these cases uniformly.
    if (conservative + moderate + eager + immediate == 0) {
      continue;
    }

    // Record per single eagerness.
    RecordReceivedPrerendersPerPrimaryPageChangedCount(
        conservative, trigger_type, "Conservative");
    RecordReceivedPrerendersPerPrimaryPageChangedCount(moderate, trigger_type,
                                                       "Moderate");
    RecordReceivedPrerendersPerPrimaryPageChangedCount(eager, trigger_type,
                                                       "Eager2");
    RecordReceivedPrerendersPerPrimaryPageChangedCount(immediate, trigger_type,
                                                       "Immediate2");
  }
}

}  // namespace content
