// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/prerenderer_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom.h"

namespace content {

namespace {

using EagernessSet =
    base::EnumSet<blink::mojom::SpeculationEagerness,
                  blink::mojom::SpeculationEagerness::kMinValue,
                  blink::mojom::SpeculationEagerness::kMaxValue>;

EagernessSet EagernessSetFromFeatureParam(std::string_view value) {
  EagernessSet set;
  for (std::string_view piece : base::SplitStringPiece(
           value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (piece == "conservative") {
      set.Put(blink::mojom::SpeculationEagerness::kConservative);
    } else if (piece == "moderate") {
      set.Put(blink::mojom::SpeculationEagerness::kModerate);
    }
  }
  return set;
}

void OnPrefetchDestroyed(WeakDocumentPtr document, const GURL& url) {
  PreloadingDecider* preloading_decider =
      PreloadingDecider::GetForCurrentDocument(
          document.AsRenderFrameHostIfValid());
  if (preloading_decider) {
    preloading_decider->OnPreloadDiscarded(
        {url, blink::mojom::SpeculationAction::kPrefetch});
  }
}

void OnPrerenderCanceled(WeakDocumentPtr document, const GURL& url) {
  PreloadingDecider* preloading_decider =
      PreloadingDecider::GetForCurrentDocument(
          document.AsRenderFrameHostIfValid());
  if (preloading_decider) {
    preloading_decider->OnPreloadDiscarded(
        {url, blink::mojom::SpeculationAction::kPrerender});
  }
}

bool PredictionOccursInOtherWebContents(
    const blink::mojom::SpeculationCandidate& candidate) {
  return base::FeatureList::IsEnabled(blink::features::kPrerender2InNewTab) &&
         candidate.action == blink::mojom::SpeculationAction::kPrerender &&
         candidate.target_browsing_context_name_hint ==
             blink::mojom::SpeculationTargetHint::kBlank;
}

}  // namespace

class PreloadingDecider::BehaviorConfig {
 public:
  BehaviorConfig()
      : ml_model_eagerness_{blink::mojom::SpeculationEagerness::kModerate},
        ml_model_enacts_candidates_(
            blink::features::kPreloadingModelEnactCandidates.Get()),
        ml_model_prefetch_moderate_threshold_{std::clamp(
            blink::features::kPreloadingModelPrefetchModerateThreshold.Get(),
            0,
            100)},
        ml_model_prerender_moderate_threshold_{std::clamp(
            blink::features::kPreloadingModelPrerenderModerateThreshold.Get(),
            0,
            100)} {
    static const base::FeatureParam<std::string> kPointerDownEagerness{
        &blink::features::kSpeculationRulesPointerDownHeuristics,
        "pointer_down_eagerness", "conservative,moderate"};
    pointer_down_eagerness_ =
        EagernessSetFromFeatureParam(kPointerDownEagerness.Get());

    static const base::FeatureParam<std::string> kPointerHoverEagerness{
        &blink::features::kSpeculationRulesPointerHoverHeuristics,
        "pointer_hover_eagerness", "moderate"};
    pointer_hover_eagerness_ =
        EagernessSetFromFeatureParam(kPointerHoverEagerness.Get());
  }

  EagernessSet EagernessSetForPredictor(
      const PreloadingPredictor& predictor) const {
    if (predictor == preloading_predictor::kUrlPointerDownOnAnchor) {
      return pointer_down_eagerness_;
    } else if (predictor == preloading_predictor::kUrlPointerHoverOnAnchor) {
      return pointer_hover_eagerness_;
    } else if (predictor ==
               preloading_predictor::kPreloadingHeuristicsMLModel) {
      return ml_model_eagerness_;
    } else {
      NOTREACHED() << "unexpected predictor " << predictor.name() << "/"
                   << predictor.ukm_value();
    }
  }

  PreloadingConfidence GetThreshold(
      const PreloadingPredictor& predictor,
      blink::mojom::SpeculationAction action) const {
    if (predictor == preloading_predictor::kUrlPointerDownOnAnchor) {
      return kNoThreshold;
    } else if (predictor == preloading_predictor::kUrlPointerHoverOnAnchor) {
      return kNoThreshold;
    } else if (predictor ==
               preloading_predictor::kPreloadingHeuristicsMLModel) {
      switch (action) {
        case blink::mojom::SpeculationAction::kPrefetch:
        case blink::mojom::SpeculationAction::kPrefetchWithSubresources:
          return ml_model_prefetch_moderate_threshold_;
        case blink::mojom::SpeculationAction::kPrerender:
          return ml_model_prerender_moderate_threshold_;
      }
    } else {
      NOTREACHED() << "unexpected predictor " << predictor.name() << "/"
                   << predictor.ukm_value();
    }
  }

