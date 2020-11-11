// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"

#include "base/strings/string_util.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

std::string DescribeFeatures(uint64_t blocklisted_features) {
  std::vector<std::string> features;
  for (size_t i = 0;
       i <= static_cast<size_t>(WebSchedulerTrackedFeature::kMaxValue); ++i) {
    if (blocklisted_features & (1 << i)) {
      features.push_back(blink::scheduler::FeatureToString(
          static_cast<WebSchedulerTrackedFeature>(i)));
    }
  }
  return base::JoinString(features, ", ");
}

}  // namespace

bool BackForwardCacheCanStoreDocumentResult::CanStore() const {
  return not_stored_reasons_.none();
}

std::string BackForwardCacheCanStoreDocumentResult::ToString() const {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;

  if (CanStore())
    return "Yes";

  std::vector<std::string> reason_strs;

  for (int i = 0; i <= static_cast<int>(Reason::kMaxValue); i++) {
    if (!not_stored_reasons_.test(static_cast<size_t>(i)))
      continue;

    reason_strs.push_back(NotRestoredReasonToString(static_cast<Reason>(i)));
  }

  return "No: " + base::JoinString(reason_strs, ", ");
}

std::string BackForwardCacheCanStoreDocumentResult::NotRestoredReasonToString(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;

  switch (reason) {
    case Reason::kNotMainFrame:
      return "not a main frame";
    case Reason::kBackForwardCacheDisabled:
      return "BackForwardCache disabled";
    case Reason::kRelatedActiveContentsExist:
      return "related active contents exist";
    case Reason::kHTTPStatusNotOK:
      return "HTTP status is not OK";
    case Reason::kSchemeNotHTTPOrHTTPS:
      return "scheme is not HTTP or HTTPS";
    case Reason::kLoading:
      return "frame is not fully loaded";
    case Reason::kWasGrantedMediaAccess:
      return "frame was granted microphone or camera access";
    case Reason::kBlocklistedFeatures:
      return "blocklisted features: " + DescribeFeatures(blocklisted_features_);
    case Reason::kDisableForRenderFrameHostCalled:
      return "BackForwardCache::DisableForRenderFrameHost() was called: " +
             base::JoinString(
                 std::vector<std::string>(disabled_reasons_.begin(),
                                          disabled_reasons_.end()),
                 ", ");
    case Reason::kDomainNotAllowed:
      return "This domain is not allowed to be stored in BackForwardCache";
    case Reason::kHTTPMethodNotGET:
      return "HTTP method is not GET";
    case Reason::kSubframeIsNavigating:
      return "subframe navigation is in progress";
    case Reason::kTimeout:
      return "timeout";
    case Reason::kCacheLimit:
      return "cache limit";
    case Reason::kJavaScriptExecution:
      return "JavaScript execution";
    case Reason::kRendererProcessKilled:
      return "renderer process is killed";
    case Reason::kRendererProcessCrashed:
      return "renderer process crashed";
    case Reason::kGrantedMediaStreamAccess:
      return "granted media stream access";
    case Reason::kSchedulerTrackedFeatureUsed:
      return "scheduler tracked feature is used";
    case Reason::kConflictingBrowsingInstance:
      return "conflicting BrowsingInstance";
    case Reason::kCacheFlushed:
      return "cache flushed";
    case Reason::kServiceWorkerVersionActivation:
      return "service worker version is activated";
    case Reason::kSessionRestored:
      return "session restored";
    case Reason::kUnknown:
      return "unknown";
    case Reason::kServiceWorkerPostMessage:
      return "postMessage from service worker";
    case Reason::kEnteredBackForwardCacheBeforeServiceWorkerHostAdded:
      return "frame already in the cache when service worker host was added";
    case Reason::kRenderFrameHostReused_SameSite:
      return "RenderFrameHost is reused for a same-site navigation";
    case Reason::kRenderFrameHostReused_CrossSite:
      return "RenderFrameHost is reused for a cross-site navigation";
    case Reason::kNotMostRecentNavigationEntry:
      return "navigation entry is not the most recent one for this document";
    case Reason::kServiceWorkerClaim:
      return "service worker claim is called";
    case Reason::kIgnoreEventAndEvict:
      return "IsInactiveAndDisallowReactivation() was called for the frame in "
             "bfcache";
    case Reason::kHaveInnerContents:
      return "RenderFrameHost has inner WebContents attached";
    case Reason::kTimeoutPuttingInCache:
      return "Timed out while waiting for page to acknowledge freezing";
    case Reason::kBackForwardCacheDisabledByLowMemory:
      return "BackForwardCache is disabled due to low memory of the device";
    case Reason::kBackForwardCacheDisabledByCommandLine:
      return "BackForwardCache is disabled through command line (may include "
             "cases where the embedder disabled it due to, e.g., enterprise "
             "policy)";
    case Reason::kFrameTreeNodeStateReset:
      return "document-associated state stored in FrameTreeNode was lost after "
             "navigating away";
  }
}

void BackForwardCacheCanStoreDocumentResult::No(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  not_stored_reasons_.set(static_cast<size_t>(reason));
}

void BackForwardCacheCanStoreDocumentResult::NoDueToFeatures(
    uint64_t features) {
  not_stored_reasons_.set(static_cast<size_t>(
      BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures));
  blocklisted_features_ |= features;
}

void BackForwardCacheCanStoreDocumentResult::NoDueToRelatedActiveContents(
    base::Optional<ShouldSwapBrowsingInstance>
        browsing_instance_not_swapped_reason) {
  not_stored_reasons_.set(static_cast<size_t>(
      BackForwardCacheMetrics::NotRestoredReason::kRelatedActiveContentsExist));
  browsing_instance_not_swapped_reason_ = browsing_instance_not_swapped_reason;
}

void BackForwardCacheCanStoreDocumentResult::
    NoDueToDisableForRenderFrameHostCalled(
        const std::set<std::string>& reasons) {
  not_stored_reasons_.set(
      static_cast<size_t>(BackForwardCacheMetrics::NotRestoredReason::
                              kDisableForRenderFrameHostCalled));
  for (const std::string& reason : reasons)
    disabled_reasons_.insert(reason);
}

BackForwardCacheCanStoreDocumentResult::
    BackForwardCacheCanStoreDocumentResult() = default;
BackForwardCacheCanStoreDocumentResult::BackForwardCacheCanStoreDocumentResult(
    BackForwardCacheCanStoreDocumentResult&&) = default;
BackForwardCacheCanStoreDocumentResult&
BackForwardCacheCanStoreDocumentResult::operator=(
    BackForwardCacheCanStoreDocumentResult&&) = default;
BackForwardCacheCanStoreDocumentResult::
    ~BackForwardCacheCanStoreDocumentResult() = default;

}  // namespace content
