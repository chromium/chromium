// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_

#include "content/browser/preloading/preconnector.h"
#include "content/browser/preloading/prefetcher.h"
#include "content/browser/preloading/prerenderer.h"
#include "content/public/browser/document_user_data.h"

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
  ~PreloadingDecider() override;

  // Receives and processes on pointer down event for 'url' target link.
  void OnPointerDown(const GURL& url);

  // Receives and processes on pointer hover event for 'url' target link.
  void OnPointerHover(const GURL& url);

  // Sets the new preloading decider observer for testing and returns the old
  // one.
  PreloadingDeciderObserverForTesting* SetObserverForTesting(
      PreloadingDeciderObserverForTesting* observer);

  // Sets the new prerenderer for testing and returns the old one.
  std::unique_ptr<Prerenderer> SetPrerendererForTesting(
      std::unique_ptr<Prerenderer> prerenderer);

  // Processes the received speculation rules candidates list.
  void UpdateSpeculationCandidates(
      const base::UnguessableToken& initiator_devtools_navigation_token,
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  // Returns true if the |url|, |action| pair is in the on-standby list.
  bool IsOnStandByForTesting(const GURL& url,
                             blink::mojom::SpeculationAction action);

 private:
  explicit PreloadingDecider(RenderFrameHost* rfh);
  friend class DocumentUserData<PreloadingDecider>;
  DOCUMENT_USER_DATA_KEY_DECL();

  // Prefetches the |url| if it is safe and eligible to be prefetched. Returns
  // false if no suitable (given |predictor|) on-standby candidate is found for
  // the given |url|, or the Prefetcher does not accept the candidate.
  bool MaybePrefetch(const GURL& url, const PreloadingPredictor& predictor);

  // Returns true if a prefetch was attempted for the |url| and is not failed or
  // discarded by Prefetcher yet, and we should wait for it to finish.
  bool ShouldWaitForPrefetchResult(const GURL& url);

  // Prerenders the |url| if it is safe and eligible to be prerendered. Returns
  // false if no suitable (given |predictor|) on-standby candidate is found for
  // the given |url|, or the Prerenderer does not accept the candidate.
  bool MaybePrerender(const GURL& url, const PreloadingPredictor& predictor);

  // Returns true if a prerender was attempted for the |url| and is not failed
  // or discarded by Prerenderer yet, and we should wait for it to finish.
  bool ShouldWaitForPrerenderResult(const GURL& url);

  // Helper function to add a preloading prediction for the |url|
  void AddPreloadingPrediction(const GURL& url, PreloadingPredictor predictor);

  // Return true if |candidate| can be selected in response to a prediction by
  // |predictor|.
  bool IsSuitableCandidate(
      const blink::mojom::SpeculationCandidatePtr& candidate,
      const PreloadingPredictor& predictor) const;

  using SpeculationCandidateKey =
      std::pair<GURL, blink::mojom::SpeculationAction>;

  // |on_standby_candidates_| stores preloading candidates for each target URL,
  // action pairs that are safe to perform but are not marked as |kEager| and
  // should be performed when we are confident enough that the user will most
  // likely navigate to the target URL.
  std::map<SpeculationCandidateKey,
           std::vector<blink::mojom::SpeculationCandidatePtr>>
      on_standby_candidates_;

  // |processed_candidates_| stores all target URL, action pairs that are
  // already processed by prefetcher or prerenderer. Right now it is needed to
  // avoid adding such candidates back to |on_standby_candidates_| whenever
  // there is an update in speculation rules.
  std::set<SpeculationCandidateKey> processed_candidates_;

  // Behavior determined dynamically. Stored on this object rather than globally
  // so that it does not span unit tests.
  struct BehaviorConfig;
  std::unique_ptr<const BehaviorConfig> behavior_config_;

  absl::optional<base::UnguessableToken> initiator_devtools_navigation_token_;

  raw_ptr<PreloadingDeciderObserverForTesting> observer_for_testing_;
  Preconnector preconnector_;
  Prefetcher prefetcher_;
  std::unique_ptr<Prerenderer> prerenderer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_
