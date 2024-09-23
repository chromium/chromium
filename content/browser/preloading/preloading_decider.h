// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_

#include "content/browser/preloading/preconnector.h"
#include "content/browser/preloading/prefetcher.h"
#include "content/browser/preloading/preloading_confidence.h"
#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/document_user_data.h"
#include "third_party/blink/public/mojom/preloading/anchor_element_interaction_host.mojom-forward.h"

namespace content {

class RenderFrameHost;
class PreloadingPredictor;

class PreloadingDeciderObserverForTesting {
 public:
  virtual ~PreloadingDeciderObserverForTesting() = default;

  virtual void UpdateSpeculationCandidates(
      const std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) = 0;
  virtual void OnPointerDown(const GURL& url) = 0;
  virtual void OnPointerHover(const GURL& url) = 0;
};

// Processes user interaction events and developer provided speculation-rules
// and based on some heuristics decides which preloading actions are safe and
// worth executing.
// TODO(isaboori): implement the preloading link selection heuristics logic
class CONTENT_EXPORT PreloadingDecider
    : public DocumentUserData<PreloadingDecider> {
 public:
  using SpeculationCandidateKey =
      std::pair<GURL, blink::mojom::SpeculationAction>;

  ~PreloadingDecider() override;

  // Receives and processes on pointer down event for 'url' target link.
  void OnPointerDown(const GURL& url);

  // Receives and processes on pointer hover event for 'url' target link.
  void OnPointerHover(const GURL& url,
                      blink::mojom::AnchorElementPointerDataPtr mouse_data);

  //  Receives and processes ML model score for 'url' target link.
  void OnPreloadingHeuristicsModelDone(const GURL& url, float score);

  // Sets the new preloading decider observer for testing and returns the old
  // one.
  PreloadingDeciderObserverForTesting* SetObserverForTesting(
      PreloadingDeciderObserverForTesting* observer);

  // Returns the prerenderer for testing.
  Prerenderer& GetPrerendererForTesting();

  // Sets the new prerenderer for testing and returns the old one.
  std::unique_ptr<Prerenderer> SetPrerendererForTesting(
      std::unique_ptr<Prerenderer> prerenderer);

  // Processes the received speculation rules candidates list.
  void UpdateSpeculationCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  // Called when LCP is predicted.
  // This is used to defer starting prerenders until LCP timing and is only
  // used under LCPTimingPredictorPrerender2.
  void OnLCPPredicted();

  // Returns true if the |url|, |action| pair is in the on-standby list.
  bool IsOnStandByForTesting(const GURL& url,
                             blink::mojom::SpeculationAction action) const;

  // Returns true if there are any candidates.
  bool HasCandidatesForTesting() const;

  // Called by PrefetchService/PrerendererImpl when a prefetch/prerender is
  // evicted/canceled.
  void OnPreloadDiscarded(const SpeculationCandidateKey key);

 private:
  explicit PreloadingDecider(RenderFrameHost* rfh);
  friend class DocumentUserData<PreloadingDecider>;
  DOCUMENT_USER_DATA_KEY_DECL();

  // Attempts preloading actions starting from the most advanced (prerendering)
  // to least (preconnect), in response to `enacting_predictor` predicting a
  // navigation to `url`. If `fallback_to_preconnect` is true, we preconnect if
  // no other action is taken.
  void MaybeEnactCandidate(const GURL& url,
                           const PreloadingPredictor& enacting_predictor,
                           PreloadingConfidence confidence,
                           bool fallback_to_preconnect);

  // Prefetches the |url| if it is safe and eligible to be prefetched. Returns
  // false if no suitable (given |enacting_predictor|) on-standby candidate is
  // found for the given |url|, or the Prefetcher does not accept the candidate.
  bool MaybePrefetch(const GURL& url,
                     const PreloadingPredictor& enacting_predictor,
                     PreloadingConfidence confidence);

  // Returns true if a prefetch was attempted for the |url| and is not failed or
  // discarded by Prefetcher yet, and we should wait for it to finish.
  bool ShouldWaitForPrefetchResult(const GURL& url);

  // Prerenders the |url| if it is safe and eligible to be prerendered. Returns
  // false for the first bool if no suitable (given |enacting_predictor|)
  // on-standby candidate is found for the given |url|, or the Prerenderer does
  // not accept the candidate. Returns true for the second bool if a
  // PreloadingPrediction has been added.
  std::pair<bool, bool> MaybePrerender(
      const GURL& url,
      const PreloadingPredictor& enacting_predictor,
      PreloadingConfidence confidence);

  // Returns true if a prerender was attempted for the |url| and is not failed
  // or discarded by Prerenderer yet, and we should wait for it to finish.
  bool ShouldWaitForPrerenderResult(const GURL& url);

  // Helper function to add a preloading prediction for the |url|
  void AddPreloadingPrediction(const GURL& url,
                               PreloadingPredictor predictor,
                               PreloadingConfidence confidence);

  // Return true if |candidate| can be selected in response to a prediction by
  // |predictor|.
  bool IsSuitableCandidate(
      const blink::mojom::SpeculationCandidatePtr& candidate,
      const PreloadingPredictor& predictor,
      PreloadingConfidence confidence,
      blink::mojom::SpeculationAction action) const;

  // Helper functions to add/remove a preloading candidate to
  // |on_standby_candidates_| and to reset |on_standby_candidates_|. Use these
  // methods to make sure |on_standby_candidates_| and
  // |no_vary_search_hint_on_standby_candidates_| are kept in sync
  void AddStandbyCandidate(
      const blink::mojom::SpeculationCandidatePtr& candidate);
  void RemoveStandbyCandidate(const SpeculationCandidateKey key);
  void ClearStandbyCandidates();

  // Helper functions to select a prerender/prefetch candidate to be
  // triggered.
  std::optional<
      std::pair<SpeculationCandidateKey, blink::mojom::SpeculationCandidatePtr>>
  GetMatchedPreloadingCandidate(const SpeculationCandidateKey& lookup_key,
                                const PreloadingPredictor& enacting_predictor,
                                PreloadingConfidence confidence) const;
  std::optional<
      std::pair<SpeculationCandidateKey, blink::mojom::SpeculationCandidatePtr>>
  GetMatchedPreloadingCandidateByNoVarySearchHint(
      const SpeculationCandidateKey& lookup_key,
      const PreloadingPredictor& enacting_predictor,
      PreloadingConfidence confidence) const;

  // |on_standby_candidates_| stores preloading candidates for each target URL,
  // action pairs that are safe to perform but are not marked as |kEager| and
  // should be performed when we are confident enough that the user will most
  // likely navigate to the target URL.
  std::map<SpeculationCandidateKey,
           std::vector<blink::mojom::SpeculationCandidatePtr>>
      on_standby_candidates_;

  // |nvs_hint_on_standby_candidates_| stores for a URL without query and
  // fragment, action pairs that are safe to perform but are not marked as
  // |kEager| and should be performed when we are confident enough that the user
  // will most likely navigate to a URL that matches based on the presence
  // of No-Vary-Search hint the candidate's URL.
  // This map needs to be kept in sync with the |on_standby_candidates_| map.
  std::map<SpeculationCandidateKey, std::set<SpeculationCandidateKey>>
      no_vary_search_hint_on_standby_candidates_;

  // |processed_candidates_| stores all target URL, action pairs that are
  // already processed by prefetcher or prerenderer, and maps them to all
  // candidates with the same URL, action pair. Right now it is needed to avoid
  // adding such candidates back to |on_standby_candidates_| whenever there is
  // an update in speculation rules.
  std::map<SpeculationCandidateKey,
           std::vector<blink::mojom::SpeculationCandidatePtr>>
      processed_candidates_;

  // Behavior determined dynamically. Stored on this object rather than globally
  // so that it does not span unit tests.
  class BehaviorConfig;
  std::unique_ptr<const BehaviorConfig> behavior_config_;

  // Whether this page has ever received an ML model prediction. Once it has,
  // the model predictions supersede the hover heuristic. We store this here,
  // rather than per-BrowserContext, since even if the model is loaded, it may
  // not run for some pages (e.g. insecure http).
  bool ml_model_available_ = false;

  raw_ptr<PreloadingDeciderObserverForTesting> observer_for_testing_;
  Preconnector preconnector_;
  Prefetcher prefetcher_;
  std::unique_ptr<Prerenderer> prerenderer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_
