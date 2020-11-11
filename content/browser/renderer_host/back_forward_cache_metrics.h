// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_

#include <bitset>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {
class BackForwardCacheCanStoreDocumentResult;
class NavigationEntryImpl;
class NavigationRequest;
class RenderFrameHostImpl;
struct LoadCommittedDetails;

// Helper class for recording metrics around history navigations.
// Associated with a main frame document and shared between all
// NavigationEntries with the same document_sequence_number for the main
// document.
//
// TODO(altimin, crbug.com/933147): Remove this class after we are done
// with implementing back-forward cache.
class BackForwardCacheMetrics
    : public base::RefCounted<BackForwardCacheMetrics> {
 public:
  // Please keep in sync with BackForwardCacheNotRestoredReason in
  // tools/metrics/histograms/enums.xml. These values should not be renumbered.
  enum class NotRestoredReason : uint8_t {
    kNotMainFrame = 0,
    // BackForwardCache is disabled due to low memory device, base::Feature or
    // command line. Note that the more specific NotRestoredReasons
    // kBackForwardCacheDisabledByLowMemory and
    // kBackForwardCacheDisabledByCommandLine will also be set as other reasons
    // along with this when appropriate.
    kBackForwardCacheDisabled = 1,
    kRelatedActiveContentsExist = 2,
    kHTTPStatusNotOK = 3,
    kSchemeNotHTTPOrHTTPS = 4,
    kLoading = 5,
    kWasGrantedMediaAccess = 6,
    kBlocklistedFeatures = 7,
    kDisableForRenderFrameHostCalled = 8,
    kDomainNotAllowed = 9,
    kHTTPMethodNotGET = 10,
    kSubframeIsNavigating = 11,
    kTimeout = 12,
    kCacheLimit = 13,
    kJavaScriptExecution = 14,
    kRendererProcessKilled = 15,
    kRendererProcessCrashed = 16,
    // 17: Dialogs are no longer a reason to exclude from BackForwardCache
    kGrantedMediaStreamAccess = 18,
    kSchedulerTrackedFeatureUsed = 19,
    kConflictingBrowsingInstance = 20,
    kCacheFlushed = 21,
    kServiceWorkerVersionActivation = 22,
    kSessionRestored = 23,
    kUnknown = 24,
    kServiceWorkerPostMessage = 25,
    kEnteredBackForwardCacheBeforeServiceWorkerHostAdded = 26,
    kRenderFrameHostReused_SameSite = 27,
    kRenderFrameHostReused_CrossSite = 28,
    kNotMostRecentNavigationEntry = 29,
    kServiceWorkerClaim = 30,
    kIgnoreEventAndEvict = 31,
    kHaveInnerContents = 32,
    kTimeoutPuttingInCache = 33,
    // BackForwardCache is disabled due to low memory device.
    kBackForwardCacheDisabledByLowMemory = 34,
    // BackForwardCache is disabled due to command-line switch (may include
    // cases where the embedder disabled it due to, e.g., enterprise policy).
    kBackForwardCacheDisabledByCommandLine = 35,
    kFrameTreeNodeStateReset = 36,
    kMaxValue = kFrameTreeNodeStateReset,
  };

  using NotRestoredReasons =
      std::bitset<static_cast<size_t>(NotRestoredReason::kMaxValue) + 1ul>;

  // Please keep in sync with BackForwardCacheHistoryNavigationOutcome in
  // tools/metrics/histograms/enums.xml. These values should not be renumbered.
  enum class HistoryNavigationOutcome {
    kRestored = 0,
    kNotRestored = 1,
    kMaxValue = kNotRestored,
  };

  // Please keep in sync with BackForwardCacheEvictedAfterDocumentRestoredReason
  // in tools/metrics/histograms/enums.xml. These values should not be
  // renumbered.
  enum class EvictedAfterDocumentRestoredReason {
    kRestored = 0,
    kByJavaScript = 1,
    kMaxValue = kByJavaScript,
  };

  // Please keep in sync with BackForwardCacheReloadsAndHistoryNavigations
  // in tools/metrics/histograms/enums.xml. These values should not be
  // renumbered.
  enum class ReloadsAndHistoryNavigations {
    kHistoryNavigation = 0,
    kReloadAfterHistoryNavigation = 1,
    kMaxValue = kReloadAfterHistoryNavigation,
  };

  // Please keep in sync with BackForwardCacheReloadsAfterHistoryNavigation
  // in tools/metrics/histograms/enums.xml. These values should not be
  // renumbered.
  enum class ReloadsAfterHistoryNavigation {
    kNotServedFromBackForwardCache = 0,
    kServedFromBackForwardCache = 1,
    kMaxValue = kServedFromBackForwardCache,
  };

  // Creates a potential new metrics object for the navigation.
  // Note that this object will not be used if the entry we are navigating to
  // already has the BackForwardCacheMetrics object (which happens for history
  // navigations).
  //
  // |document_sequence_number| is the sequence number of the document
  // associated with the document associated with the navigating frame and it is
  // ignored if the navigating frame is not a main one.
  static scoped_refptr<BackForwardCacheMetrics>
  CreateOrReuseBackForwardCacheMetrics(
      NavigationEntryImpl* currently_committed_entry,
      bool is_main_frame_navigation,
      int64_t document_sequence_number);

  // Records when the page is evicted after the document is restored e.g. when
  // the race condition by JavaScript happens.
  static void RecordEvictedAfterDocumentRestored(
      EvictedAfterDocumentRestoredReason reason);

  // Notifies that the main frame has started a navigation to an entry
  // associated with |this|.
  //
  // This is the point in time that a back-forward cache hit could be shown to
  // the user.
  //
  // Note that in some cases (synchronous renderer-initiated navigations
  // which create navigation entry only when committed) this call might
  // be missing, but they should not matter for bfcache.
  void MainFrameDidStartNavigationToDocument();

  // Notifies that an associated entry has committed a navigation.
  // |back_forward_cache_allowed| indicates whether back-forward cache is
  // allowed for the URL of |navigation_request|.
  void DidCommitNavigation(NavigationRequest* navigation_request,
                           bool back_forward_cache_allowed);

  // Records when another navigation commits away from the most recent entry
  // associated with |this|.  This is the point in time that the previous
  // document could enter the back-forward cache.
  // |new_main_document| points to the newly committed RFH, which might or might
  // not be the same as the RFH for the old document.
  void MainFrameDidNavigateAwayFromDocument(
      RenderFrameHostImpl* new_main_document,
      LoadCommittedDetails* details,
      NavigationRequest* navigation);

  // Snapshots the state of the features active on the page before closing it.
  // It should be called at the same time when the document might have been
  // placed in the back-forward cache.
  void RecordFeatureUsage(RenderFrameHostImpl* main_frame);

  // Marks when the page is not cached, or evicted. This information is useful
  // e.g., to prioritize the tasks to improve cache-hit rate.
  void MarkNotRestoredWithReason(
      const BackForwardCacheCanStoreDocumentResult& can_store);

  // Injects a clock for mocking time.
  // Should be called only from the UI thread.
  CONTENT_EXPORT static void OverrideTimeForTesting(base::TickClock* clock);

 private:
  friend class base::RefCounted<BackForwardCacheMetrics>;

  explicit BackForwardCacheMetrics(int64_t document_sequence_number);

  ~BackForwardCacheMetrics();

  // Recursively collects the feature usage information from the subtree
  // of a given frame.
  void CollectFeatureUsageFromSubtree(RenderFrameHostImpl* rfh,
                                      const url::Origin& main_frame_origin);

  // Dumps the current recorded information.
  // |back_forward_cache_allowed| indicates whether back-forward cache is
  // allowed for the URL of |navigation_request|.
  void RecordMetricsForHistoryNavigationCommit(
      NavigationRequest* navigation,
      bool back_forward_cache_allowed) const;

  // Record metrics for the number of reloads after history navigation. In
  // particular we are interested in number of reloads after a restore from
  // the back-forward cache as a proxy for detecting whether the page was
  // broken or not.
  void RecordHistogramForReloadsAndHistoryNavigations(
      bool is_reload,
      bool back_forward_cache_allowed) const;

  // Record additional reason why navigation was not served from bfcache which
  // are known only at the commit time.
  void UpdateNotRestoredReasonsForNavigation(NavigationRequest* navigation);

  bool ShouldRecordBrowsingInstanceNotSwappedReason() const;

  void RecordHistoryNavigationUkm(NavigationRequest* navigation);

  // Main frame document sequence number that identifies all NavigationEntries
  // this metrics object is associated with.
  const int64_t document_sequence_number_;

  // NavigationHandle's ID for the last main frame navigation. This is updated
  // for a main frame, not-same-document navigation.
  //
  // Should not be confused with NavigationEntryId.
  int64_t last_committed_cross_document_main_frame_navigation_id_ = -1;

  int64_t last_committed_navigation_entry_id_ = -1;

  uint64_t main_frame_features_ = 0;
  // We record metrics for same-origin frames and cross-origin frames
  // differently as we might want to apply different policies for them,
  // especially for the things around web platform compatibility (e.g. ignore
  // unload handlers in cross-origin iframes but not in same-origin). The
  // details are still subject to metrics, however. NOTE: This is not related to
  // which process these frames are hosted in.
  uint64_t same_origin_frames_features_ = 0;
  uint64_t cross_origin_frames_features_ = 0;

  base::Optional<base::TimeTicks> started_navigation_timestamp_;
  base::Optional<base::TimeTicks> navigated_away_from_main_document_timestamp_;

  NotRestoredReasons not_restored_reasons_;
  uint64_t blocklisted_features_ = 0;
  base::Optional<ShouldSwapBrowsingInstance>
      browsing_instance_not_swapped_reason_;

  // The reasons given at BackForwardCache::DisableForRenderFrameHost. These are
  // a further breakdown of NotRestoredReason::kDisableForRenderFrameHostCalled.
  std::set<std::string> disabled_reasons_;

  // This value is updated only for navigations which are not same-document and
  // main-frame navigations.
  bool previous_navigation_is_history_ = false;
  bool previous_navigation_is_served_from_bfcache_ = false;

  base::Optional<base::TimeTicks> renderer_killed_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardCacheMetrics);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_
