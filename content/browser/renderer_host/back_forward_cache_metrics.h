// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_

#include <bitset>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom-forward.h"

namespace base {
class TickClock;
}

namespace url {
class Origin;
}

namespace content {
class BackForwardCacheCanStoreDocumentResult;
class BackForwardCacheCanStoreTreeResult;
class NavigationEntryImpl;
class NavigationRequest;
class RenderFrameHostImpl;
struct BackForwardCacheCanStoreDocumentResultWithTree;

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
    kMinValue = 0,
    kNotPrimaryMainFrame = 0,
    // BackForwardCache is disabled due to low memory device, base::Feature or
    // command line. Note that the more specific NotRestoredReasons
    // kBackForwardCacheDisabledByLowMemory and
    // kBackForwardCacheDisabledByCommandLine will also be set as other reasons
    // along with this when appropriate.
    kBackForwardCacheDisabled = 1,
    kRelatedActiveContentsExist = 2,
    kHTTPStatusNotOK = 3,
    kSchemeNotHTTPOrHTTPS = 4,
    // DOMContentLoaded event has not yet fired. This means that deferred
    // scripts have not run yet and pagehide/pageshow event handlers may not be
    // installed yet.
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
    // 18: GrantedMediaStreamAccess is no longer blocking.
    // 19: kSchedulerTrackedFeatureUsed is no longer used.
    kConflictingBrowsingInstance = 20,
    kCacheFlushed = 21,
    kServiceWorkerVersionActivation = 22,
    kSessionRestored = 23,
    kUnknown = 24,
    kServiceWorkerPostMessage = 25,
    kEnteredBackForwardCacheBeforeServiceWorkerHostAdded = 26,
    // 27: kRenderFrameHostReused_SameSite was removed.
    // 28: kRenderFrameHostReused_CrossSite was removed.
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
    // 36: kFrameTreeNodeStateReset was removed.
    // 37: kNetworkRequestDatapipeDrained = 37 was removed and broken into 43
    // and 44.
    kNetworkRequestRedirected = 38,
    kNetworkRequestTimeout = 39,
    kNetworkExceedsBufferLimit = 40,
    kNavigationCancelledWhileRestoring = 41,
    // 42: kBackForwardCacheDisabledForPrerender was removed and merged into 0.
    kUserAgentOverrideDiffers = 43,
    // 44: kNetworkRequestDatapipeDrainedAsDatapipe was removed now that
    // ScriptStreamer is supported.
    kNetworkRequestDatapipeDrainedAsBytesConsumer = 45,
    kForegroundCacheLimit = 46,
    kBrowsingInstanceNotSwapped = 47,
    kBackForwardCacheDisabledForDelegate = 48,
    // 49: kOptInUnloadHeaderNotPresent was removed as the experiments ended.
    kUnloadHandlerExistsInMainFrame = 50,
    kUnloadHandlerExistsInSubFrame = 51,
    kServiceWorkerUnregistration = 52,
    kCacheControlNoStore = 53,
    kCacheControlNoStoreCookieModified = 54,
    kCacheControlNoStoreHTTPOnlyCookieModified = 55,
    kNoResponseHead = 56,
    // 57: kActivationNavigationsDisallowedForBug1234857 was fixed.
    kErrorDocument = 58,
    kFencedFramesEmbedder = 59,
    kMaxValue = kFencedFramesEmbedder,
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

  // Gets the metrics object for a committed navigation.
  // Note that this object will not be used if the entry we are navigating to
  // already has the BackForwardCacheMetrics object (which happens for history
  // navigations). We will reuse `previous_entry`'s metrics object if the
  // navigation is a subframe navigation or if it's same-document with
  // `previous_entry`'s document.
  //
  // |document_sequence_number| is the sequence number of the document
  // associated with the navigating frame.
  static scoped_refptr<BackForwardCacheMetrics>
  CreateOrReuseBackForwardCacheMetricsForNavigation(
      NavigationEntryImpl* previous_entry,
      bool is_main_frame_navigation,
      int64_t committing_document_sequence_number);

  explicit BackForwardCacheMetrics(int64_t document_sequence_number);

  BackForwardCacheMetrics(const BackForwardCacheMetrics&) = delete;
  BackForwardCacheMetrics& operator=(const BackForwardCacheMetrics&) = delete;

  // Records when the page is evicted after the document is restored e.g. when
  // the race condition by JavaScript happens.
  static void RecordEvictedAfterDocumentRestored(
      EvictedAfterDocumentRestoredReason reason);

  // Sets the reason why the browsing instance is swapped/not swapped. Passing
  // absl::nullopt resets the reason.
  void SetBrowsingInstanceSwapResult(
      absl::optional<ShouldSwapBrowsingInstance> reason);

  absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result()
      const {
    return browsing_instance_swap_result_;
  }

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
  void MainFrameDidNavigateAwayFromDocument();

  // Snapshots the state of the features active on the page before closing it.
  // It should be called at the same time when the document might have been
  // placed in the back-forward cache.
  void RecordFeatureUsage(RenderFrameHostImpl* main_frame);