  bool ml_model_enacts_candidates() const {
    return ml_model_enacts_candidates_;
  }

 private:
  // Any confidence value is >= kNoThreshold, so the associated action will
  // happen regardless of the confidence value.
  static constexpr PreloadingConfidence kNoThreshold{0};

  EagernessSet pointer_down_eagerness_;
  EagernessSet pointer_hover_eagerness_;
  const EagernessSet ml_model_eagerness_;
  const bool ml_model_enacts_candidates_ = false;
  const PreloadingConfidence ml_model_prefetch_moderate_threshold_{
      kNoThreshold};
  const PreloadingConfidence ml_model_prerender_moderate_threshold_{
      kNoThreshold};
};

DOCUMENT_USER_DATA_KEY_IMPL(PreloadingDecider);

PreloadingDecider::PreloadingDecider(RenderFrameHost* rfh)
    : DocumentUserData<PreloadingDecider>(rfh),
      behavior_config_(std::make_unique<BehaviorConfig>()),
      observer_for_testing_(nullptr),
      preconnector_(render_frame_host()),
      prefetcher_(render_frame_host()),
      prerenderer_(std::make_unique<PrerendererImpl>(render_frame_host())) {
  PrefetchDocumentManager::GetOrCreateForCurrentDocument(rfh)
      ->SetPrefetchDestructionCallback(
          base::BindRepeating(&OnPrefetchDestroyed, rfh->GetWeakDocumentPtr()));

  prerenderer_->SetPrerenderCancellationCallback(
      base::BindRepeating(&OnPrerenderCanceled, rfh->GetWeakDocumentPtr()));
}

PreloadingDecider::~PreloadingDecider() = default;

void PreloadingDecider::AddPreloadingPrediction(
    const GURL& url,
    PreloadingPredictor predictor,
    PreloadingConfidence confidence) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  auto* preloading_data =
      PreloadingDataImpl::GetOrCreateForWebContents(web_contents);
  ukm::SourceId triggered_primary_page_source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  preloading_data->AddPreloadingPrediction(
      predictor, confidence, PreloadingData::GetSameURLMatcher(url),
      triggered_primary_page_source_id);
}

void PreloadingDecider::OnPointerDown(const GURL& url) {
  if (observer_for_testing_) {
    observer_for_testing_->OnPointerDown(url);
  }
  MaybeEnactCandidate(url, preloading_predictor::kUrlPointerDownOnAnchor,
                      PreloadingConfidence{100},
                      /*fallback_to_preconnect=*/true);
}

void PreloadingDecider::OnPreloadingHeuristicsModelDone(const GURL& url,
                                                        float score) {
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kPreloadingHeuristicsMLModel));
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  auto* preloading_data = static_cast<PreloadingDataImpl*>(
      PreloadingData::GetOrCreateForWebContents(web_contents));
  preloading_data->AddExperimentalPreloadingPrediction(
      /*name=*/"OnPreloadingHeuristicsMLModel",
      /*url_match_predicate=*/PreloadingData::GetSameURLMatcher(url),
      /*score=*/score,
      /*min_score=*/0.0,
      /*max_score=*/1.0,
      /*buckets=*/100);

  if (!behavior_config_->ml_model_enacts_candidates()) {
    return;
  }

  ml_model_available_ = true;

  const PreloadingConfidence confidence{std::clamp(
      base::saturated_cast<int>(std::nearbyint(score * 100.f)), 0, 100)};

  MaybeEnactCandidate(url, preloading_predictor::kPreloadingHeuristicsMLModel,
                      confidence, /*fallback_to_preconnect=*/false);
}

