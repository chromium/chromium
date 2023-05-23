// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading_decider.h"

#include "base/check_op.h"
#include "base/containers/enum_set.h"
#include "base/strings/string_split.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/prerenderer_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using EagernessSet =
    base::EnumSet<blink::mojom::SpeculationEagerness,
                  blink::mojom::SpeculationEagerness::kMinValue,
                  blink::mojom::SpeculationEagerness::kMaxValue>;

EagernessSet EagernessSetFromFeatureParam(base::StringPiece value) {
  EagernessSet set;
  for (base::StringPiece piece : base::SplitStringPiece(
           value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (piece == "conservative") {
      set.Put(blink::mojom::SpeculationEagerness::kConservative);
    } else if (piece == "moderate") {
      set.Put(blink::mojom::SpeculationEagerness::kModerate);
    }
  }
  return set;
}

}  // namespace

struct PreloadingDecider::BehaviorConfig {
  BehaviorConfig() {
    static const base::FeatureParam<std::string> kPointerDownEagerness{
        &blink::features::kSpeculationRulesPointerDownHeuristics,
        "pointer_down_eagerness", "conservative,moderate"};
    pointer_down_eagerness =
        EagernessSetFromFeatureParam(kPointerDownEagerness.Get());

    static const base::FeatureParam<std::string> kPointerHoverEagerness{
        &blink::features::kSpeculationRulesPointerHoverHeuristics,
        "pointer_hover_eagerness", "moderate"};
    pointer_hover_eagerness =
        EagernessSetFromFeatureParam(kPointerHoverEagerness.Get());
  }

  EagernessSet EagernessSetForPredictor(
      const PreloadingPredictor& predictor) const {
    if (predictor.ukm_value() ==
        preloading_predictor::kUrlPointerDownOnAnchor.ukm_value()) {
      return pointer_down_eagerness;
    } else if (predictor.ukm_value() ==
               preloading_predictor::kUrlPointerHoverOnAnchor.ukm_value()) {
      return pointer_hover_eagerness;
    } else {
      DLOG(WARNING) << "unexpected predictor " << predictor.name() << "/"
                    << predictor.ukm_value();
      return {};
    }
  }

  EagernessSet pointer_down_eagerness;
  EagernessSet pointer_hover_eagerness;
};

DOCUMENT_USER_DATA_KEY_IMPL(PreloadingDecider);

PreloadingDecider::PreloadingDecider(RenderFrameHost* rfh)
    : DocumentUserData<PreloadingDecider>(rfh),
      behavior_config_(std::make_unique<BehaviorConfig>()),
      observer_for_testing_(nullptr),
      preconnector_(render_frame_host()),
      prefetcher_(render_frame_host()),
      prerenderer_(std::make_unique<PrerendererImpl>(render_frame_host())) {}

PreloadingDecider::~PreloadingDecider() = default;

void PreloadingDecider::AddPreloadingPrediction(const GURL& url,
                                                PreloadingPredictor predictor) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  auto* preloading_data =
      PreloadingData::GetOrCreateForWebContents(web_contents);
  preloading_data->AddPreloadingPrediction(
      predictor,
      /*confidence=*/100, PreloadingData::GetSameURLMatcher(url));
}

void PreloadingDecider::OnPointerDown(const GURL& url) {
  if (observer_for_testing_) {
    observer_for_testing_->OnPointerDown(url);
  }
  // For pointer down link selection heuristics, we first call |MaybePrerender|
  // to check whether it is safe to prerender the |url| and if so we request to
  // prerender the |url| and return. Otherwise, by calling
  // |ShouldWaitForPrerenderResult| we check whether there is an active
  // prerender is in progress for |url| or will return if there is one. We then
  // call |MaybePrefetch| to check whether prefetching the |url| is safe and if
  // so we request the new prefetch and return. Otherwise, we call
  // |ShouldWaitForPrefetchResult| to check whether there is an active prefetch
  // in progress for the |url| and return if there is one. At last, we request a
  // preconnect for the |url| if prefetching the |url| is not allowed or has
  // failed before.
  if (base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesPointerDownHeuristics)) {
    if (MaybePrerender(url, preloading_predictor::kUrlPointerDownOnAnchor)) {
      AddPreloadingPrediction(url,
                              preloading_predictor::kUrlPointerDownOnAnchor);
      return;
    }
    if (ShouldWaitForPrerenderResult(url))
      return;

    if (MaybePrefetch(url, preloading_predictor::kUrlPointerDownOnAnchor)) {
      AddPreloadingPrediction(url,
                              preloading_predictor::kUrlPointerDownOnAnchor);
      return;
    }
    // Ideally it is preferred to fallback to preconnect asynchronously if a
    // prefetch attempt fails. We should revisit it later perhaps after having
    // data showing it is worth doing so.
    if (ShouldWaitForPrefetchResult(url))
      return;
  }
  preconnector_.MaybePreconnect(url);
}

