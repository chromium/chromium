// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"

#include <inttypes.h>
#include <cstdint>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/debug_utils.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;
using Reason = BackForwardCacheMetrics::NotRestoredReason;

std::string DescribeFeatures(BlockListedFeatures blocklisted_features) {
  std::vector<std::string> features;
  for (WebSchedulerTrackedFeature feature : blocklisted_features) {
    features.push_back(blink::scheduler::FeatureToHumanReadableString(feature));
  }
  return base::JoinString(features, ", ");
}

std::vector<std::string> FeaturesToStringVector(
    BlockListedFeatures blocklisted_features) {
  std::vector<std::string> features;
  for (WebSchedulerTrackedFeature feature : blocklisted_features) {
    features.push_back(blink::scheduler::FeatureToShortString(feature));
  }
  return features;
}

const char* BrowsingInstanceSwapResultToString(
    std::optional<ShouldSwapBrowsingInstance> reason) {
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
    case ShouldSwapBrowsingInstance::kNo_NotPrimaryMainFrame:
      return "BI not swapped - not a primary main frame";
    case ShouldSwapBrowsingInstance::kNo_InitiatorRequestedNoProactiveSwap:
      return "BI not swapped - initiator requested no proactive swap";
  }
}

using ProtoEnum =
    perfetto::protos::pbzero::BackForwardCacheCanStoreDocumentResult;