void PreloadingDecider::OnPointerHover(
    const GURL& url,
    blink::mojom::AnchorElementPointerDataPtr mouse_data) {
  if (observer_for_testing_) {
    observer_for_testing_->OnPointerHover(url);
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  auto* preloading_data = static_cast<PreloadingDataImpl*>(
      PreloadingData::GetOrCreateForWebContents(web_contents));
  preloading_data->AddExperimentalPreloadingPrediction(
      /*name=*/"OnPointerHoverWithMotionEstimator",
      /*url_match_predicate=*/PreloadingData::GetSameURLMatcher(url),
      /*score=*/std::clamp(mouse_data->mouse_velocity, 0.0, 500.0),
      /*min_score=*/0,
      /*max_score=*/500,
      /*buckets=*/100);

  // Preconnecting on hover events should not be done if the link is not safe
  // to prefetch or prerender.
  constexpr bool fallback_to_preconnect = false;
  MaybeEnactCandidate(url, preloading_predictor::kUrlPointerHoverOnAnchor,
                      PreloadingConfidence{100}, fallback_to_preconnect);
}

void PreloadingDecider::MaybeEnactCandidate(
    const GURL& url,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence,
    bool fallback_to_preconnect) {
  if (const auto [found, added_prediction] =
          MaybePrerender(url, enacting_predictor, confidence);
      found) {
    // If the prediction is associated with another WebContents, don't duplicate
    // it here.
    if (!added_prediction) {
      AddPreloadingPrediction(url, enacting_predictor, confidence);
    }
    return;
  }

  AddPreloadingPrediction(url, enacting_predictor, confidence);

  if (ShouldWaitForPrerenderResult(url)) {
    // If there is a prerender in progress already, don't attempt a prefetch.
    return;
  }

  if (MaybePrefetch(url, enacting_predictor, confidence)) {
    return;
  }
  // Ideally it is preferred to fallback to preconnect asynchronously if a
  // prefetch attempt fails. We should revisit it later perhaps after having
  // data showing it is worth doing so.
  if (!fallback_to_preconnect || ShouldWaitForPrefetchResult(url)) {
    return;
  }
  preconnector_.MaybePreconnect(url);
}

void PreloadingDecider::AddStandbyCandidate(
    const blink::mojom::SpeculationCandidatePtr& candidate) {
  SpeculationCandidateKey key{candidate->url, candidate->action};
  on_standby_candidates_[key].push_back(candidate.Clone());

  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  if (candidate->no_vary_search_hint) {
    SpeculationCandidateKey key_no_vary_search{
        candidate->url.ReplaceComponents(replacements), candidate->action};
    no_vary_search_hint_on_standby_candidates_[key_no_vary_search].insert(key);
  }
}

void PreloadingDecider::RemoveStandbyCandidate(
    const SpeculationCandidateKey key) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  SpeculationCandidateKey key_no_vary_search{
      key.first.ReplaceComponents(replacements), key.second};
  auto it = no_vary_search_hint_on_standby_candidates_.find(key_no_vary_search);
  if (it != no_vary_search_hint_on_standby_candidates_.end()) {
    it->second.erase(key);
    if (it->second.empty()) {
      no_vary_search_hint_on_standby_candidates_.erase(it);
    }
  }
  on_standby_candidates_.erase(key);
}

void PreloadingDecider::ClearStandbyCandidates() {
  no_vary_search_hint_on_standby_candidates_.clear();
  on_standby_candidates_.clear();
}

