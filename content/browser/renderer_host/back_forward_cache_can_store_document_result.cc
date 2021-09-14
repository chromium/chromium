// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"

#include <inttypes.h>
#include <cstdint>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/common/debug_utils.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

std::string DescribeFeatures(BlockListedFeatures blocklisted_features) {
  std::vector<std::string> features;
  for (WebSchedulerTrackedFeature feature : blocklisted_features) {
    features.push_back(blink::scheduler::FeatureToHumanReadableString(feature));
  }
  return base::JoinString(features, ", ");
}

const char* BrowsingInstanceSwapResultToString(
    absl::optional<ShouldSwapBrowsingInstance> reason) {
  if (!reason)
    return "no BI swap result";
  switch (reason.value()) {
    case ShouldSwapBrowsingInstance::kYes_ForceSwap:
      return "forced BI swap";
    case ShouldSwapBrowsingInstance::kNo_ProactiveSwapDisabled:
      return "BI not swapped - proactive swap disabled";
    case ShouldSwapBrowsingInstance::kNo_NotMainFrame:
      return "BI not swapped - not a main frame";
    case ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents:
      return "BI not swapped - has related active contents";
    case ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite:
      return "BI not swapped - current SiteInstance does not have site";
    case ShouldSwapBrowsingInstance::kNo_SourceURLSchemeIsNotHTTPOrHTTPS:
      return "BI not swapped - source URL scheme is not HTTP(S)";
    case ShouldSwapBrowsingInstance::kNo_SameSiteNavigation:
      return "BI not swapped - same site navigation";
    case ShouldSwapBrowsingInstance::kNo_AlreadyHasMatchingBrowsingInstance:
      return "BI not swapped - already has matching BrowsingInstance";
    case ShouldSwapBrowsingInstance::kNo_RendererDebugURL:
      return "BI not swapped - URL is a renderer debug URL";
    case ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache:
      return "BI not swapped - old page can't be stored in bfcache";
    case ShouldSwapBrowsingInstance::kYes_CrossSiteProactiveSwap:
      return "proactively swapped BI (cross-site)";
    case ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap:
      return "proactively swapped BI (same-site)";
    case ShouldSwapBrowsingInstance::kNo_SameDocumentNavigation:
      return "BI not swapped - same-document navigation";
    case ShouldSwapBrowsingInstance::kNo_SameUrlNavigation:
      return "BI not swapped - navigation to the same URL";
    case ShouldSwapBrowsingInstance::kNo_WillReplaceEntry:
      return "BI not swapped - navigation entry will be replaced";
    case ShouldSwapBrowsingInstance::kNo_Reload:
      return "BI not swapped - reloading";
    case ShouldSwapBrowsingInstance::kNo_Guest:
      return "BI not swapped - <webview> guest";
    case ShouldSwapBrowsingInstance::kNo_HasNotComittedAnyNavigation:
      return "BI not swapped - hasn't committed any navigation";
    case ShouldSwapBrowsingInstance::
        kNo_UnloadHandlerExistsOnSameSiteNavigation:
      return "BI not swapped - unload handler exists and the navigation is "
             "same-site";
  }
}

}  // namespace

bool BackForwardCacheCanStoreDocumentResult::HasNotStoredReason(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  return not_stored_reasons_.Has(reason);
}

void BackForwardCacheCanStoreDocumentResult::AddNotStoredReason(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  not_stored_reasons_.Put(reason);
  if (reason == BackForwardCacheMetrics::NotRestoredReason::kNoResponseHead ||
      reason ==
          BackForwardCacheMetrics::NotRestoredReason::kSchemeNotHTTPOrHTTPS) {
    if (not_stored_reasons_.Has(
            BackForwardCacheMetrics::NotRestoredReason::kNoResponseHead) &&
        not_stored_reasons_.Has(BackForwardCacheMetrics::NotRestoredReason::
                                    kSchemeNotHTTPOrHTTPS) &&
        !not_stored_reasons_.Has(
            BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK)) {
      CaptureTraceForNavigationDebugScenario(
          DebugScenario::kDebugNoResponseHeadForHttpOrHttps);
      base::debug::DumpWithoutCrashing();
    }
  }
}