ProtoEnum::BackForwardCacheNotRestoredReason NotRestoredReasonToTraceEnum(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  switch (reason) {
    case Reason::kNotPrimaryMainFrame:
      return ProtoEnum::NOT_MAIN_FRAME;
    case Reason::kBackForwardCacheDisabled:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED;
    case Reason::kRelatedActiveContentsExist:
      return ProtoEnum::RELATED_ACTIVE_CONTENTS_EXIST;
    case Reason::kHTTPStatusNotOK:
      return ProtoEnum::HTTP_STATUS_NOT_OK;
    case Reason::kSchemeNotHTTPOrHTTPS:
      return ProtoEnum::SCHEME_NOT_HTTP_OR_HTTPS;
    case Reason::kLoading:
      return ProtoEnum::LOADING;
    case Reason::kDisableForRenderFrameHostCalled:
      return ProtoEnum::DISABLE_FOR_RENDER_FRAME_HOST_CALLED;
    case Reason::kDomainNotAllowed:
      return ProtoEnum::DOMAIN_NOT_ALLOWED;
    case Reason::kHTTPMethodNotGET:
      return ProtoEnum::HTTP_METHOD_NOT_GET;
    case Reason::kSubframeIsNavigating:
      return ProtoEnum::SUBFRAME_IS_NAVIGATING;
    case Reason::kTimeout:
      return ProtoEnum::TIMEOUT;
    case Reason::kCacheLimit:
      return ProtoEnum::CACHE_LIMIT;
    case Reason::kJavaScriptExecution:
      return ProtoEnum::JAVASCRIPT_EXECUTION;
    case Reason::kRendererProcessKilled:
      return ProtoEnum::RENDERER_PROCESS_KILLED;
    case Reason::kRendererProcessCrashed:
      return ProtoEnum::RENDERER_PROCESS_CRASHED;
    case Reason::kConflictingBrowsingInstance:
      return ProtoEnum::CONFLICTING_BROWSING_INSTANCE;
    case Reason::kCacheFlushed:
      return ProtoEnum::CACHE_FLUSHED;
    case Reason::kServiceWorkerVersionActivation:
      return ProtoEnum::SERVICE_WORKER_VERSION_ACTIVATION;
    case Reason::kSessionRestored:
      return ProtoEnum::SESSION_RESTORED;
    case Reason::kServiceWorkerPostMessage:
      return ProtoEnum::SERVICE_WORKER_POST_MESSAGE;
    case Reason::kEnteredBackForwardCacheBeforeServiceWorkerHostAdded:
      return ProtoEnum::
          ENTERED_BACK_FORWARD_CACHE_BEFORE_SERVICE_WORKER_HOST_ADDED;
    case Reason::kServiceWorkerClaim:
      return ProtoEnum::SERVICE_WORKER_CLAIM;
    case Reason::kIgnoreEventAndEvict:
      return ProtoEnum::IGNORE_EVENT_AND_EVICT;
    case Reason::kHaveInnerContents:
      return ProtoEnum::HAVE_INNER_CONTENTS;
    case Reason::kTimeoutPuttingInCache:
      return ProtoEnum::TIMEOUT_PUTTING_IN_CACHE;
    case Reason::kBackForwardCacheDisabledByLowMemory:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED_BY_LOW_MEMORY;
    case Reason::kBackForwardCacheDisabledByCommandLine:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED_BY_COMMAND_LINE;
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return ProtoEnum::NETWORK_REQUEST_DATAPIPE_DRAINED_AS_BYTES_CONSUMER;
    case Reason::kNetworkRequestRedirected:
      return ProtoEnum::NETWORK_REQUEST_REDIRECTED;
    case Reason::kNetworkRequestTimeout:
      return ProtoEnum::NETWORK_REQUEST_TIMEOUT;
    case Reason::kNetworkExceedsBufferLimit:
      return ProtoEnum::NETWORK_EXCEEDS_BUFFER_LIMIT;
    case Reason::kNavigationCancelledWhileRestoring:
      return ProtoEnum::NAVIGATION_CANCELLED_WHILE_RESTORING;
    case Reason::kUserAgentOverrideDiffers:
      return ProtoEnum::USER_AGENT_OVERRIDE_DIFFERS;
    case Reason::kForegroundCacheLimit:
      return ProtoEnum::FOREGROUND_CACHE_LIMIT;
    case Reason::kBrowsingInstanceNotSwapped:
      return ProtoEnum::BROWSING_INSTANCE_NOT_SWAPPED;
    case Reason::kBackForwardCacheDisabledForDelegate:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED_FOR_DELEGATE;
    case Reason::kUnloadHandlerExistsInMainFrame:
      return ProtoEnum::UNLOAD_HANDLER_EXISTS_IN_MAIN_FRAME;
    case Reason::kUnloadHandlerExistsInSubFrame:
      return ProtoEnum::UNLOAD_HANDLER_EXISTS_IN_SUBFRAME;
    case Reason::kServiceWorkerUnregistration:
      return ProtoEnum::SERVICE_WORKER_UNREGISTRATION;
    case Reason::kCacheControlNoStore:
      return ProtoEnum::CACHE_CONTROL_NO_STORE;
    case Reason::kCacheControlNoStoreCookieModified:
      return ProtoEnum::CACHE_CONTROL_NO_STORE_COOKIE_MODIFIED;
    case Reason::kCacheControlNoStoreHTTPOnlyCookieModified:
      return ProtoEnum::CACHE_CONTROL_NO_STORE_HTTP_ONLY_COOKIE_MODIFIED;
    case Reason::kErrorDocument:
      return ProtoEnum::ERROR_DOCUMENT;
    case Reason::kCookieDisabled:
      return ProtoEnum::COOKIE_DISABLED;
    case Reason::kHTTPAuthRequired:
      return ProtoEnum::HTTP_AUTH_REQUIRED;
    case Reason::kCookieFlushed:
      return ProtoEnum::COOKIE_FLUSHED;
    case Reason::kBroadcastChannelOnMessage:
      return ProtoEnum::BROADCAST_CHANNEL_ON_MESSAGE;
    case Reason::kWebViewSettingsChanged:
      return ProtoEnum::WEBVIEW_SETTINGS_CHANGED;
    case Reason::kWebViewJavaScriptObjectChanged:
      return ProtoEnum::WEBVIEW_JAVASCRIPT_OBJECT_CHANGED;
    case Reason::kWebViewMessageListenerInjected:
      return ProtoEnum::WEBVIEW_MESSAGE_LISTENER_INJECTED;
    case Reason::kWebViewSafeBrowsingAllowlistChanged:
      return ProtoEnum::WEBVIEW_SAFE_BROWSING_ALLOWLIST_CHANGED;
    case Reason::kWebViewDocumentStartJavascriptChanged:
      return ProtoEnum::WEBVIEW_DOCUMENT_START_JAVASCRIPT_CHANGED;
    case Reason::kBlocklistedFeatures:
      return ProtoEnum::BLOCKLISTED_FEATURES;
    case Reason::kUnknown:
      return ProtoEnum::UNKNOWN;
  }
  NOTREACHED_IN_MIGRATION();
  return ProtoEnum::UNKNOWN;
}

}  // namespace

void BackForwardCacheCanStoreDocumentResult::WriteIntoTrace(
    perfetto::TracedProto<
        perfetto::protos::pbzero::BackForwardCacheCanStoreDocumentResult>
        result) const {
  for (auto reason : not_restored_reasons()) {
    result->set_back_forward_cache_not_restored_reason(
        NotRestoredReasonToTraceEnum(reason));
  }
}