void PreloadingDecider::UpdateSpeculationCandidates(
    std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (observer_for_testing_) {
    observer_for_testing_->UpdateSpeculationCandidates(candidates);
  }
  devtools_instrumentation::DidUpdateSpeculationCandidates(render_frame_host(),
                                                           candidates);

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  auto* preloading_data = static_cast<PreloadingDataImpl*>(
      PreloadingData::GetOrCreateForWebContents(web_contents));
  preloading_data->SetIsNavigationInDomainCallback(
      content_preloading_predictor::kSpeculationRules,
      base::BindRepeating([](NavigationHandle* navigation_handle) -> bool {
        return ui::PageTransitionIsWebTriggerable(
            navigation_handle->GetPageTransition());
      }));
  PredictorDomainCallback is_new_link_nav =
      base::BindRepeating([](NavigationHandle* navigation_handle) -> bool {
        auto page_transition = navigation_handle->GetPageTransition();
        return ui::PageTransitionCoreTypeIs(
                   page_transition, ui::PageTransition::PAGE_TRANSITION_LINK) &&
               (page_transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) == 0 &&
               ui::PageTransitionIsNewNavigation(page_transition);
      });
  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kUrlPointerDownOnAnchor, is_new_link_nav);
  preloading_data->SetIsNavigationInDomainCallback(
      preloading_predictor::kUrlPointerHoverOnAnchor, is_new_link_nav);
  if (base::FeatureList::IsEnabled(
          blink::features::kPreloadingHeuristicsMLModel) &&
      behavior_config_->ml_model_enacts_candidates()) {
    preloading_data->SetIsNavigationInDomainCallback(
        preloading_predictor::kPreloadingHeuristicsMLModel, is_new_link_nav);
  }

  // Here we look for all preloading candidates that are safe to perform, but
  // their eagerness level is not high enough to perform without the trigger
  // form link selection heuristics logic. We then remove them from the
  // |candidates| list to prevent them from being initiated and will add them
  // to |on_standby_candidates_| to be later considered by the heuristics logic.
  auto should_mark_as_on_standby = [&](const auto& candidate) {
    SpeculationCandidateKey key{candidate->url, candidate->action};
    if (candidate->eagerness != blink::mojom::SpeculationEagerness::kEager &&
        processed_candidates_.find(key) == processed_candidates_.end()) {
      // A PreloadingPrediction is intentionally not created for these
      // candidates. Non-eager rules aren't predictions per se, but a
      // declaration to the browser that preloading would be safe.
      AddStandbyCandidate(candidate);
      // TODO(isaboori) In current implementation, after calling prefetcher
      // ProcessCandidatesForPrefetch, the prefetch_service starts checking the
      // eligibility of the candidates and it will add any eligible candidates
      // to the prefetch_queue_starts and starts prefetching them as soon as
      // possible. For that reason here we remove on-standby candidates from the
      // list. The prefetch service should be updated to let us pass the
      // on-standby candidates to prefetch_service from here to let it check
      // their eligibility right away without starting to prefetch them. It
      // should also be possible to trigger the start of the prefetch based on
      // heuristics.
      return true;
    }

    processed_candidates_[key].push_back(candidate.Clone());

    // TODO(crbug.com/40230530): Pass the action requested by speculation rules
    // to PreloadingPrediction.
    // A new web contents will be created for the case of prerendering into a
    // new tab, so recording PreloadingPrediction is delayed until
    // PrerenderNewTabHandle::StartPrerendering.
    bool add_preloading_prediction =
        !PredictionOccursInOtherWebContents(*candidate);

    if (add_preloading_prediction) {
      PreloadingTriggerType trigger_type =
          PreloadingTriggerTypeFromSpeculationInjectionType(
              candidate->injection_type);
      // Eager candidates are enacted by the same predictor that creates them.
      PreloadingPredictor enacting_predictor =
          GetPredictorForPreloadingTriggerType(trigger_type);
      AddPreloadingPrediction(candidate->url, std::move(enacting_predictor),
                              PreloadingConfidence{100});
    }

    return false;
  };

  ClearStandbyCandidates();

  // The lists of SpeculationCandidates cached in |processed_candidates_| will
  // be stale now, so we clear the lists now and repopulate them below.
  for (auto& entry : processed_candidates_) {
    entry.second.clear();
  }

  // Move eager candidates to the front. This will avoid unnecessarily
  // marking some non-eager candidates as on-standby when there is an eager
  // candidate with the same URL that will be processed immediately.
  base::ranges::stable_partition(candidates, [&](const auto& candidate) {
    return candidate->eagerness == blink::mojom::SpeculationEagerness::kEager;
  });

  // The candidates remaining after this call will be all eager candidates,
  // and all non-eager candidates whose (url, action) pair has already been
  // processed.
  std::erase_if(candidates, should_mark_as_on_standby);

  prefetcher_.ProcessCandidatesForPrefetch(candidates);

  prerenderer_->ProcessCandidatesForPrerender(candidates);
}

void PreloadingDecider::OnLCPPredicted() {
  prerenderer_->OnLCPPredicted();
}

bool PreloadingDecider::MaybePrefetch(
    const GURL& url,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence) {
  SpeculationCandidateKey key{url, blink::mojom::SpeculationAction::kPrefetch};
  std::optional<std::pair<PreloadingDecider::SpeculationCandidateKey,
                          blink::mojom::SpeculationCandidatePtr>>
      matched_candidate_pair =
          GetMatchedPreloadingCandidate(key, enacting_predictor, confidence);
  if (!matched_candidate_pair.has_value()) {
    return false;
  }

  key = matched_candidate_pair.value().first;
  bool result = prefetcher_.MaybePrefetch(
      std::move(matched_candidate_pair.value().second), enacting_predictor);

  auto it = on_standby_candidates_.find(key);
  CHECK(it != on_standby_candidates_.end());
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates_for_key =
      std::move(it->second);
  RemoveStandbyCandidate(key);
  processed_candidates_[std::move(key)] = std::move(candidates_for_key);
  return result;
}

