// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_

#include <list>
#include <memory>
#include <set>
#include <unordered_set>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_process_host_internal_observer.h"
#include "content/browser/renderer_host/stored_page.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;
class SiteInstance;

// This feature is used to limit the scope of back-forward cache experiment
// without enabling it. To control the URLs list by using this feature by
// generating the metrics only for "allowed_websites" param. Mainly, to ensure
// that metrics from the control and experiment groups are consistent.
BASE_FEATURE(kRecordBackForwardCacheMetricsWithoutEnabling,
             "RecordBackForwardCacheMetricsWithoutEnabling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Removes the time limit for cached content. This is used on bots to identify
// accidentally passing tests.
BASE_FEATURE(kBackForwardCacheNoTimeEviction,
             "BackForwardCacheNoTimeEviction",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature to allow exposing cross-origin subframes' NotRestoredReasons.
BASE_FEATURE(kAllowCrossOriginNotRestoredReasons,
             "AllowCrossOriginNotRestoredReasons",
             base::FEATURE_DISABLED_BY_DEFAULT);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kBackForwardCacheSize);
CONTENT_EXPORT extern const base::FeatureParam<int>
    kBackForwardCacheSizeCacheSize;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kBackForwardCacheSizeForegroundCacheSize;

// Controls the interaction between back/forward cache and
// unload. When enabled, pages with unload handlers may enter the
// cache.
BASE_FEATURE(kBackForwardCacheUnloadAllowed,
             "BackForwardCacheUnloadAllowed",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Combines a flattened list and a tree of the reasons why each document cannot
// enter the back/forward cache (might be empty if it can). The tree saves the
// reasons for each document in the tree (including those without the reasons)
// in a tree format, with each node corresponding to one document. The flattened
// list is the combination of all reasons for all documents in the tree.
// CONTENT_EXPORT is for exporting only for testing.
struct CONTENT_EXPORT BackForwardCacheCanStoreDocumentResultWithTree {
  BackForwardCacheCanStoreDocumentResultWithTree(
      BackForwardCacheCanStoreDocumentResult& flattened_reasons,
      std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_reasons);
  BackForwardCacheCanStoreDocumentResultWithTree(
      BackForwardCacheCanStoreDocumentResultWithTree&& other);
  ~BackForwardCacheCanStoreDocumentResultWithTree();

  BackForwardCacheCanStoreDocumentResult flattened_reasons;
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_reasons;

  // The conditions for storing and restoring the pages are different in that
  // pages with cache-control:no-store can enter back/forward cache depending on
  // the experiment flag, but can never be restored.
  bool CanStore() const { return flattened_reasons.CanStore(); }
  bool CanRestore() const { return flattened_reasons.CanRestore(); }
};

// BackForwardCache:
//
// After the user navigates away from a document, the old one goes into the
// frozen state and is kept in this object. They can potentially be reused
// after an history navigation. Reusing a document means swapping it back with
// the current_frame_host.
//
//
// BackForwardCache Size & Pruning Logic:
//
// 1. `EnforceCacheSizeLimit()` is called to prune the cache size down on
//    storing a new cache entry, or when the renderer process's
//    `GetPriority()` state changes.
//    A. [Android-only] The number of entries where `HasForegroundedProcess()`
//       is true is pruned to `GetForegroundedEntriesCacheSize()`.
//    B. Prunes to `GetCacheSize()` entries no matter what kinds of tabs
//       BackForwardCache is in.
//    C. When a `RenderFrameHost` enters BackForwardCache, it schedules a task
//       in `RenderFrameHostImpl::StartBackForwardCacheEvictionTimer()` to
//       evicts the outermost frame after
//       `kDefaultTimeToLiveInBackForwardCacheInSeconds` seconds.
// 2. In `performance_manager::policies::BFCachePolicy`:
//    A. [Desktop-only] On moderate memory pressure, the number of entries in a
//       visible tab's cache is pruned to
//       `ForegroundCacheSizeOnModeratePressure()`. The number in a non-visible
//       tab is pruned to `BackgroundCacheSizeOnModeratePressure()`.
//    B. [Desktop-only] On critical memory pressure, the cache is cleared.
class CONTENT_EXPORT BackForwardCacheImpl
    : public BackForwardCache,
      public RenderProcessHostInternalObserver,
      public StoredPage::Delegate {
  friend class BackForwardCacheCanStoreTreeResult;
  friend class BackForwardCacheMetrics;

 public:
  enum MessageHandlingPolicyWhenCached {
    kMessagePolicyNone,
    kMessagePolicyLog,
    kMessagePolicyDump,
  };

  static MessageHandlingPolicyWhenCached
  GetChannelAssociatedMessageHandlingPolicy();

  // BackForwardCache entry, consisting of the page and associated metadata.
  class Entry {
   public:
    explicit Entry(std::unique_ptr<StoredPage> stored_page);
    ~Entry();

    void WriteIntoTrace(perfetto::TracedValue context);

    // Indicates whether or not all the |render_view_hosts| in this entry have
    // received the acknowledgement from renderer that it finished running
    // handlers.
    bool AllRenderViewHostsReceivedAckFromRenderer();

    std::unique_ptr<StoredPage> TakeStoredPage() {
      return std::move(stored_page_);
    }
    void SetPageRestoreParams(
        blink::mojom::PageRestoreParamsPtr page_restore_params) {
      stored_page_->SetPageRestoreParams(std::move(page_restore_params));
    }

    void SetStoredPageDelegate(StoredPage::Delegate* delegate) {
      stored_page_->SetDelegate(delegate);
    }

    void SetViewTransitionState(
        std::optional<blink::ViewTransitionState> view_transition_state) {
      stored_page_->SetViewTransitionState(std::move(view_transition_state));
    }

    // The main document being stored.
    RenderFrameHostImpl* render_frame_host() {
      return stored_page_->render_frame_host();
    }

    const StoredPage::RenderViewHostImplSafeRefSet& render_view_hosts() {
      return stored_page_->render_view_hosts();
    }

    const StoredPage::RenderFrameProxyHostMap& proxy_hosts() const {
      return stored_page_->proxy_hosts();
    }

    size_t proxy_hosts_size() { return stored_page_->proxy_hosts_size(); }

   private:
    friend class BackForwardCacheImpl;

    std::unique_ptr<StoredPage> stored_page_;
  };

  explicit BackForwardCacheImpl(BrowserContext* browser_context);

  BackForwardCacheImpl(const BackForwardCacheImpl&) = delete;
  BackForwardCacheImpl& operator=(const BackForwardCacheImpl&) = delete;

  ~BackForwardCacheImpl() override;

  // Returns whether MediaSession's service is allowed for the BackForwardCache.
  static bool IsMediaSessionServiceAllowed();

  // Returns whether back/forward cache is enabled for screen reader users.
  static bool IsScreenReaderAllowed();

  // Returns where back/forward cache is allowed for pages with unload handlers.
  static bool IsUnloadAllowed();

  // Log an unexpected message from the renderer. Doing it here so that it is
  // grouped with other back/forward cache vlogging and e.g. will show up in
  // test logs. `message_name` varies in each build however when a test failure
  // occurs, it should be possible to recreate the build and find which message
  // corresponds to this the value.
  static void VlogUnexpectedRendererToBrowserMessage(
      const char* interface_name_,
      uint32_t message_name,
      RenderFrameHostImpl* rfh);

  // Returns the reasons (if any) why this document and its children cannot
  // enter the back/forward cache. Depends on the |render_frame_host| and its
  // children's state. Should only be called after we've navigated away from
  // |render_frame_host|, which means nothing about the page can change (usage
  // of blocklisted features, pending navigations, load state, etc.) anymore.
  // Note that criteria for storing and restoring can be different, i.e.
  // |CanStore()| and |CanRestore()| might give different results.
  // Note that the returned result will not include non-sticky features if the
  // browser has not received an IPC ACK from the renderer. See also the
  // comments for |RequestedFeatures|. If you always want to include non-sticky
  // features, use GetCompleteBackForwardCacheEligibilityForReporting() instead.
  BackForwardCacheCanStoreDocumentResultWithTree
  GetCurrentBackForwardCacheEligibility(RenderFrameHostImpl* render_frame_host);

  // Whether a RenderFrameHost could be stored into the BackForwardCache at some
  // point in the future. Different than GetCurrentBackForwardCacheEligibility()
  // above and GetCompleteBackForwardCacheEligibilityForReporting() below, we
  // won't check for properties of |render_frame_host| that might change in the
  // future such as usage of certain APIs (non-sticky features), loading state,
  // existence of pending navigation requests, etc. This should be treated as a
  // "best guess" on whether a page still has a chance to be stored in the
  // back-forward cache later on, and should not be used as a final check before
  // storing a page to the back-forward cache (for that, use
  // GetCurrentBackForwardCacheEligibility() instead).
  BackForwardCacheCanStoreDocumentResultWithTree
  GetFutureBackForwardCacheEligibilityPotential(
      RenderFrameHostImpl* render_frame_host);

  // This will return all the reasons present at the point of calling that could
  // block back/forward cache, including both sticky and non-sticky features,
  // regardless of the IPC ack status (unlike
  // GetCurrentBackForwardCacheEligibility()). Note that non-sticky features
  // might get cleaned in pagehide handlers and might not block back/forward
  // cache, and this result will include them anyway.
  BackForwardCacheCanStoreDocumentResultWithTree
  GetCompleteBackForwardCacheEligibilityForReporting(
      RenderFrameHostImpl* render_frame_host);

  // Moves the specified BackForwardCache entry into the BackForwardCache. It
  // can be reused in a future history navigation by using RestoreEntry(). When
  // the BackForwardCache is full, the least recently used document is evicted.
  // Precondition: CanStoreDocument(*(entry->render_frame_host)).
  void StoreEntry(std::unique_ptr<Entry> entry);

  // Ensures that the cache is within its size limits. This should be called
  // whenever events occur that could put the cache outside its limits. What
  // those events are depends on the cache limit policy.
  void EnforceCacheSizeLimit();

  enum GetEntryFailureCase {
    // No matched BFCache entry is found.
    kEntryNotFound,
    // BFCache entry is found, but it was evicted before the `GetOrEvictEntry()`
    // call.
    kEntryEvictedBefore,
    // BFCache entry is found and not evicted, but it's no longer eligible for
    // BFCache, and gets evicted in the `GetOrEvictEntry()` call.
    kEntryIneligibleAndEvicted,
  };

  // Returns a pointer to a cached BackForwardCache entry matching
  // `navigation_entry_id`.
  // Returns nullptr if no matching entry is found or if the entry is evicted.
  // If the returned entry is null, this method will also return a
  // `BackForwardCacheImpl::GetEntryResult`, which contains information about
  // whether it's because a matching entry was found or the entry was evicted.

  // Note: The returned pointer should be used temporarily only within the
  // execution of a single task on the event loop. Beyond that, there is no
  // guarantee the pointer will be valid, because the document may be
  // removed/evicted from the cache.

  // WARNING: Calling this method may result in the eviction of the BFCache
  // entry if it is no longer eligible for the BFCache but has not been evicted
  // yet. If the eviction is triggered while there is an ongoing BFCache
  // restore, the caller must discard the NavigationRequest that is about to
  // commit the restore, otherwise the NavigationRequest may try to access the
  // RenderFrameHost after it has been deleted.
  base::expected<BackForwardCacheImpl::Entry*,
                 BackForwardCacheImpl::GetEntryFailureCase>
  GetOrEvictEntry(int navigation_entry_id);

  // During a history navigation, moves an entry out of the BackForwardCache
  // knowing its |navigation_entry_id|. |page_restore_params| includes
  // information that is needed by the entry's page after getting restored,
  // which includes the latest history information (offset, length) and the
  // timestamp corresponding to the start of the back-forward cached navigation,
  // which would be communicated to the page to allow it to record the latency
  // of this navigation.
  std::unique_ptr<Entry> RestoreEntry(
      int navigation_entry_id,
      blink::mojom::PageRestoreParamsPtr page_restore_params);

  // Evict all cached pages in the same BrowsingInstance as
  // |site_instance|.
  void EvictFramesInRelatedSiteInstances(SiteInstance* site_instance);

  // Immediately deletes all frames in the cache. This should only be called
  // when WebContents is being destroyed.
  void Shutdown();

  // Posts a task to destroy all frames in the BackForwardCache that have been
  // marked as evicted.
  void PostTaskToDestroyEvictedFrames();

  // This enum indicates if the method is called from a "Cache-Control:
  // no-store" context, i.e. the page's same-origin main document has
  // "Cache-Control: no-store" header.
  enum CacheControlNoStoreContext {
    kInCCNSContext,
    kNotInCCNSContext,
  };

  // Storing frames in back-forward cache is not supported indefinitely
  // due to potential privacy issues and memory leaks. Instead we are evicting
  // the frame from the cache after the time to live, which can be controlled
  // via experiment.
  // The return value may vary depending on if the main frame of the cached page
  // has "Cache-Control: no-store" header.
  static base::TimeDelta GetTimeToLiveInBackForwardCache(
      CacheControlNoStoreContext ccns_context);

  // Gets the maximum number of entries the BackForwardCache can hold per tab.
  static size_t GetCacheSize();

  // The back-forward cache is experimented on a limited set of URLs. This
  // method returns true if the |url| matches one of those. URL not matching
  // this won't enter the back-forward cache. This can still return true even
  // when BackForwardCache is disabled for metrics purposes. It checks
  // |IsHostPathAllowed| then |IsHostPathAllowed|
  bool IsAllowed(const GURL& current_url);
  // Returns true if the host and path are allowed according to the
  // "allowed_websites" and "blocked_webites" parameters of
  // |feature::kBackForwardCache|. An empty "allowed_websites" implies that all
  // websites are allowed.
  bool IsHostPathAllowed(const GURL& current_url);
  // Returns true if query does not contain any of the parameters in
  // "blocked_cgi_params" parameter of |feature::kBackForwardCache|. The
  // comparison is done by splitting the query string on "&" and looking for
  // exact matches in the list (parameter name and value). It does not consider
  // URL escaping.
  bool IsQueryAllowed(const GURL& current_url);

  // Called just before commit for a navigation that's served out of the back
  // forward cache. This method will disable eviction in renderers and invoke
  // |done_callback| when they are ready for the navigation to be committed.
  void WillCommitNavigationToCachedEntry(Entry& bfcache_entry,
                                         base::OnceClosure done_callback);

  // Returns the task runner that should be used by the eviction timer.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_for_testing_
               ? task_runner_for_testing_
               : base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  // Inject task runner for precise timing control in browser tests.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    task_runner_for_testing_ = task_runner;
  }

  const std::list<std::unique_ptr<Entry>>& GetEntries();
  std::list<Entry*> GetEntriesForRenderViewHostImpl(
      const RenderViewHostImpl* rvhi) const;

  // BackForwardCache overrides:
  void Flush() override;
  void Flush(NotRestoredReason reason) override;
  void Prune(size_t limit) override;
  void DisableForTesting(DisableForTestingReason reason) override;

  // Evict all entries from the BackForwardCache that match the removal filter.
  void Flush(
      const StoragePartition::StorageKeyMatcherFunction& storage_key_filter);

  // Evict all entries from the BackForwardCache that were loaded with
  // "Cache-Control: no-store" header and match the removal filter.
  void FlushCacheControlNoStoreEntries(
      const StoragePartition::StorageKeyMatcherFunction& storage_key_filter);

  // RenderProcessHostInternalObserver methods
  void RenderProcessPriorityChanged(RenderProcessHostImpl* host) override;

  // Returns true if we are managing the cache size using foreground and
  // background limits (if finch parameter "foreground_cache_size" > 0).
  static bool UsingForegroundBackgroundCacheSizeLimit();

  // Returns true if one of the BFCache entries has a matching
  // RFH/RFPH/RVH with the same SIG ID/RVH ID.
  // TODO(crbug.com/354382462): Remove these once the bug is fixed.
  bool IsRenderFrameHostWithSIGInBackForwardCacheForDebugging(
      SiteInstanceGroupId site_instance_group_id);
  bool IsRenderFrameProxyHostWithSIGInBackForwardCacheForDebugging(
      SiteInstanceGroupId site_instance_group_id);
  bool IsRenderViewHostWithMapIdInBackForwardCacheForDebugging(
      const RenderViewHostImpl& rvh);

  bool IsRelatedSiteInstanceInBackForwardCacheForDebugging(
      SiteInstance& site_instance);

  // StoredPage::Delegate overrides:
  void RenderViewHostNoLongerStored(RenderViewHostImpl* rvh) override;

  // Construct a tree of NotRestoredReasons for |rfh| without checking the
  // eligibility of all the documents in the frame tree. This should be only
  // used for evicting the back/forward cache entry where we know why the entry
  // is not eligible and which document is causing it.
  // This preserves the frame tree structure after eviction, because the actual
  // page and frame tree is not kept around after eviction.
  // |rfh| will be marked as having |eviction_reason| as not restored reasons.
  static BackForwardCacheCanStoreDocumentResultWithTree
  CreateEvictionBackForwardCacheCanStoreTreeResult(
      RenderFrameHostImpl& rfh,
      BackForwardCacheCanStoreDocumentResult& eviction_reason);

  bool should_allow_storing_pages_with_cache_control_no_store() {
    return should_allow_storing_pages_with_cache_control_no_store_;
  }

  // Returns true if there is a BFCached entry that sufficiently matches the
  // navigation that just committed in `committing_rfh` with initiator origin
  // `initiator_origin`, such that the entry could have been used (the URL,
  // origin, initiator origin, and security properties are the same, and if
  // `require_no_subframes` is used, has no subframes). This is
  // called in response to new non-reload/session-restore cross-document
  // navigation commits.
  bool HasPotentiallyMatchingEntry(
      const RenderFrameHostImpl& committing_rfh,
      const std::optional<url::Origin>& initiator_origin,
      bool require_no_subframes) const;

 private:
  // Destroys all evicted frames in the BackForwardCache.
  void DestroyEvictedFrames();

  // Populates the reasons that are only relevant for main documents such as
  // browser settings, the main document's URL & HTTP status, etc.
  void PopulateReasonsForMainDocument(
      BackForwardCacheCanStoreDocumentResult& result,
      RenderFrameHostImpl* render_frame_host);

  // This enum indicates what features to include when recording
  // NotRestoredReasons.
  enum RequestedFeatures {
    // Report only sticky reasons.
    kOnlySticky,
    // If the entry has received an IPC ack from the renderer, report all the
    // reasons. Otherwise only include sticky features, because non-sticky
    // features might be removed by the renderer when the browser signals it is
    // about to put the page into BFCache.
    kAllIfAcked,
    // Regardless of the ack status, report all the reasons.
    kAll,
  };

  // Populates the reasons why this |rfh| and its subframes cannot enter the
  // back/forward cache in a flat list through |flattened_result| and as a tree
  // through its return value.
  // |requested_features| controls whether we include non-sticky reasons in the
  // result.
  BackForwardCacheCanStoreDocumentResultWithTree PopulateReasonsForPage(
      RenderFrameHostImpl* rfh,
      BackForwardCacheCanStoreDocumentResult& flattened_result,
      RequestedFeatures requested_features);

  // Updates the result to include CacheControlNoStore reasons if the flag is
  // on.
  void UpdateCanStoreToIncludeCacheControlNoStore(
      BackForwardCacheCanStoreDocumentResult& result,
      RenderFrameHostImpl* render_frame_host);

  // Return the matching entry which has |page|.
  BackForwardCacheImpl::Entry* FindMatchingEntry(PageImpl& page);

  void RenderViewHostNoLongerStoredInternal(RenderViewHostImpl* rvh);

  // If non-zero, the cache may contain at most this many entries with involving
  // foregrounded processes and the remaining space can only be used by entries
  // with no foregrounded processes. We can be less strict on memory usage of
  // background processes because Android will kill the process if memory
  // becomes scarce.
  static size_t GetForegroundedEntriesCacheSize();

  // Enforces a limit on the number of entries. Which entries are counted
  // towards the limit depends on the values of |foregrounded_only|. If it's
  // true it only considers entries that are associated with a foregrounded
  // process. Otherwise all entries are considered.
  size_t EnforceCacheSizeLimitInternal(size_t limit, bool foregrounded_only);

  // Updates |process_to_entry_map_| with processes from |entry|. These must
  // be called after adding or removing an entry in |entries_|.
  void AddProcessesForEntry(Entry& entry);
  void RemoveProcessesForEntry(Entry& entry);

  static BlockListedFeatures GetAllowedFeatures(
      RequestedFeatures requested_features,
      CacheControlNoStoreContext ccns_context);

  static BlockListedFeatures GetDisallowedFeatures(
      RequestedFeatures requested_features,
      CacheControlNoStoreContext ccns_context);

  // Contains the set of stored Entries.
  // Invariant:
  // - Ordered from the most recently used to the last recently used.
  // - Once the list is full, the least recently used document is evicted.
  std::list<std::unique_ptr<Entry>> entries_;

  // Keeps track of the observed RenderProcessHosts. This is populated
  // from and kept in sync with |entries_|. The RenderProcessHosts are collected
  // from each Entry's RenderViewHosts. Every RenderProcessHost in here is
  // observed by |this|. Every RenderProcessHost in this is referenced by a
  // RenderViewHost in the Entry and so will be valid.
  std::multiset<RenderProcessHost*> observed_processes_;

  // Whether the BackForwardCache has been enabled for pages loaded with
  // "Cache-Control: no-store" header.
  bool should_allow_storing_pages_with_cache_control_no_store_;

  // Only used in tests. Whether the BackforwardCache has been disabled for
  // testing.
  bool is_disabled_for_testing_ = false;

  // Only used for tests. This task runner is used for precise injection in
  // browser tests and for timing control.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing_;

  // To enter the back-forward cache, the main document URL's must match one of
  // the field trial parameter "allowed_websites". This is represented here by a
  // set of host and path prefix. When |allowed_urls_| is empty, it means there
  // are no restrictions on URLs.
  const base::flat_map<std::string,              // URL's host,
                       std::vector<std::string>  // URL's path prefix
                       >
      allowed_urls_;

  // This is an emergency kill switch per url to stop BFCache. The data will be
  // provided via the field trial parameter "blocked_websites".
  // "blocked_websites" have priority over "allowed_websites". This is
  // represented here by a set of host and path prefix.
  const base::flat_map<std::string,              // URL's host,
                       std::vector<std::string>  // URL's path prefix
                       >
      blocked_urls_;

  // Data provided from the "blocked_cgi_params" feature param. If any of these
  // occur in the query of the URL then the page is not eligible for caching.
  // See |IsQueryAllowed|.
  const base::flat_set<std::string> blocked_cgi_params_;

  // Helper class to iterate through the frame tree in the page and populate the
  // NotRestoredReasons.
  class NotRestoredReasonBuilder {
   public:
    // Construct a tree of NotRestoredReasons by checking the eligibility of
    // each frame in the frame tree rooted at |root_rfh|.
    // |root_rfh| represents the root document of the page. |include_non_sticky|
    // controls whether or not we should record non-sticky reasons in the tree.
    NotRestoredReasonBuilder(RenderFrameHostImpl* root_rfh,
                             RequestedFeatures requested_features);

    // Struct for containing the RenderFrameHostImpl that is going to be
    // evicted if applicable. |reasons| represent why |rfh_to_be_evicted| will
    // be evicted.
    struct EvictionInfo {
      EvictionInfo(RenderFrameHostImpl& rfh,
                   BackForwardCacheCanStoreDocumentResult* reasons)
          : rfh_to_be_evicted(&rfh), reasons(reasons) {}
      const raw_ptr<RenderFrameHostImpl> rfh_to_be_evicted;
      raw_ptr<const BackForwardCacheCanStoreDocumentResult> reasons;
    };

    NotRestoredReasonBuilder(RenderFrameHostImpl* root_rfh,
                             RequestedFeatures requested_features,
                             std::optional<EvictionInfo> eviction_info);

    ~NotRestoredReasonBuilder();

    // Access the populated result.
    BackForwardCacheCanStoreDocumentResult& GetFlattenedResult() {
      // TODO(yuzus): Check that |flattened_result_| and the tree result match.
      return flattened_result_;
    }

    std::unique_ptr<BackForwardCacheCanStoreTreeResult> GetTreeResult() {
      return std::move(tree_result_);
    }

    // Populates `result` with the blocking reasons for this document. If
    // "include_non_sticky" is true, it includes non-sticky reasons.
    void PopulateReasonsForDocument(
        BackForwardCacheCanStoreDocumentResult& result,
        RenderFrameHostImpl* rfh,
        RequestedFeatures requested_features);

    // Populates the sticky reasons for `rfh` without recursing into subframes.
    // Sticky features can't be unregistered and remain active for the rest of
    // the lifetime of the page.
    void PopulateStickyReasonsForDocument(
        BackForwardCacheCanStoreDocumentResult& result,
        RenderFrameHostImpl* rfh);

    // Populates the non-sticky reasons for `rfh` without recursing into
    // subframes. Non-sticky reasons mean the reasons that may be resolved later
    // such as when the page releases blocking resources in pagehide.
    void PopulateNonStickyReasonsForDocument(
        BackForwardCacheCanStoreDocumentResult& result,
        RenderFrameHostImpl* rfh,
        RequestedFeatures requested_features);

   private:
    // Populate NotRestoredReasons for the `rfh` by
    // iterating the frame tree and populating NotRestoredReasons in
    // |flattened_result_|.
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> PopulateReasons(
        RenderFrameHostImpl* rfh);

    // Root document of the tree.
    const raw_ptr<RenderFrameHostImpl> root_rfh_;
    // BackForwardCacheImpl instance to access eligibility check functions.
    const raw_ref<BackForwardCacheImpl> bfcache_;
    // Flattened list of NotRestoredReasons for the tree. This is empty at the
    // start and has to be merged using |GetFlattenedResult()|.
    BackForwardCacheCanStoreDocumentResult flattened_result_;
    // Tree result of NotRestoredReasons. This is populated in the constructor.
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_result_;
    // See |RequestedFeatures|.
    const RequestedFeatures requested_features_;
    // Contains the information of the RenderFrameHost that causes eviction, if
    // applicable. If set, the result returned by the builder will only contain
    // the NotRestoredReason for the RenderFrameHost that causes eviction
    // (instead of the reasons for the whole tree).
    std::optional<EvictionInfo> eviction_info_;
  };

  base::WeakPtrFactory<BackForwardCacheImpl> weak_factory_;

  // For testing:
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheMetricsTest, AllFeaturesCovered);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheActiveSizeTest, ActiveCacheSize);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheOverwriteSizeTest,
                           OverwrittenCacheSize);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheDefaultSizeTest, DefaultCacheSize);
};

