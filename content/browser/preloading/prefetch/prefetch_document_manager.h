// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/prefetch_metrics.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"
#include "third_party/blink/public/mojom/tokens/tokens.mojom.h"
#include "url/gurl.h"

namespace content {

class PrefetchContainer;
class PrefetchHandle;
class PrefetchService;
class PrefetchType;
class PreloadPipelineInfo;
class PreloadingPredictor;
class SpeculationRulesTags;
enum class PreloadingType;

// Manages the state of and tracks metrics about prefetches for a single page
// load.
class CONTENT_EXPORT PrefetchDocumentManager
    : public DocumentUserData<PrefetchDocumentManager> {
 public:
  using PrefetchDestructionCallback =
      base::RepeatingCallback<void(const GURL&)>;

  ~PrefetchDocumentManager() override;

  PrefetchDocumentManager(const PrefetchDocumentManager&) = delete;
  const PrefetchDocumentManager operator=(const PrefetchDocumentManager&) =
      delete;

  // Returns the `PrefetchDocumentManager` associated with a Document if already
  // exists, or `nullptr` otherwise.
  static PrefetchDocumentManager* FromDocumentToken(
      int process_id,
      const blink::DocumentToken& document_token);

  // Processes the given speculation candidates to see if they can be
  // prefetched. Any candidates that can be prefetched are removed from
  // |candidates|, and a prefetch for the URL of the candidate is started.
  void ProcessCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates);

  // Attempts to prefetch the given candidate. Returns true if a new prefetch
  // for the candidate's URL is started.
  bool MaybePrefetch(blink::mojom::SpeculationCandidatePtr candidate,
                     const PreloadingPredictor& enacting_predictor);

  void PrefetchAheadOfPrerender(
      scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
      blink::mojom::SpeculationCandidatePtr candidate,
      const PreloadingPredictor& enacting_predictor);

  // Starts the process to prefetch |url| with the given |prefetch_type|.
  void PrefetchUrl(const GURL& url,
                   const PrefetchType& prefetch_type,
                   const PreloadingPredictor& enacting_predictor,
                   const blink::mojom::Referrer& referrer,
                   std::optional<SpeculationRulesTags> speculation_rules_tags,
                   const network::mojom::NoVarySearchPtr& no_vary_search_hint,
                   scoped_refptr<PreloadPipelineInfo> preload_pipeline_info);

  // Checking the canary cache can be a slow and blocking operation (see
  // crbug.com/1266018), so we only do this for the first non-decoy prefetch we
  // make on the page.
  bool HaveCanaryChecksStarted() const { return have_canary_checks_started_; }
  void OnCanaryChecksStarted() { have_canary_checks_started_ = true; }

  // Returns metrics for prefetches requested by the associated page load.
  PrefetchReferringPageMetrics& GetReferringPageMetrics() {
    return referring_page_metrics_;
  }

  // Updates metrics when the eligibility check for a prefetch requested by this
  // page load is completed.
  void OnEligibilityCheckComplete(bool is_eligible);

  // Updates metrics when the response for a prefetch requested by this page
  // load is received.
  void OnPrefetchSuccessful(PrefetchContainer* prefetch);

  // Whether the prefetch attempt for target |url| failed or discarded
  bool IsPrefetchAttemptFailedOrDiscarded(const GURL& url);

  // Returns a tuple: (can_prefetch_now, prefetch_to_evict). 'can_prefetch_now'
  // is true if we can prefetch |next_prefetch| based on the state of the
  // document, and the number of existing completed prefetches. The eagerness of
  // |next_prefetch| is taken into account when making the decision.
  // 'prefetch_to_evict' is set to an existing prefetch if one needs to be
  // evicted to make space for the prefetch of |next_prefetch|, or nullptr
  // otherwise. 'prefetch_to_evict' will only be non-null if 'can_prefetch_now'
  // is true.
  std::tuple<bool, base::WeakPtr<PrefetchContainer>> CanPrefetchNow(
      PrefetchContainer* next_prefetch);

  // See documentation for |prefetch_destruction_callback_|.
  void SetPrefetchDestructionCallback(PrefetchDestructionCallback callback);

  // Called when a PrefetchContainer started by |this| is being destroyed.
  void PrefetchWillBeDestroyed(PrefetchContainer* prefetch);

  base::WeakPtr<PrefetchDocumentManager> GetWeakPtr() {
    return weak_method_factory_.GetWeakPtr();
  }

  static void SetPrefetchServiceForTesting(PrefetchService* prefetch_service);

  void ResetPrefetchAheadOfPrerenderIfExist(PreloadingType preloading_type,
                                            const GURL& url);

 private:
  explicit PrefetchDocumentManager(RenderFrameHost* rfh);
  friend DocumentUserData;

  // Helper function to get the |PrefetchService| associated with |this|.
  PrefetchService* GetPrefetchService() const;

  bool IsPrefetchAttemptFailedOrDiscardedInternal(
      const GURL& url,
      PreloadingType planned_max_preloading_type);

  blink::DocumentToken document_token_;

  // This map holds references to all |PrefetchContainer| associated with
  // |this|.
  //
  // Keyed with `(url, planned_max_preloading_type)`.
  // `planned_max_preloading_type == kPrerender` indicates it's ahead of
  // prerender.
  //
  // We allow normal prefetch and prefetch ahead of prerender with the same key
  // here, to handle and merge them in `PrefetchService`.
  std::map<std::pair<GURL, PreloadingType>, std::unique_ptr<PrefetchHandle>>
      all_prefetches_;

  // Stores whether or not canary checks have been started for this page.
  bool have_canary_checks_started_{false};

  // A list of immediate prefetch requests (from this page) that have completed
  // (oldest to newest).
  std::vector<base::WeakPtr<PrefetchContainer>> completed_immediate_prefetches_;
  // A list of non-immediate prefetch requests (from this page) that have
  // completed (oldest to newest).
  std::vector<base::WeakPtr<PrefetchContainer>>
      completed_non_immediate_prefetches_;

  // Metrics related to the prefetches requested by this page load.
  PrefetchReferringPageMetrics referring_page_metrics_;

  // Callback that is run when a prefetch started by |this| is being destroyed.
  PrefetchDestructionCallback prefetch_destruction_callback_;

  base::WeakPtrFactory<PrefetchDocumentManager> weak_method_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
