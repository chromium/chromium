// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_
#define CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_

#include <cstdint>
#include <map>
#include <optional>
#include <set>

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {

class Page;
class RenderFrameHost;

// Public API for the BackForwardCache.
//
// After the user navigates away from a document, the old one might go into the
// frozen state and will be kept in the cache. It can potentially be reused
// at a later time if the user navigates back.
//
// Not all documents can or will be cached. You should not assume a document
// will be cached.
//
// All methods of this class should be called from the UI thread.
class CONTENT_EXPORT BackForwardCache {
 public:
  // LINT.IfChange(NotRestoredReason)
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
    // 6: WasGrantedMediaAccess is no longer blocking.
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
    // 29: kNotMostRecentNavigationEntry was removed.
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
    // 56: kNoResponseHead was fixed.
    // 57: kActivationNavigationsDisallowedForBug1234857 was fixed.
    kErrorDocument = 58,
    // 59: kFencedFramesEmbedder was removed.
    kCookieDisabled = 60,
    kHTTPAuthRequired = 61,
    kCookieFlushed = 62,
    kBroadcastChannelOnMessage = 63,
    kWebViewSettingsChanged = 64,
    kWebViewJavaScriptObjectChanged = 65,
    kWebViewMessageListenerInjected = 66,
    kWebViewSafeBrowsingAllowlistChanged = 67,
    kWebViewDocumentStartJavascriptChanged = 68,
    kMaxValue = kWebViewDocumentStartJavascriptChanged,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:BackForwardCacheNotRestoredReason)

  // Back/forward cache can be disabled from within content and also from
  // embedders. This means we cannot have a unified enum that covers reasons
  // from different layers. Instead we namespace the reasons and allow each
  // source to manage its own enum. The previous approach was to use a hash of
  // the string for logging but this made it hard identify the reasons in the
  // logged data and also meant there was no control over new uses of the API.
  //
  // The logged value is |enum_value + source_type << 16|.
  enum class DisabledSource {
    // We reserve 0 because because the previous approach just used the strings
    // hashed to uint16.
    kLegacy = 0,
    kTesting = 1,
    kContent = 2,
    kEmbedder = 3,
  };
  typedef uint16_t DisabledReasonType;
  static const uint16_t kDisabledReasonTypeBits = 16;

  // Represents a reason to disable back-forward cache, given by a |source|.
  // |context| is arbitrary context that will be preserved and passed through,
  // e.g. an extension ID responsible for disabling BFCache that can be shown in
  // passed devtools. It preserves the |description| and |context| that
  // accompany it, however they are ignored for <, == and !=.
  struct CONTENT_EXPORT DisabledReason {
    DisabledReason(BackForwardCache::DisabledSource source,
                   BackForwardCache::DisabledReasonType id,
                   std::string description,
                   std::string context,
                   std::string report_string);
    DisabledReason(const DisabledReason&);

    const BackForwardCache::DisabledSource source;
    const BackForwardCache::DisabledReasonType id;
    const std::string description;
    const std::string context;
    // Report string used for NotRestoredReasons API. This will be brief and
    // will mask extension related reasons as "Extensions".
    const std::string report_string;

    bool operator<(const DisabledReason&) const;
    bool operator==(const DisabledReason&) const;
    bool operator!=(const DisabledReason&) const;
  };

  // Prevents the `render_frame_host` from entering the BackForwardCache. A
  // RenderFrameHost can only enter the BackForwardCache if the main one and all
  // its children can. This action can not be undone. Any document that is
  // assigned to this same RenderFrameHost in the future will not be cached
  // either. In practice this is not a big deal as only navigations that use a
  // new frame can be cached.
  //
  // This might be needed for example by components that listen to events via a
  // WebContentsObserver and keep some sort of per frame state, as this state
  // might be lost and not be recreated when navigating back.
  //
  // If the page is already in the cache an eviction is triggered.
  //
  // `render_frame_host`: non-null.
  // `reason`: Describes who is disabling this and why.
  // `source_id`: see
  // `BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap` for what it
  // means and when it's set.
  static void DisableForRenderFrameHost(
      RenderFrameHost* render_frame_host,
      DisabledReason reason,
      std::optional<ukm::SourceId> source_id = std::nullopt);

  // Helper function to be used when it is not always possible to guarantee the
  // `render_frame_host` to be still alive when this is called. In this case,
  // its `id` can be used.
  // For what `source_id` means and when it's set, see
  // `BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap`.
  static void DisableForRenderFrameHost(
      GlobalRenderFrameHostId id,
      DisabledReason reason,
      std::optional<ukm::SourceId> source_id = std::nullopt);

  // Helper function to be used when the input |page| has seen any form data
  // associated. This state will be set on the BackForwardCacheMetrics
  // associated with the main frame, is not persisted across session restores,
  // and only set in Android Custom tabs for now.
  // TODO(crbug.com/40251494): Set this boolean for all platforms.
  static void SetHadFormDataAssociated(Page& page);

  // List of reasons the BackForwardCache was disabled for a specific test. If a
  // test needs to be disabled for a reason not covered below, please add to
  // this enum.
  enum DisableForTestingReason {
    // The test has expectations that won't make sense if caching is enabled.
    //
    // One alternative to disabling BackForwardCache is to make the test's logic
    // conditional, based on whether or not BackForwardCache is enabled.
    //
    // You should also consider whether it would make sense to instead split
    // into two tests, one using a cacheable page, and one using an uncacheable
    // page.
    //
    // Even though BackForwardCache is already enabled by default, it is not
    // guaranteed to preserve and cache the previous document on every
    // navigation, and even if it does, it is still possible for a cached
    // document to get discarded without it ever getting restored, so not every
    // history navigation will restore a document from the back/forward cache.
    // Thus, testing cases where a document does not get preserved and cached
    // on navigation or not restored on history navigation is completely valid.
    TEST_REQUIRES_NO_CACHING,

    // Unload events never fire for documents that are put into the
    // BackForwardCache. This is by design, as there is never an appropriate
    // moment to fire unload if the document is cached.
    // In short, this is because:
    //
    // * We can't fire unload when going into the cache, because it may be
    // destructive, and put the document into an unknown/bad state. Pages can
    // also be cached and restored multiple times, and we don't want to invoke
    // unload more than once.
    //
    // * We can't fire unload when the document is evicted from the cache,
    // because at that point we don't want to run javascript for privacy and
    // security reasons.
    //
    // An alternative to disabling the BackForwardCache, is to have the test
    // load a page that is ineligible for caching (e.g. due to an unsupported
    // feature).
    TEST_USES_UNLOAD_EVENT,

    // This test expects that same-site navigations won't result in a
    // RenderFrameHost / RenderFrame / `blink::WebView` / RenderWidget change.
    // But when same-site BackForwardCache is enabled, the change usually does
    // happen. Even so, there will still be valid navigations that don't result
    // in those objects changing, so we should keep the test as is, just with
    // BackForwardCache disabled.
    TEST_ASSUMES_NO_RENDER_FRAME_CHANGE,
  };

  // Returns true if BackForwardCache is enabled.
  static bool IsBackForwardCacheFeatureEnabled();

  // TODO(crbug.com/345117894): Add reasons to all the callsites of Flush().
  // Evict all entries from the BackForwardCache with reason kCacheFlushed.
  virtual void Flush() = 0;

  // Evict all entries from the BackForwardCache with specific reason.
  virtual void Flush(NotRestoredReason reason) = 0;

  // Evict back/forward cache entries from the least recently used ones until
  // the cache is within the given size limit.
  virtual void Prune(size_t limit) = 0;

  // Disables the BackForwardCache so that no documents will be stored/served.
  // This allows tests to "force" not using the BackForwardCache, this can be
  // useful when:
  // * Tests rely on a new document being loaded.
  // * Tests want to test this case specifically.
  // Callers should pass an accurate |reason| to make future triaging of
  // disabled tests easier.
  //
  // Note: It's preferable to make tests BackForwardCache compatible
  // when feasible, rather than using this method. Also please consider whether
  // you actually should have 2 tests, one with the document cached
  // (BackForwardCache enabled), and one without.
  virtual void DisableForTesting(DisableForTestingReason reason) = 0;

 protected:
  BackForwardCache() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_