void PreloadingDecider::OnPointerHover(const GURL& url) {
  if (observer_for_testing_) {
    observer_for_testing_->OnPointerHover(url);
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesPointerHoverHeuristics)) {
    // First try to prerender the |url|, if not possible try to prefetch,
    // otherwise try to preconnect to it.
    if (MaybePrerender(url, preloading_predictor::kUrlPointerHoverOnAnchor)) {
      AddPreloadingPrediction(url,
                              preloading_predictor::kUrlPointerHoverOnAnchor);
      return;
    }
    if (ShouldWaitForPrerenderResult(url))
      return;

    if (MaybePrefetch(url, preloading_predictor::kUrlPointerHoverOnAnchor)) {
      AddPreloadingPrediction(url,
                              preloading_predictor::kUrlPointerHoverOnAnchor);
      return;
    }
    // ditto (async fallback)
    if (ShouldWaitForPrefetchResult(url))
      return;
    preconnector_.MaybePreconnect(url);
  }
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

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  auto* preloading_data =
      PreloadingData::GetOrCreateForWebContents(web_contents);
  preloading_data->SetIsNavigationInDomainCallback(
      content_preloading_predictor::kSpeculationRules,
      base::BindRepeating([](NavigationHandle* navigation_handle) -> bool {
        return ui::PageTransitionIsWebTriggerable(
            navigation_handle->GetPageTransition());
      }));
  if (base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesPointerDownHeuristics)) {
    preloading_data->SetIsNavigationInDomainCallback(
        preloading_predictor::kUrlPointerDownOnAnchor,
        base::BindRepeating([](NavigationHandle* navigation_handle) -> bool {
          return ui::PageTransitionCoreTypeIs(
                     navigation_handle->GetPageTransition(),
                     ui::PageTransition::PAGE_TRANSITION_LINK) &&
                 ui::PageTransitionIsNewNavigation(
                     navigation_handle->GetPageTransition());
        }));
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kSpeculationRulesPointerHoverHeuristics)) {
    preloading_data->SetIsNavigationInDomainCallback(
        preloading_predictor::kUrlPointerHoverOnAnchor,
        base::BindRepeating([](NavigationHandle* navigation_handle) -> bool {
          return ui::PageTransitionCoreTypeIs(
                     navigation_handle->GetPageTransition(),
                     ui::PageTransition::PAGE_TRANSITION_LINK) &&
                 ui::PageTransitionIsNewNavigation(
                     navigation_handle->GetPageTransition());
        }));
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

    processed_candidates_.insert(std::move(key));
    // TODO(crbug.com/1341019): Pass the action requested by speculation rules
    // to PreloadingPrediction.
    AddPreloadingPrediction(candidate->url, GetPredictorForSpeculationRules(
                                                candidate->injection_world));

    return false;
  };

  ClearStandbyCandidates();
  base::EraseIf(candidates, should_mark_as_on_standby);

  prefetcher_.ProcessCandidatesForPrefetch(candidates);

  prerenderer_->ProcessCandidatesForPrerender(candidates);
}

