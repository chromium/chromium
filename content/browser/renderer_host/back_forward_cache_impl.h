// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_

#include <list>
#include <memory>
#include <unordered_map>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;
class RenderFrameProxyHost;
class RenderViewHostImpl;
class SiteInstance;

// This feature is used to limit the scope of back-forward cache experiment
// without enabling it. To control the URLs list by using this feature by
// generating the metrics only for "allowed_websites" param. Mainly, to ensure
// that metrics from the control and experiment groups are consistent.
constexpr base::Feature kRecordBackForwardCacheMetricsWithoutEnabling{
    "RecordBackForwardCacheMetricsWithoutEnabling",
    base::FEATURE_DISABLED_BY_DEFAULT};

// BackForwardCache:
//
// After the user navigates away from a document, the old one goes into the
// frozen state and is kept in this object. They can potentially be reused
// after an history navigation. Reusing a document means swapping it back with
// the current_frame_host.
class CONTENT_EXPORT BackForwardCacheImpl : public BackForwardCache {
 public:
  enum MessageHandlingPolicyWhenCached {
    kMessagePolicyNone,
    kMessagePolicyLog,
    kMessagePolicyDump,
  };

  static MessageHandlingPolicyWhenCached
  GetChannelAssociatedMessageHandlingPolicy();

  struct Entry {
    using RenderFrameProxyHostMap =
        std::unordered_map<int32_t /* SiteInstance ID */,
                           std::unique_ptr<RenderFrameProxyHost>>;

    Entry(std::unique_ptr<RenderFrameHostImpl> rfh,
          RenderFrameProxyHostMap proxy_hosts,
          std::set<RenderViewHostImpl*> render_view_hosts);
    ~Entry();

    // The main document being stored.
    std::unique_ptr<RenderFrameHostImpl> render_frame_host;

    // Proxies of the main document as seen by other processes.
    // Currently, we only store proxies for SiteInstances of all subframes on
    // the page, because pages using window.open and nested WebContents are not
    // cached.
    RenderFrameProxyHostMap proxy_hosts;

    // RenderViewHosts belonging to the main frame, and its proxies (if any).
    //
    // While RenderViewHostImpl(s) are in the BackForwardCache, they aren't
    // reused for pages outside the cache. This prevents us from having two main
    // frames, (one in the cache, one live), associated with a single
    // RenderViewHost.
    //
    // Keeping these here also prevents RenderFrameHostManager code from
    // unwittingly iterating over RenderViewHostImpls that are in the cache.
    std::set<RenderViewHostImpl*> render_view_hosts;

    // Additional parameters to send with SetPageLifecycleState calls when we're
    // restoring a page from the back-forward cache.
    blink::mojom::PageRestoreParamsPtr page_restore_params;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  BackForwardCacheImpl();
  ~BackForwardCacheImpl();

  // Returns whether a RenderFrameHost can be stored into the BackForwardCache
  // right now. Depends on the |render_frame_host| and its children's state.
  // Should only be called after we've navigated away from |render_frame_host|,
  // which means nothing about the page can change (usage of blocklisted
  // features, pending navigations, load state, etc.) anymore.
  BackForwardCacheCanStoreDocumentResult CanStorePageNow(
      RenderFrameHostImpl* render_frame_host);

  // Whether a RenderFrameHost could be stored into the BackForwardCache at some
  // point in the future. Different than CanStorePageNow() above, we won't check
  // for properties of |render_frame_host| that might change in the future such
  // as usage of certain APIs, loading state, existence of pending navigation
  // requests, etc. This should be treated as a "best guess" on whether a page
  // still has a chance to be stored in the back-forward cache later on, and
  // should not be used as a final check before storing a page to the
  // back-forward cache (for that, use CanStorePageNow() instead).
  BackForwardCacheCanStoreDocumentResult CanPotentiallyStorePageLater(
      RenderFrameHostImpl* render_frame_host);

  // Moves the specified BackForwardCache entry into the BackForwardCache. It
  // can be reused in a future history navigation by using RestoreEntry(). When
  // the BackForwardCache is full, the least recently used document is evicted.
  // Precondition: CanStoreDocument(*(entry->render_frame_host)).
  void StoreEntry(std::unique_ptr<Entry> entry);