std::optional<std::pair<PreloadingDecider::SpeculationCandidateKey,
                        blink::mojom::SpeculationCandidatePtr>>
PreloadingDecider::GetMatchedPreloadingCandidate(
    const PreloadingDecider::SpeculationCandidateKey& lookup_key,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence) const {
  blink::mojom::SpeculationCandidatePtr candidate;

  auto it = on_standby_candidates_.find(lookup_key);
  if (it != on_standby_candidates_.end()) {
    auto inner_it =
        base::ranges::find_if(it->second, [&](const auto& candidate) {
          return IsSuitableCandidate(candidate, enacting_predictor, confidence,
                                     lookup_key.second);
        });
    if (inner_it != it->second.end()) {
      candidate = inner_it->Clone();
    }
  }

  if (candidate) {
    return std::make_pair(lookup_key, std::move(candidate));
  }

  auto matched_candidate_pair = GetMatchedPreloadingCandidateByNoVarySearchHint(
      lookup_key, enacting_predictor, confidence);
  if (!matched_candidate_pair.has_value()) {
    return std::nullopt;
  }

  return std::move(matched_candidate_pair.value());
}

std::optional<std::pair<PreloadingDecider::SpeculationCandidateKey,
                        blink::mojom::SpeculationCandidatePtr>>
PreloadingDecider::GetMatchedPreloadingCandidateByNoVarySearchHint(
    const PreloadingDecider::SpeculationCandidateKey& lookup_key,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence) const {
  blink::mojom::SpeculationCandidatePtr candidate;
  SpeculationCandidateKey key;

  // Check all URLs that might match via NVS hint.
  // If there are multiple candidates that match the first one.
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  const GURL url_without_query_and_ref =
      lookup_key.first.ReplaceComponents(replacements);
  auto nvs_it = no_vary_search_hint_on_standby_candidates_.find(
      {url_without_query_and_ref, lookup_key.second});
  if (nvs_it == no_vary_search_hint_on_standby_candidates_.end()) {
    return std::nullopt;
  }
  for (const auto& standby_key : nvs_it->second) {
    CHECK_EQ(standby_key.second, lookup_key.second);
    const GURL& preload_url = standby_key.first;
    // Every preload in this set might come back with NVS header of
    // "params" and match. But we will consider only the first preload that
    // has a No-Vary-Search hint that is matching.
    auto standby_it = on_standby_candidates_.find(standby_key);
    CHECK(standby_it != on_standby_candidates_.end());
    auto inner_it = base::ranges::find_if(
        standby_it->second, [&](const auto& on_standby_candidate) {
          return on_standby_candidate->no_vary_search_hint &&
                 no_vary_search::ParseHttpNoVarySearchDataFromMojom(
                     on_standby_candidate->no_vary_search_hint)
                     .AreEquivalent(lookup_key.first, preload_url) &&
                 IsSuitableCandidate(on_standby_candidate, enacting_predictor,
                                     confidence, standby_key.second);
        });
    if (inner_it != standby_it->second.end()) {
      candidate = inner_it->Clone();
      key = standby_key;
      break;
    }
  }

  if (!candidate) {
    return std::nullopt;
  }

  return std::make_pair(key, std::move(candidate));
}

bool PreloadingDecider::ShouldWaitForPrefetchResult(const GURL& url) {
  // TODO(liviutinta): Don't implement any No-Vary-Search hint matching here
  // for now. It is not clear how to match `url` with a `processed_candidate`.
  // Also, for a No-Vary-Search hint matched candidate we might end up not
  // using the processed_candidate at all. We will revisit this later.
  auto it = processed_candidates_.find(
      {url, blink::mojom::SpeculationAction::kPrefetch});
  if (it == processed_candidates_.end()) {
    return false;
  }
  return !prefetcher_.IsPrefetchAttemptFailedOrDiscarded(url);
}

std::pair<bool, bool> PreloadingDecider::MaybePrerender(
    const GURL& url,
    const PreloadingPredictor& enacting_predictor,
    PreloadingConfidence confidence) {
  std::pair<bool, bool> result{false, false};
  SpeculationCandidateKey key{url, blink::mojom::SpeculationAction::kPrerender};
  std::optional<std::pair<PreloadingDecider::SpeculationCandidateKey,
                          blink::mojom::SpeculationCandidatePtr>>
      matched_candidate_pair =
          GetMatchedPreloadingCandidate(key, enacting_predictor, confidence);
  if (!matched_candidate_pair.has_value()) {
    return result;
  }

  key = matched_candidate_pair.value().first;
  blink::mojom::SpeculationCandidatePtr candidate =
      std::move(matched_candidate_pair.value().second);
  result.first =
      prerenderer_->MaybePrerender(candidate, enacting_predictor, confidence);

  result.second =
      result.first && PredictionOccursInOtherWebContents(*candidate);

  auto it = on_standby_candidates_.find(key);
  CHECK(it != on_standby_candidates_.end());
  std::vector<blink::mojom::SpeculationCandidatePtr> processed =
      std::move(it->second);
  RemoveStandbyCandidate(it->first);
  processed_candidates_[std::move(key)] = std::move(processed);
  return result;
}