bool PreloadingDecider::MaybePrefetch(const GURL& url,
                                      const PreloadingPredictor& predictor) {
  SpeculationCandidateKey key{url, blink::mojom::SpeculationAction::kPrefetch};
  std::vector<blink::mojom::SpeculationCandidatePtr> candidates;

  auto it = on_standby_candidates_.find(key);
  if (it != on_standby_candidates_.end()) {
    auto inner_it =
        base::ranges::find_if(it->second, [&](const auto& candidate) {
          return IsSuitableCandidate(candidate, predictor);
        });
    if (inner_it != it->second.end()) {
      // TODO(isaboori): prefetcher should provide MaybePrefetch interface to
      // directly send the candidate to it instead of passing it as a vector.
      candidates.push_back(inner_it->Clone());
    }
  }

  if (candidates.empty()) {
    // Check all URLs that might match via NVS hint.
    // If there are multiple candidates that match prefetch the first one.
    GURL::Replacements replacements;
    replacements.ClearRef();
    replacements.ClearQuery();
    const GURL url_without_query_and_ref = url.ReplaceComponents(replacements);
    auto nvs_it = no_vary_search_hint_on_standby_candidates_.find(
        {url_without_query_and_ref,
         blink::mojom::SpeculationAction::kPrefetch});
    if (nvs_it == no_vary_search_hint_on_standby_candidates_.end()) {
      return false;
    }
    for (const auto& standby_key : nvs_it->second) {
      CHECK_EQ(standby_key.second, blink::mojom::SpeculationAction::kPrefetch);
      const GURL& prefetch_url = standby_key.first;
      // Every prefetch in this set might come back with NVS header of
      // "params" and match. But we will consider only the first prefetch that
      // has a No-Vary-Search hint that is matching.
      auto standby_it = on_standby_candidates_.find(standby_key);
      CHECK(standby_it != on_standby_candidates_.end());
      auto inner_it =
          base::ranges::find_if(standby_it->second, [&](const auto& candidate) {
            return candidate->no_vary_search_hint &&
                   NoVarySearchHelper::ParseHttpNoVarySearchDataFromMojom(
                       candidate->no_vary_search_hint)
                       .AreEquivalent(url, prefetch_url) &&
                   IsSuitableCandidate(candidate, predictor);
          });
      if (inner_it != standby_it->second.end()) {
        candidates.push_back(inner_it->Clone());
        key = standby_key;
        break;
      }
    }
  }

  if (candidates.empty()) {
    return false;
  }

  prefetcher_.ProcessCandidatesForPrefetch(candidates);
  bool result = candidates.empty();

  RemoveStandbyCandidate(key);
  processed_candidates_.insert(std::move(key));
  return result;
}

bool PreloadingDecider::ShouldWaitForPrefetchResult(const GURL& url) {
  // TODO(liviutinta): Don't implement any No-Vary-Search hint matching here
  // for now. It is not clear how to match `url` with a `processed_candidate`.
  // Also, for a No-Vary-Search hint matched candidate we might end up not
  // using the processed_candidate at all. We will revisit this later.
  auto it = processed_candidates_.find(
      {url, blink::mojom::SpeculationAction::kPrefetch});
  if (it == processed_candidates_.end())
    return false;
  return !prefetcher_.IsPrefetchAttemptFailedOrDiscarded(url);
}

bool PreloadingDecider::MaybePrerender(const GURL& url,
                                       const PreloadingPredictor& predictor) {
  SpeculationCandidateKey key{url, blink::mojom::SpeculationAction::kPrerender};
  auto it = on_standby_candidates_.find(key);
  if (it == on_standby_candidates_.end()) {
    return false;
  }

  auto inner_it = base::ranges::find_if(it->second, [&](const auto& candidate) {
    return IsSuitableCandidate(candidate, predictor);
  });
  if (inner_it == it->second.end()) {
    return false;
  }

  bool result = prerenderer_->MaybePrerender(inner_it->Clone());

  RemoveStandbyCandidate(it->first);
  processed_candidates_.insert(std::move(key));
  return result;
}

bool PreloadingDecider::ShouldWaitForPrerenderResult(const GURL& url) {
  auto it = processed_candidates_.find(
      {url, blink::mojom::SpeculationAction::kPrerender});
  if (it == processed_candidates_.end())
    return false;
  return prerenderer_->ShouldWaitForPrerenderResult(url);
}

bool PreloadingDecider::IsSuitableCandidate(
    const blink::mojom::SpeculationCandidatePtr& candidate,
    const PreloadingPredictor& predictor) const {
  return behavior_config_->EagernessSetForPredictor(predictor).Has(
      candidate->eagerness);
}

PreloadingDeciderObserverForTesting* PreloadingDecider::SetObserverForTesting(
    PreloadingDeciderObserverForTesting* observer) {
  return std::exchange(observer_for_testing_, observer);
}

std::unique_ptr<Prerenderer> PreloadingDecider::SetPrerendererForTesting(
    std::unique_ptr<Prerenderer> prerenderer) {
  return std::exchange(prerenderer_, std::move(prerenderer));
}

bool PreloadingDecider::IsOnStandByForTesting(
    const GURL& url,
    blink::mojom::SpeculationAction action) {
  return on_standby_candidates_.find({url, action}) !=
         on_standby_candidates_.end();
}

}  // namespace content