bool BackForwardCacheCanStoreDocumentResult::CanStore() const {
  return not_stored_reasons_.Empty();
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

std::string DisallowActivationReasonsToString(
    const std::set<uint64_t>& reasons) {
  std::vector<std::string> descriptions;
  for (const uint64_t reason : reasons) {
    descriptions.push_back(base::StringPrintf("%" PRIu64, reason));
  }
  return base::JoinString(descriptions, ", ");
}
}  // namespace

std::string BackForwardCacheCanStoreDocumentResult::ToString() const {
  if (CanStore())
    return "Yes";

  std::vector<std::string> reason_strs;

  for (BackForwardCacheMetrics::NotRestoredReason reason :
       not_stored_reasons_) {
    reason_strs.push_back(NotRestoredReasonToString(reason));
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
      return base::StringPrintf(
          "related active contents exist: %s",
          BrowsingInstanceSwapResultToString(browsing_instance_swap_result_));
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
             "bfcache " +
             DisallowActivationReasonsToString(disallow_activation_reasons_);
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
    case Reason::kServiceWorkerUnregistration:
      return "ServiceWorker is unregistered while the controllee page is in "
             "bfcache.";
    case Reason::kCacheControlNoStore:
      return "Pages with cache-control:no-store went into bfcache temporarily "
             "because of the flag, and there was no cookie change.";
    case Reason::kCacheControlNoStoreCookieModified:
      return "Pages with cache-control:no-store went into bfcache temporarily "
             "because of the flag, and while in bfcache the cookie was "
             "modified or deleted and thus evicted.";
    case Reason::kCacheControlNoStoreHTTPOnlyCookieModified:
      return "Pages with cache-control:no-store went into bfcache temporarily "
             "because of the flag, and while in bfcache the HTTP-only cookie"
             "was modified or deleted and thus evicted.";
    case Reason::kNoResponseHead:
      return "main RenderFrameHost doesn't have response headers set, probably "
             "due not having successfully committed a navigation.";
    case Reason::kActivationNavigationsDisallowedForBug1234857:
      return "Activation navigations are disallowed to avoid bypassing "
             "PasswordProtectionService as a workaround for "
             "https://crbug.com/1234857.";
  }
}

void BackForwardCacheCanStoreDocumentResult::No(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  // Either |NoDueToFeatures()| or |NoDueToDisableForRenderFrameHostCalled|
  // should be called instead.
  DCHECK_NE(reason,
            BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures);
  DCHECK_NE(reason, BackForwardCacheMetrics::NotRestoredReason::
                        kDisableForRenderFrameHostCalled);

  AddNotStoredReason(reason);
}

void BackForwardCacheCanStoreDocumentResult::NoDueToFeatures(
    BlockListedFeatures features) {
  AddNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures);
  blocklisted_features_.PutAll(features);
}

void BackForwardCacheCanStoreDocumentResult::
    NoDueToDisableForRenderFrameHostCalled(
        const std::set<BackForwardCache::DisabledReason>& reasons) {
  AddNotStoredReason(BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled);
  for (const BackForwardCache::DisabledReason& reason : reasons)
    disabled_reasons_.insert(reason);
}

void BackForwardCacheCanStoreDocumentResult::NoDueToRelatedActiveContents(
    absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result) {
  AddNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kRelatedActiveContentsExist);
  browsing_instance_swap_result_ = browsing_instance_swap_result;
}

void BackForwardCacheCanStoreDocumentResult::NoDueToDisallowActivation(
    uint64_t reason) {
  AddNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict);
  disallow_activation_reasons_.insert(reason);
}

void BackForwardCacheCanStoreDocumentResult::AddReasonsFrom(
    const BackForwardCacheCanStoreDocumentResult& other) {
  not_stored_reasons_.PutAll(other.not_stored_reasons_);
  blocklisted_features_.PutAll(other.blocklisted_features());
  for (const BackForwardCache::DisabledReason& reason :
       other.disabled_reasons()) {
    disabled_reasons_.insert(reason);
  }
  if (other.browsing_instance_swap_result_)
    browsing_instance_swap_result_ = other.browsing_instance_swap_result_;
  for (const auto reason : other.disallow_activation_reasons()) {
    disallow_activation_reasons_.insert(reason);
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