bool PreloadingDecider::ShouldWaitForPrerenderResult(const GURL& url) {
  auto it = processed_candidates_.find(
      {url, blink::mojom::SpeculationAction::kPrerender});
  if (it == processed_candidates_.end()) {
    return false;
  }
  return prerenderer_->ShouldWaitForPrerenderResult(url);
}

bool PreloadingDecider::IsSuitableCandidate(
    const blink::mojom::SpeculationCandidatePtr& candidate,
    const PreloadingPredictor& predictor,
    PreloadingConfidence confidence,
    blink::mojom::SpeculationAction action) const {
  EagernessSet eagerness_set_for_predictor =
      behavior_config_->EagernessSetForPredictor(predictor);

  // If the ML model is available, its decisions supersede the hover heuristic.
  if (ml_model_available_ &&
      predictor == preloading_predictor::kUrlPointerHoverOnAnchor) {
    eagerness_set_for_predictor.RemoveAll(
        behavior_config_->EagernessSetForPredictor(
            preloading_predictor::kPreloadingHeuristicsMLModel));
  }

  return eagerness_set_for_predictor.Has(candidate->eagerness) &&
         confidence >= behavior_config_->GetThreshold(predictor, action);
}

PreloadingDeciderObserverForTesting* PreloadingDecider::SetObserverForTesting(
    PreloadingDeciderObserverForTesting* observer) {
  return std::exchange(observer_for_testing_, observer);
}

Prerenderer& PreloadingDecider::GetPrerendererForTesting() {
  CHECK(prerenderer_);
  return *prerenderer_;
}

std::unique_ptr<Prerenderer> PreloadingDecider::SetPrerendererForTesting(
    std::unique_ptr<Prerenderer> prerenderer) {
  prerenderer->SetPrerenderCancellationCallback(base::BindRepeating(
      &OnPrerenderCanceled, render_frame_host().GetWeakDocumentPtr()));
  return std::exchange(prerenderer_, std::move(prerenderer));
}

bool PreloadingDecider::IsOnStandByForTesting(
    const GURL& url,
    blink::mojom::SpeculationAction action) const {
  return on_standby_candidates_.contains({url, action});
}

bool PreloadingDecider::HasCandidatesForTesting() const {
  return !on_standby_candidates_.empty() ||
         !no_vary_search_hint_on_standby_candidates_.empty() ||
         !processed_candidates_.empty();
}

void PreloadingDecider::OnPreloadDiscarded(SpeculationCandidateKey key) {
  auto it = processed_candidates_.find(key);
  // If the preload is triggered outside of `PreloadingDecider`, ignore it.
  // Currently, `PrerendererImpl` triggers prefetch ahead of prerender.
  if (it == processed_candidates_.end()) {
    return;
  }

  std::vector<blink::mojom::SpeculationCandidatePtr> candidates =
      std::move(it->second);
  processed_candidates_.erase(it);
  for (const auto& candidate : candidates) {
    if (candidate->eagerness != blink::mojom::SpeculationEagerness::kEager) {
      AddStandbyCandidate(candidate);
    }
    // TODO(crbug.com/40064525): Add support for the case where |candidate|'s
    // eagerness is kEager. In a scenario where the prefetch evicted is a
    // non-eager prefetch, we could theoretically reprefetch using the eager
    // candidate (and have it use the eager prefetch quota). In that scenario,
    // perhaps not evicting and just making the prefetch use the eager limit
    // might be a better option too. In the case where an eager prefetch is
    // evicted, we don't want to immediately try and reprefetch the candidate;
    // it would defeat the purpose of evicting in the first place, and due to a
    // possible-rentrancy into PrefetchService::Prefetch(), it could cause us to
    // exceed the limit.

    // TODO(crbug.com/40275452): Add implementation for the kEager case for
    // prerender.
  }
}

}  // namespace content