// Allow external code to be notified when back-forward cache is disabled for a
// RenderFrameHost. This should be used only by the testing infrastructure which
// want to know the exact reason why the cache was disabled. There can be only
// one observer.
class CONTENT_EXPORT BackForwardCacheTestDelegate {
 public:
  BackForwardCacheTestDelegate();
  virtual ~BackForwardCacheTestDelegate();

  virtual void OnDisabledForFrameWithReason(
      GlobalRenderFrameHostId id,
      BackForwardCache::DisabledReason reason) = 0;
};

// Represents the reasons that a subtree cannot enter BFCache as a tree with a
// node for every document in the subtree, in frame tree order. It also includes
// documents that have no blocking reason.
class CONTENT_EXPORT BackForwardCacheCanStoreTreeResult {
 public:
  friend class BackForwardCacheImpl;

  using ChildrenVector =
      std::vector<std::unique_ptr<BackForwardCacheCanStoreTreeResult>>;

  BackForwardCacheCanStoreTreeResult() = delete;
  BackForwardCacheCanStoreTreeResult(BackForwardCacheCanStoreTreeResult&) =
      delete;
  BackForwardCacheCanStoreTreeResult& operator=(
      BackForwardCacheCanStoreTreeResult&&) = delete;
  ~BackForwardCacheCanStoreTreeResult();

