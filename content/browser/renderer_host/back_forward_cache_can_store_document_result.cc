// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

std::string DescribeFeatures(uint64_t blocklisted_features) {
  std::vector<std::string> features;
  for (uint32_t i = 0;
       i <= static_cast<uint32_t>(WebSchedulerTrackedFeature::kMaxValue); ++i) {
    if (blocklisted_features & (1ULL << i)) {
      features.push_back(blink::scheduler::FeatureToHumanReadableString(
          static_cast<WebSchedulerTrackedFeature>(i)));
    }
  }
  return base::JoinString(features, ", ");
}

}  // namespace

bool BackForwardCacheCanStoreDocumentResult::CanStore() const {
  return not_stored_reasons_.none();
}

namespace {
std::string DisabledReasonsToString(
    const std::set<BackForwardCache::DisabledReason>& reasons) {
  std::vector<std::string> descriptions;
  for (const auto& reason : reasons) {
    descriptions.push_back(base::StringPrintf(
        "%d:%d:%s", reason.source, reason.id, reason.description.c_str()));
  }
  return base::JoinString(descriptions, ", ");
}
}  // namespace

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
             DisabledReasonsToString(disabled_reasons_);
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
    case Reason::kForegroundCacheLimit:
      return "foreground cache limit";
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
    case Reason::kNavigationCancelledWhileRestoring:
      return "Navigation request was cancelled after js eviction was disabled";
    case Reason::kNetworkRequestRedirected:
      return "Network request is redirected in bfcache";
    case Reason::kNetworkRequestTimeout:
      return "Network request is open for too long and exceeds time limit";
    case Reason::kNetworkExceedsBufferLimit:
      return "Network request reads too much data and exceeds buffer limit";
    case Reason::kBackForwardCacheDisabledForPrerender:
      return "BackForwardCache is disabled for Prerender";
    case Reason::kUserAgentOverrideDiffers:
      return "User-agent override differs";
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return "Network requests' datapipe has been passed as bytes consumer";
    case Reason::kBrowsingInstanceNotSwapped:
      return "Browsing instance is not swapped";
    case Reason::kBackForwardCacheDisabledForDelegate:
      return "BackForwardCache is not supported by delegate";
    case Reason::kOptInUnloadHeaderNotPresent:
      return "BFCache-Opt-In header not present, or does not include `unload` "
             "token, and an experimental config which requires it is active.";
    case Reason::kUnloadHandlerExistsInMainFrame:
      return "Unload handler exists in the main frame, and the current "
             "experimental config doesn't permit it to be BFCached.";
    case Reason::kUnloadHandlerExistsInSubFrame:
      return "Unload handler exists in a sub frame, and the current "
             "experimental config doesn't permit it to be BFCached.";
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

void BackForwardCacheCanStoreDocumentResult::
    NoDueToDisableForRenderFrameHostCalled(
        const std::set<BackForwardCache::DisabledReason>& reasons) {
  not_stored_reasons_.set(
      static_cast<size_t>(BackForwardCacheMetrics::NotRestoredReason::
                              kDisableForRenderFrameHostCalled));
  for (const BackForwardCache::DisabledReason& reason : reasons)
    disabled_reasons_.insert(reason);
}

void BackForwardCacheCanStoreDocumentResult::AddReasonsFrom(
    const BackForwardCacheCanStoreDocumentResult& other) {
  not_stored_reasons_ |= other.not_stored_reasons();
  blocklisted_features_ |= other.blocklisted_features();
  for (const BackForwardCache::DisabledReason& reason :
       other.disabled_reasons()) {
    disabled_reasons_.insert(reason);
  }
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