bool BackForwardCacheCanStoreDocumentResult::operator==(
    const BackForwardCacheCanStoreDocumentResult& other) const {
  return not_restored_reasons() == other.not_restored_reasons() &&
         blocklisted_features() == other.blocklisted_features() &&
         disabled_reasons() == other.disabled_reasons() &&
         browsing_instance_swap_result() ==
             other.browsing_instance_swap_result() &&
         disallow_activation_reasons() == other.disallow_activation_reasons() &&
         ax_events() == other.ax_events();
}

bool BackForwardCacheCanStoreDocumentResult::HasNotRestoredReason(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  return not_restored_reasons_.Has(reason);
}

void BackForwardCacheCanStoreDocumentResult::AddNotRestoredReason(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  not_restored_reasons_.Put(reason);
}

bool BackForwardCacheCanStoreDocumentResult::CanStore() const {
  if (not_restored_reasons_.Has(Reason::kCacheControlNoStore) ||
      not_restored_reasons_.Has(Reason::kCacheControlNoStoreCookieModified) ||
      not_restored_reasons_.Has(
          Reason::kCacheControlNoStoreHTTPOnlyCookieModified)) {
    // Cache-control:no-store related reasons are only recorded when the
    // experiment is on to allow pages with cache-control:no-store into back/
    // forward cache.
    // If there are other reasons present outside of cache-control:no-store
    // related reasons, the page is not eligible for storing.
    return Difference(not_restored_reasons_,
                      {Reason::kCacheControlNoStore,
                       Reason::kCacheControlNoStoreCookieModified,
                       Reason::kCacheControlNoStoreHTTPOnlyCookieModified})
        .empty();
  } else {
    return not_restored_reasons_.empty();
  }
}

bool BackForwardCacheCanStoreDocumentResult::CanRestore() const {
  return not_restored_reasons_.empty();
}

const BlockListedFeatures
BackForwardCacheCanStoreDocumentResult::blocklisted_features() const {
  BlockListedFeatures features;
  for (const auto& [key, value] : blocking_details_map_) {
    features.Put(key);
  }
  return features;
}