  // Returns a pointer to a cached BackForwardCache entry matching
  // |navigation_entry_id| if it exists in the BackForwardCache. Returns nullptr
  // if no matching entry is found.
  //
  // Note: The returned pointer should be used temporarily only within the
  // execution of a single task on the event loop. Beyond that, there is no
  // guarantee the pointer will be valid, because the document may be
  // removed/evicted from the cache.
  Entry* GetEntry(int navigation_entry_id);

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

  // Evict all entries from the BackForwardCache.
  void Flush();

  // Evict all cached pages in the same BrowsingInstance as
  // |site_instance|.
  void EvictFramesInRelatedSiteInstances(SiteInstance* site_instance);

  // Immediately deletes all frames in the cache. This should only be called
  // when WebContents is being destroyed.
  void Shutdown();

  // Posts a task to destroy all frames in the BackForwardCache that have been
  // marked as evicted.
  void PostTaskToDestroyEvictedFrames();

  // Storing frames in back-forward cache is not supported indefinitely
  // due to potential privacy issues and memory leaks. Instead we are evicting
  // the frame from the cache after the time to live, which can be controlled
  // via experiment.
  static base::TimeDelta GetTimeToLiveInBackForwardCache();

  // The back-forward cache is experimented on a limited set of URLs. This
  // method returns true if the |url| matches one of those. URL not matching
  // this won't enter the back-forward cache.
  // This is controlled by GetAllowedURLs method which depends on the
  // following:
  //  - feature::kBackForwardCache param -> allowed_websites.
  //  - kRecordBackForwardCacheMetricsWithoutEnabling param -> allowed_websites.

  // If no param is set all websites are allowed by default. This can still
  // return true even when BackForwardCache is disabled for metrics purposes.
  bool IsAllowed(const GURL& current_url);

  // Returns the task runner that should be used by the eviction timer.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return task_runner_for_testing_ ? task_runner_for_testing_
                                    : base::ThreadTaskRunnerHandle::Get();
  }

  // Inject task runner for precise timing control in browser tests.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    task_runner_for_testing_ = task_runner;
  }

  // Sets the number of documents that can be stored in the cache. This is meant
  // for use from within tests only.
  // If |cache_size_limit_for_testing| is 0 (the default), the normal cache
  // size limit will be used.
  void set_cache_size_limit_for_testing(size_t cache_size_limit_for_testing) {
    cache_size_limit_for_testing_ = cache_size_limit_for_testing;
  }

  const std::list<std::unique_ptr<Entry>>& GetEntries();

  void DisableForTesting(DisableForTestingReason reason) override;

 private:
  // Destroys all evicted frames in the BackForwardCache.
  void DestroyEvictedFrames();

  // Helper for recursively checking each child. See CanStorePageNow() and
  // CanPotentiallyStorePageLater().
  void CheckDynamicStatesOnSubtree(
      BackForwardCacheCanStoreDocumentResult* result,
      RenderFrameHostImpl* render_frame_host);
  void CanStoreRenderFrameHostLater(
      BackForwardCacheCanStoreDocumentResult* result,
      RenderFrameHostImpl* render_frame_host);

  // Contains the set of stored Entries.
  // Invariant:
  // - Ordered from the most recently used to the last recently used.
  // - Once the list is full, the least recently used document is evicted.
  std::list<std::unique_ptr<Entry>> entries_;

  // Only used in tests. Whether the BackforwardCached has been disabled for
  // testing.
  bool is_disabled_for_testing_ = false;

  // Only used in tests. If non-zero, this value will be used as the cache size
  // limit.
  size_t cache_size_limit_for_testing_ = 0;

  // Only used for tests. This task runner is used for precise injection in
  // browser tests and for timing control.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing_;

  // To enter the back-forward cache, the main document URL's must match one of
  // the field trial parameter "allowed_websites". This is represented here by a
  // set of host and path prefix.
  std::map<std::string,              // URL's host,
           std::vector<std::string>  // URL's path prefix
           >
      allowed_urls_;

  base::WeakPtrFactory<BackForwardCacheImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackForwardCacheImpl);
};

// Allow external code to be notified when back-forward cache is disabled for a
// RenderFrameHost. This should be used only by the testing infrastructure which
// want to know the exact reason why the cache was disabled. There can be only
// one observer.
class CONTENT_EXPORT BackForwardCacheTestDelegate {
 public:
  BackForwardCacheTestDelegate();
  virtual ~BackForwardCacheTestDelegate();

  virtual void OnDisabledForFrameWithReason(GlobalFrameRoutingId id,
                                            base::StringPiece reason) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_IMPL_H_
