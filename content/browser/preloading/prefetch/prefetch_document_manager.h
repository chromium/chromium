// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/speculation_host_devtools_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/http/http_no_vary_search_data.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;
class PrefetchContainer;
class PrefetchService;

// Manages the state of and tracks metrics about prefetches for a single page
// load.
class CONTENT_EXPORT PrefetchDocumentManager
    : public DocumentUserData<PrefetchDocumentManager>,
      public WebContentsObserver {
 public:
  using PrefetchDestructionCallback =
      base::RepeatingCallback<void(const GURL&)>;

  ~PrefetchDocumentManager() override;

  PrefetchDocumentManager(const PrefetchDocumentManager&) = delete;
  const PrefetchDocumentManager operator=(const PrefetchDocumentManager&) =
      delete;

  // WebContentsObserver.
  void DidStartNavigation(NavigationHandle* navigation_handle) override;

  // Processes the given speculation candidates to see if they can be
  // prefetched. Any candidates that can be prefetched are removed from
  // |candidates|, and a prefetch for the URL of the candidate is started.
  void ProcessCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates,
      base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer);

  // Attempts to prefetch the given candidate. Returns true if a new prefetch
  // for the candidate's URL is started.
  bool MaybePrefetch(
      blink::mojom::SpeculationCandidatePtr candidate,
      base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer);

  // Starts the process to prefetch |url| with the given |prefetch_type|.
  void PrefetchUrl(
      const GURL& url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer,
      const network::mojom::NoVarySearchPtr& no_vary_search_expected,
      blink::mojom::SpeculationInjectionWorld world,
      base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer);

  // Releases ownership of the |PrefetchContainer| associated with |url|. The
  // prefetch is removed from |owned_prefetches_|, but a pointer to it remains
  // in |all_prefetches_|.
  std::unique_ptr<PrefetchContainer> ReleasePrefetchContainer(const GURL& url);

  // Checking the canary cache can be a slow and blocking operation (see
  // crbug.com/1266018), so we only do this for the first non-decoy prefetch we
  // make on the page.
  bool HaveCanaryChecksStarted() const { return have_canary_checks_started_; }
  void OnCanaryChecksStarted() { have_canary_checks_started_ = true; }

  // A page can only start |PrefetchServiceMaximumNumberOfPrefetchesPerPage|
  // number of prefetch requests.
  int GetNumberOfPrefetchRequestAttempted() const {
    return number_prefetch_request_attempted_;
  }
  void OnPrefetchRequestAttempted() { number_prefetch_request_attempted_++; }

  // Returns metrics for prefetches requested by the associated page load.
  PrefetchReferringPageMetrics& GetReferringPageMetrics() {
    return referring_page_metrics_;
  }

  // Updates metrics when the eligibility check for a prefetch requested by this
  // page load is completed.
  void OnEligibilityCheckComplete(bool is_eligible);

  // Called when the head is available in the prefetched response.
  void OnPrefetchedHeadReceived(const GURL& url);

  // Updates metrics when the response for a prefetch requested by this page
  // load is received.
  void OnPrefetchSuccessful(PrefetchContainer* prefetch);

  // Whether the prefetch attempt for target |url| failed or discarded
  bool IsPrefetchAttemptFailedOrDiscarded(const GURL& url);

  base::WeakPtr<PrefetchContainer> MatchUrl(const GURL& url) const;
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
  GetAllForUrlWithoutRefAndQueryForTesting(const GURL& url) const;
  void EnableNoVarySearchSupport();

  // Returns true if we can prefetch |next_prefetch| based on the number of
  // existing completed prefetches. This method will make room for
  // another prefetch by evicting an existing prefetch if possible. The
  // eagerness of |next_prefetch| is taken into account when making the
  // decision.
  bool CanPrefetchNow(PrefetchContainer* next_prefetch);

  // See documentation for |prefetch_destruction_callback_|.
  void SetPrefetchDestructionCallback(PrefetchDestructionCallback callback);

  // Called when a PrefetchContainer started by |this| is being destroyed.
  void PrefetchWillBeDestroyed(PrefetchContainer* prefetch);

  // Destroys |prefetch|. |prefetch| could either be owned by |this| or by
  // PrefetchService.
  void EvictPrefetch(base::WeakPtr<PrefetchContainer> prefetch);

  base::WeakPtr<PrefetchDocumentManager> GetWeakPtr() {
    return weak_method_factory_.GetWeakPtr();
  }

  static void SetPrefetchServiceForTesting(PrefetchService* prefetch_service);

 private:
  explicit PrefetchDocumentManager(RenderFrameHost* rfh);
  friend DocumentUserData;

  // Helper function to get the |PrefetchService| associated with |this|.
  PrefetchService* GetPrefetchService() const;

  // This map holds references to all |PrefetchContainer| associated with
  // |this|, regardless of ownership.
  std::map<GURL, base::WeakPtr<PrefetchContainer>> all_prefetches_;

  // This map holds all |PrefetchContainer| currently owned by |this|. |this|
  // owns all |PrefetchContainer| from when they are created in |PrefetchUrl|
  // until |PrefetchService| starts the network request for the prefetch, at
  // which point |PrefetchService| takes ownership.
  std::map<GURL, std::unique_ptr<PrefetchContainer>> owned_prefetches_;

  // Stores whether or not canary checks have been started for this page.
  bool have_canary_checks_started_{false};

  // The number of prefetch requests that have been attempted for prefetches
  // requested by this page.
  int number_prefetch_request_attempted_{0};

  // The number of eager prefetch requests (from this page) that have completed.
  // An 'eager' prefetch is a prefetch whose eagerness is kEager.
  size_t number_eager_prefetches_completed_{0};
  // A list of non-eager prefetch requests (from this page) that have completed
  // (oldest to newest).
  base::circular_deque<base::WeakPtr<PrefetchContainer>>
      completed_non_eager_prefetches_;

  // Metrics related to the prefetches requested by this page load.
  PrefetchReferringPageMetrics referring_page_metrics_;

  bool no_vary_search_support_enabled_ = false;

  // Callback that is run when a prefetch started by |this| is being destroyed.
  PrefetchDestructionCallback prefetch_destruction_callback_;

  base::WeakPtrFactory<PrefetchDocumentManager> weak_method_factory_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DOCUMENT_MANAGER_H_