namespace {
std::string DisabledReasonsToString(
    const BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap& reasons,
    bool for_not_restored_reasons = false) {
  std::vector<std::string> descriptions;
  for (const auto& [reason, _] : reasons) {
    std::string string_to_add;
    if (for_not_restored_reasons) {
      // When |for_not_restored_reasons| is true, prepare a string to report to
      // NotRestoredReasons API. For this we report brief strings, and we mask
      // extension related reasons saying "Extensions".
      string_to_add = reason.report_string;
    } else {
      string_to_add = base::StringPrintf(
          "%d:%d:%s:%s", static_cast<int>(reason.source), reason.id,
          reason.description.c_str(), reason.context.c_str());
    }
    descriptions.push_back(string_to_add);
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
       not_restored_reasons_) {
    reason_strs.push_back(NotRestoredReasonToString(reason));
  }
  return "No: " + base::JoinString(reason_strs, ", ");
}

std::unordered_set<std::string>
BackForwardCacheCanStoreDocumentResult::GetStringReasons() const {
  // Use unordered_set to avoid duplicate items.
  std::unordered_set<std::string> reason_strs;
  for (BackForwardCacheMetrics::NotRestoredReason reason :
       not_restored_reasons_) {
    switch (reason) {
      case Reason::kBlocklistedFeatures:
        for (auto feature : FeaturesToStringVector(blocklisted_features())) {
          reason_strs.insert(feature);
        }
        break;
      default:
        reason_strs.insert(NotRestoredReasonToReportString(reason));
    }
  }
  return reason_strs;
}

std::string BackForwardCacheCanStoreDocumentResult::NotRestoredReasonToString(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  switch (reason) {
    case Reason::kNotPrimaryMainFrame:
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
    case Reason::kBlocklistedFeatures:
      return "blocklisted features: " +
             DescribeFeatures(blocklisted_features());
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
    case Reason::kUserAgentOverrideDiffers:
      return "User-agent override differs";
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return "Network requests' datapipe has been passed as bytes consumer";
    case Reason::kBrowsingInstanceNotSwapped:
      return "Browsing instance is not swapped";
    case Reason::kBackForwardCacheDisabledForDelegate:
      return "BackForwardCache is not supported by delegate";
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
    case Reason::kErrorDocument:
      return "Error documents cannot be stored in bfcache";
    case Reason::kCookieDisabled:
      return "Cookie is disabled for the page.";
    case Reason::kHTTPAuthRequired:
      return "Same-origin HTTP authentication is required in another tab.";
    case Reason::kCookieFlushed:
      return "Cookie is flushed.";
    case Reason::kBroadcastChannelOnMessage:
      return "Broadcast channel in bfcache received a message";
    case Reason::kWebViewSettingsChanged:
      return "Android WebView settings changed";
    case Reason::kWebViewJavaScriptObjectChanged:
      return "Android WebView injected javascript object changed";
    case Reason::kWebViewMessageListenerInjected:
      return "Android WebView injected new message listener";
    case Reason::kWebViewSafeBrowsingAllowlistChanged:
      return "Android WebView safe browsing allowlist changed";
    case Reason::kWebViewDocumentStartJavascriptChanged:
      return "Android WebView document start script changed";
  }
}

std::string
BackForwardCacheCanStoreDocumentResult::NotRestoredReasonToReportString(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  switch (reason) {
    // Report strings have to match the ones defined in the spec.
    // If you ever add a new one, you have to add it to the spec as well.
    // https://html.spec.whatwg.org/#nrr-details-reason
    case Reason::kNotPrimaryMainFrame:
      return "not-main-frame";
    case Reason::kRelatedActiveContentsExist:
      return "non-trivial-browsing-context-group";
    case Reason::kSchemeNotHTTPOrHTTPS:
      return "response-scheme-not-http-or-https";
    case Reason::kLoading:
      return "navigating";
    case Reason::kBlocklistedFeatures:
      // This should not be reported. Instead actual feature list will be
      // reported.
      return "Blocklisted feature";
    case Reason::kHTTPMethodNotGET:
      return "response-method-not-get";
    case Reason::kSubframeIsNavigating:
      return "frame-navigating";
    case Reason::kTimeout:
      return "timeout";
    case Reason::kServiceWorkerVersionActivation:
      return "serviceworker-version-activation";
    case Reason::kSessionRestored:
      return "session-restored";
    case Reason::kServiceWorkerPostMessage:
      return "serviceworker-postmessage";
    case Reason::kEnteredBackForwardCacheBeforeServiceWorkerHostAdded:
      return "serviceworker-added-after-bfcache";
    case Reason::kServiceWorkerClaim:
      return "serviceworker-claim";
    case Reason::kNavigationCancelledWhileRestoring:
      return "navigation-canceled";
    case Reason::kServiceWorkerUnregistration:
      return "serviceworker-unregistration";
    case Reason::kErrorDocument:
    case Reason::kHTTPStatusNotOK:
      return "response-status-not-ok";
    case Reason::kUnloadHandlerExistsInMainFrame:
    case Reason::kUnloadHandlerExistsInSubFrame:
      return "unload-listener";
    case Reason::kNetworkRequestRedirected:
    case Reason::kNetworkRequestTimeout:
    case Reason::kNetworkExceedsBufferLimit:
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return "outstanding-network-request";
    case Reason::kBroadcastChannelOnMessage:
      return "broadcastchannel-message";
    case Reason::kCacheControlNoStore:
    case Reason::kCacheControlNoStoreCookieModified:
    case Reason::kCacheControlNoStoreHTTPOnlyCookieModified:
      return "response-cache-control-no-store";
    case Reason::kCookieDisabled:
      return "cookie-disabled";
    case Reason::kHTTPAuthRequired:
      return "response-auth-required";
    case Reason::kCookieFlushed:
      return "cookie-removed";
    case Reason::kDisableForRenderFrameHostCalled:
      return DisabledReasonsToString(disabled_reasons_,
                                     /*for_not_restored_reasons=*/true);
    case Reason::kUserAgentOverrideDiffers:
    case Reason::kCacheFlushed:
    case Reason::kCacheLimit:
    case Reason::kForegroundCacheLimit:
    case Reason::kHaveInnerContents:
    case Reason::kJavaScriptExecution:
    case Reason::kBackForwardCacheDisabledByLowMemory:
    case Reason::kBackForwardCacheDisabled:
    case Reason::kBackForwardCacheDisabledByCommandLine:
    case Reason::kBackForwardCacheDisabledForDelegate:
    case Reason::kBrowsingInstanceNotSwapped:
    case Reason::kConflictingBrowsingInstance:
    case Reason::kDomainNotAllowed:
    case Reason::kIgnoreEventAndEvict:
    case Reason::kRendererProcessKilled:
    case Reason::kRendererProcessCrashed:
    case Reason::kTimeoutPuttingInCache:
    case Reason::kWebViewSettingsChanged:
    case Reason::kWebViewJavaScriptObjectChanged:
    case Reason::kWebViewMessageListenerInjected:
    case Reason::kWebViewSafeBrowsingAllowlistChanged:
    case Reason::kWebViewDocumentStartJavascriptChanged:
    case Reason::kUnknown:
      return "masked";
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

  AddNotRestoredReason(reason);
}

void BackForwardCacheCanStoreDocumentResult::NoDueToFeatures(
    BlockingDetailsMap map) {
  AddNotRestoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures);
  for (const auto& [k, v] : map) {
    if (blocking_details_map_.contains(k)) {
      for (auto& details : map[k]) {
        blocking_details_map_[k].push_back(std::move(details));
      }
    } else {
      blocking_details_map_[k] = std::move(map[k]);
    }
  }
}

void BackForwardCacheCanStoreDocumentResult::
    NoDueToDisableForRenderFrameHostCalled(
        const BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap&
            reasons) {
  // This should only be called with non-empty reasons.
  DCHECK(reasons.size());
  for (const auto& reason : reasons) {
    disabled_reasons_.insert(reason);
    // This will be a no-op after the first time but it's written like this to
    // guarantee that we do not set it without a reason.
    AddNotRestoredReason(BackForwardCacheMetrics::NotRestoredReason::
                             kDisableForRenderFrameHostCalled);
  }
}

void BackForwardCacheCanStoreDocumentResult::NoDueToRelatedActiveContents(
    std::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result) {
  AddNotRestoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kRelatedActiveContentsExist);
  browsing_instance_swap_result_ = browsing_instance_swap_result;
}