  // Adds reasons of this subtree's root document to the tree result from
  // |BackForwardCacheCanStoreDocumentResult|.
  void AddReasonsToSubtreeRootFrom(
      const BackForwardCacheCanStoreDocumentResult& result);

  // The reasons for this subtree's root document.
  const BackForwardCacheCanStoreDocumentResult& GetDocumentResult() const {
    return document_result_;
  }

  // The blocking details map for this subtree's root document.
  const BackForwardCacheCanStoreDocumentResult::BlockingDetailsMap&
  GetBlockingDetailsMap() const {
    return document_result_.blocking_details_map();
  }

  // Populate NotRestoredReasons mojom struct based on the existing tree of
  // reason to report to the renderer.
  // This should be called only when the root document is outermost main
  // document.
  // We have access to attributes of cross-origin iframes that are children of
  // same-origin iframes. This method's purpose is to ensure that we only return
  // the information that should be exposed based on origin. (i.e. we only
  // include information iframes that are direct children of same-origin
  // frames).
  blink::mojom::BackForwardCacheNotRestoredReasonsPtr
  GetWebExposedNotRestoredReasons();

  // Flatten the tree and return a flattened list of not restored reasons that
  // includes all the reasons in the tree.
  const BackForwardCacheCanStoreDocumentResult FlattenTree();