  // Adds the flattened list of NotRestoredReasons to the existing
  // |page_store_result_|.
  // TODO(yuzus): Make this function take
  // BackForwardCacheCanStoreDocumentResultWithTree.
  void AddNotRestoredFlattenedReasonsToExistingResult(
      BackForwardCacheCanStoreDocumentResult& flattened);

  // Sets |can_store| as the final NotRestoredReasons to report. This replaces
  // the existing |page_store_tree_result_|.
  void SetNotRestoredReasons(
      BackForwardCacheCanStoreDocumentResultWithTree& can_store);

  // Populate and return the mojom struct from |page_store_tree_result_|.
  blink::mojom::BackForwardCacheNotRestoredReasonsPtr
  GetWebExposedNotRestoredReasons();

  // Records additional reasons why a history navigation was not served from
  // BFCache. The reasons are recorded only after the history navigation started
  // because it's about the history navigation (e.g. kSessionRestored) or
  // reasons that might not have been recorded yet (e.g.
  // kBrowsingInstanceNotSwapped).
  void UpdateNotRestoredReasonsForNavigation(NavigationRequest* navigation);

  // Exported for testing.
  // The DisabledReason's source and id combined to give a unique uint64.
  CONTENT_EXPORT static uint64_t MetricValue(BackForwardCache::DisabledReason);

  // Injects a clock for mocking time.
  // Should be called only from the UI thread.
  CONTENT_EXPORT static void OverrideTimeForTesting(base::TickClock* clock);

  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    // Report the tree result of NotRestoredReason to the observer.
    virtual void NotifyNotRestoredReasons(
        std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_result) = 0;
  };

  void SetObserverForTesting(TestObserver* observer) {
    test_observer_ = observer;
  }

  // Returns if |navigation| is cross-document main frame history navigation.
  static bool IsCrossDocumentMainFrameHistoryNavigation(
      NavigationRequest* navigation);

 private:
  friend class base::RefCounted<BackForwardCacheMetrics>;

  ~BackForwardCacheMetrics();

  // Recursively collects the feature usage information from the subtree
  // of a given frame.
  void CollectFeatureUsageFromSubtree(RenderFrameHostImpl* rfh,
                                      const url::Origin& main_frame_origin);

  // Dumps the current recorded information for a history navigation for UMA.
  // |back_forward_cache_allowed| indicates whether back-forward cache is
  // allowed for the URL of |navigation_request|.
  void RecordHistoryNavigationUMA(NavigationRequest* navigation,
                                  bool back_forward_cache_allowed) const;
  // Records UKM for a history navigation.
  void RecordHistoryNavigationUKM(NavigationRequest* navigation);

  // Record metrics for the number of reloads after history navigation. In
  // particular we are interested in number of reloads after a restore from
  // the back-forward cache as a proxy for detecting whether the page was
  // broken or not.
  void RecordHistogramForReloadsAfterHistoryNavigations(
      bool is_reload,
      bool back_forward_cache_allowed) const;

  // Whether the last navigation swapped BrowsingInstance or not. Returns true
  // if the last navigation did swap BrowsingInstance, or if it's unknown
  // (`browsing_instance_swap_result_` is not set).  Returns false otherwise.
  bool DidSwapBrowsingInstance() const;

  // Main frame document sequence number that identifies all
  // NavigationEntries this metrics object is associated with.
  const int64_t document_sequence_number_;

  // NavigationHandle's ID for the last cross-document main frame navigation
  // that uses this metrics object.
  //
  // Should not be confused with NavigationEntryId.
  int64_t last_committed_cross_document_main_frame_navigation_id_ = -1;

  // These values are updated only for cross-document main frame navigations.
  bool previous_navigation_is_history_ = false;
  bool previous_navigation_is_served_from_bfcache_ = false;

  // ====== Post-navigation reuse boundary ========
  // The variables above these are kept after we finished
  // logging the metrics for the last navigation that used this metrics object,
  // as they are needed for logging metrics for future navigations.
  // The variables below are reset after logging.

  blink::scheduler::WebSchedulerTrackedFeatures main_frame_features_;
  // We record metrics for same-origin frames and cross-origin frames
  // differently as we might want to apply different policies for them,
  // especially for the things around web platform compatibility (e.g. ignore
  // unload handlers in cross-origin iframes but not in same-origin). The
  // details are still subject to metrics, however. NOTE: This is not related to
  // which process these frames are hosted in.
  blink::scheduler::WebSchedulerTrackedFeatures same_origin_frames_features_;
  blink::scheduler::WebSchedulerTrackedFeatures cross_origin_frames_features_;

  absl::optional<base::TimeTicks> started_navigation_timestamp_;
  absl::optional<base::TimeTicks> navigated_away_from_main_document_timestamp_;
  absl::optional<base::TimeTicks> renderer_killed_timestamp_;

  // TODO: Store BackForwardCacheCanStoreDocumentResultWithTree instead of
  // storing unique_ptr of BackForwardCacheCanStoreDocumentResult and
  // BackForwardCacheCanStoreTreeResult respectively.
  std::unique_ptr<BackForwardCacheCanStoreDocumentResult> page_store_result_;
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> page_store_tree_result_;

  // The reason why the last attempted navigation in the main frame used or
  // didn't use a new BrowsingInstance.
  absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result_;

  raw_ptr<TestObserver> test_observer_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_