void BackForwardCacheCanStoreDocumentResult::NoDueToDisallowActivation(
    uint64_t reason) {
  AddNotRestoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict);
  disallow_activation_reasons_.insert(reason);
}

void BackForwardCacheCanStoreDocumentResult::NoDueToAXEvents(
    const std::vector<ui::AXEvent>& events) {
  DCHECK(base::FeatureList::IsEnabled(features::kEvictOnAXEvents));
  for (auto& event : events) {
    ax_events_.insert(event.event_type);
  }
  AddNotRestoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict);
  disallow_activation_reasons_.insert(DisallowActivationReasonId::kAXEvent);
}

void BackForwardCacheCanStoreDocumentResult::AddReasonsFrom(
    const BackForwardCacheCanStoreDocumentResult& other) {
  not_restored_reasons_.PutAll(other.not_restored_reasons_);
  for (const auto& [k, v] : other.blocking_details_map()) {
    for (const auto& details : v) {
      blocking_details_map_[k].push_back(details.Clone());
    }
  }
  for (const auto& reason : other.disabled_reasons()) {
    disabled_reasons_.insert(reason);
  }
  if (other.browsing_instance_swap_result_)
    browsing_instance_swap_result_ = other.browsing_instance_swap_result_;
  for (const auto reason : other.disallow_activation_reasons()) {
    disallow_activation_reasons_.insert(reason);
  }
  for (const auto event : other.ax_events()) {
    ax_events_.insert(event);
  }
}

BackForwardCacheCanStoreDocumentResult::
    BackForwardCacheCanStoreDocumentResult() = default;
BackForwardCacheCanStoreDocumentResult::BackForwardCacheCanStoreDocumentResult(
    BackForwardCacheCanStoreDocumentResult& other)
    : not_restored_reasons_(other.not_restored_reasons_),
      disabled_reasons_(other.disabled_reasons_),
      browsing_instance_swap_result_(other.browsing_instance_swap_result_),
      disallow_activation_reasons_(other.disallow_activation_reasons_),
      ax_events_(other.ax_events_) {
  // Manually copy `blocking_details_map_`.
  for (const auto& [k, v] : other.blocking_details_map()) {
    for (const auto& details : v) {
      blocking_details_map_[k].push_back(details.Clone());
    }
  }
}
BackForwardCacheCanStoreDocumentResult::BackForwardCacheCanStoreDocumentResult(
    BackForwardCacheCanStoreDocumentResult&&) = default;
BackForwardCacheCanStoreDocumentResult::
    ~BackForwardCacheCanStoreDocumentResult() = default;

}  // namespace content
