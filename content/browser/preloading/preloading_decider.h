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
enum class PreloadingPredictor;

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

  // Set the new preloading decider observer for testing and returns the old
  // one.
  PreloadingDeciderObserverForTesting* SetObserverForTesting(
      PreloadingDeciderObserverForTesting* observer);

  // Processes the received speculation rules candidates list.
  void UpdateSpeculationCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

 private:
  explicit PreloadingDecider(RenderFrameHost* rfh);
  friend class DocumentUserData<PreloadingDecider>;
  DOCUMENT_USER_DATA_KEY_DECL();

  // Prefetches the |url| if it is safe and eligible to be prefetched. Returns
  // false if no on-standby candidate is found for the given |url|, or the
  // Prefetcher does not accept the candidate.
  bool MaybePrefetch(const GURL& url);
  // Whether a prefetch was attempted for the |url| and was it failed or
  // discarded by the Prefetcher.
  bool ShouldWaitForPrefetchResult(const GURL& url);
  // Helper function to add a preloading prediction for the |url|
  void AddPreloadingPrediction(const GURL& url, PreloadingPredictor predictor);

  using SpeculationCandidateKey =
      std::pair<GURL, blink::mojom::SpeculationAction>;
  // |on_standby_candidates_| stores preloading candidates for each target URL,
  // action pairs that are safe to perform but are not marked as |kEager| and
  // should be performed when we are confident enough that the user will most
  // likely navigate to the target URL.
  std::map<SpeculationCandidateKey, blink::mojom::SpeculationCandidatePtr>
      on_standby_candidates_;
  // |processed_candidates_| stores all target URL, action pairs that are
  // already processed by prefetcher or prerenderer. Right now it is needed to
  // avoid adding such candidates back to |on_standby_candidates_| whenever
  // there is an update in speculation rules.
  std::set<SpeculationCandidateKey> processed_candidates_;

  raw_ptr<PreloadingDeciderObserverForTesting> observer_for_testing_;
  Preconnector preconnector_;
  Prefetcher prefetcher_;
  Prerenderer prerenderer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_DECIDER_H_