  // The children nodes. We can access the children nodes of this
  // node/document from this vector.
  const ChildrenVector& GetChildren() const { return children_; }

  // Whether this subtree's root document's origin is the same origin with the
  // origin of the page's root document origin. Returns false if this document
  // is cross-origin.
  bool IsSameOrigin() const { return is_same_origin_; }

  // The URL of the document corresponding to this subtree's root document.
  const GURL& GetUrl() const { return url_; }

  // Creates and returns an empty tree.
  static std::unique_ptr<BackForwardCacheCanStoreTreeResult> CreateEmptyTree(
      RenderFrameHostImpl* rfh);
  static std::unique_ptr<BackForwardCacheCanStoreTreeResult>
  CreateEmptyTreeForNavigation(NavigationRequest* navigation);

 private:
  friend class BackForwardCacheImplTest;
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheImplTest,
                           CrossOriginReachableFrameCount);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheImplTest, CrossOriginAllMasked);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheImplTestExposeCrossOrigin,
                           FirstCrossOriginReachable);
  FRIEND_TEST_ALL_PREFIXES(BackForwardCacheImplTestExposeCrossOrigin,
                           SecondCrossOriginReachable);
  // This constructor is for creating a tree for |rfh| as the subtree's root
  // document's frame.
  BackForwardCacheCanStoreTreeResult(
      RenderFrameHostImpl* rfh,
      const url::Origin& main_document_origin,
      const GURL& url,
      BackForwardCacheCanStoreDocumentResult& result_for_this_document);

  // Creates an empty placeholder tree with the empty result.
  BackForwardCacheCanStoreTreeResult(bool is_same_origin, const GURL& url);

  // Helper function for |GetWebExposedNotRestoredReasons()|. |index| is the
  // random index of the cross-origin iframe that we decided to report
  // from all the reachable cross-origin iframes. We decrement this count
  // every time we call this function, and report only when |index| is 0 so
  // that reporting happens only for randomly picked one of such iframes.
  // TODO(crbug.com/41491384): Add "masked" when UA internal reasons such as
  // memory pressure and browsing instance not swapped are blocking as well.
  blink::mojom::BackForwardCacheNotRestoredReasonsPtr
  GetWebExposedNotRestoredReasonsInternal(int& index);

  // Returns if any cross-origin iframe in the tree is blocking and is not
  // a randomly selected iframe (i.e. does not have "masked" as its reason).
  // If this is true, we need to add "masked" to main frame's reasons.
  // |index| is the random index of the cross-origin iframe that we decided to
  // report from all the reachable cross-origin iframes.
  bool HasUnexposedCrossOriginBlockingIframe(int& index);

  // Count the number of cross-origin frames that are direct children of
  // same-origin frames, including the main frame, in the tree.
  uint32_t GetCrossOriginReachableFrameCount();

  void FlattenTreeHelper(
      BackForwardCacheCanStoreDocumentResult* document_result);

  void AppendChild(std::unique_ptr<BackForwardCacheCanStoreTreeResult> child);

  // See |GetDocumentResult|
  BackForwardCacheCanStoreDocumentResult document_result_;

  // See |GetChildren|
  ChildrenVector children_;

  // See |IsSameOrigin|
  const bool is_same_origin_;
  // Whether or not the root document of this tree is the outermoust main
  // frame's document.
  const bool is_root_outermost_main_frame_;
  // The id, name and src attribute of the frame owner of this subtree's root
  // document.
  const std::optional<std::string> id_;
  const std::optional<std::string> name_;
  const std::optional<std::string> src_;
  // See |GetUrl|
  const GURL url_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_
