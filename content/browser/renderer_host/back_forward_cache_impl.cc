// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_impl.h"

#include <algorithm>
#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-shared.h"
#include "third_party/blink/public/mojom/script_source_location.mojom.h"
#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace content {

class RenderProcessHostInternalObserver;

// Allows overriding the sizes of back/forward cache.
// Sizes set via this feature's parameters take precedence over others.
// Enables BackForwardCache size for
//  - desktop: https://crbug.com/1291435.
//  - android: https://crbug.com/1395281.
BASE_FEATURE(kBackForwardCacheSize, base::FEATURE_ENABLED_BY_DEFAULT);
// Sets BackForwardCache cache_size=6.
const base::FeatureParam<int> kBackForwardCacheSizeCacheSize{
    &kBackForwardCacheSize, "cache_size", 6};
// Disables EnforceCacheSizeLimitInternal() with foreground_cache_size=0, as
// the BFCachePolicy manager takes care of pruning for foreground tabs as well.
const base::FeatureParam<int> kBackForwardCacheSizeForegroundCacheSize{
    &kBackForwardCacheSize, "foreground_cache_size", 0};

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;
using blink::scheduler::WebSchedulerTrackedFeatures;

// The default time to live in seconds for documents in BackForwardCache.
// See also crbug.com/1305878.
static constexpr int kDefaultTimeToLiveInBackForwardCacheInSeconds = 600;

#if BUILDFLAG(IS_ANDROID)
bool IsProcessBindingEnabled() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled()) {
    return false;
  }
  const std::string process_binding_param =
      base::GetFieldTrialParamValueByFeature(features::kBackForwardCache,
                                             "process_binding_strength");
  return process_binding_param.empty() || process_binding_param == "DISABLE";
}

// Association of ChildProcessImportance to corresponding string names.
const base::FeatureParam<ChildProcessImportance>::Option
    child_process_importance_options[] = {
        {ChildProcessImportance::IMPORTANT, "IMPORTANT"},
        {ChildProcessImportance::MODERATE, "MODERATE"},
        {ChildProcessImportance::PERCEPTIBLE, "PERCEPTIBLE"},
        {ChildProcessImportance::NORMAL, "NORMAL"}};

// Defines the binding strength for a processes holding cached pages. The value
// is read from an experiment parameter value. Ideally this would be lower than
// the one for processes holding the foreground page and similar to that of
// background tabs so that the OS will hopefully kill the foreground tab last.
// The default importance is set to MODERATE.
const base::FeatureParam<ChildProcessImportance> kChildProcessImportanceParam{
    &features::kBackForwardCache, "process_binding_strength",
    ChildProcessImportance::MODERATE, &child_process_importance_options};
#endif

WebSchedulerTrackedFeatures SupportedFeaturesImpl() {
  WebSchedulerTrackedFeatures features;
  if (!IsBackForwardCacheEnabled()) {
    return features;
  }

  static constexpr base::FeatureParam<std::string> supported_features(
      &features::kBackForwardCache, "supported_features", "");
  std::vector<std::string> tokens =
      base::SplitString(supported_features.Get(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& token : tokens) {
    auto feature = blink::scheduler::StringToFeature(token);
    if (feature.has_value()) {
      features.Put(feature.value());
    } else if (!blink::scheduler::IsRemovedFeature(token)) {
      // |feature| might not have its value when the user is in an experimental
      // group with the feature, and the feature is already removed.
      DLOG(WARNING) << "Invalid feature string: " << token;
    }
  }
  return features;
}

WebSchedulerTrackedFeatures SupportedFeatures() {
  static WebSchedulerTrackedFeatures features = SupportedFeaturesImpl();
  return features;
}

bool IgnoresOutstandingNetworkRequestForTesting() {
  if (!IsBackForwardCacheEnabled()) {
    return false;
  }
  static constexpr base::FeatureParam<bool>
      outstanding_network_request_supported(
          &features::kBackForwardCache,
          "ignore_outstanding_network_request_for_testing", false);
  return outstanding_network_request_supported.Get();
}

// Ignore all features that the page is using and all DisableForRenderFrameHost
// calls and force all pages to be cached. Should be used only for local testing
// and debugging -- things will break when this param is used.
bool ShouldIgnoreBlocklists() {
  if (!IsBackForwardCacheEnabled()) {
    return false;
  }
  static constexpr base::FeatureParam<bool> should_ignore_blocklists(
      &features::kBackForwardCache, "should_ignore_blocklists", false);
  return should_ignore_blocklists.Get();
}

// A list of WebSchedulerTrackedFeatures that always block back/forward
// cache. Some of these features are listed as blocking back/forward cache
// when actually the blocking is flag controlled and they are not registered
// as being used if we don't want them to block.
WebSchedulerTrackedFeatures GetDisallowedWebSchedulerTrackedFeatures() {
  return {WebSchedulerTrackedFeature::kBroadcastChannel,
          WebSchedulerTrackedFeature::kContainsPlugins,
          WebSchedulerTrackedFeature::kDummy,
          WebSchedulerTrackedFeature::kIdleManager,
          WebSchedulerTrackedFeature::kIndexedDBEvent,
          WebSchedulerTrackedFeature::kKeyboardLock,
          WebSchedulerTrackedFeature::kKeepaliveRequest,
          WebSchedulerTrackedFeature::kLiveMediaStreamTrack,
          WebSchedulerTrackedFeature::kPaymentManager,
          WebSchedulerTrackedFeature::kPictureInPicture,
          WebSchedulerTrackedFeature::kPrinting,
          WebSchedulerTrackedFeature::kRequestedAudioCapturePermission,
          WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors,
          WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission,
          WebSchedulerTrackedFeature::kRequestedMIDIPermission,
          WebSchedulerTrackedFeature::kRequestedVideoCapturePermission,
          WebSchedulerTrackedFeature::kSmartCard,
          WebSchedulerTrackedFeature::kSharedWorker,
          WebSchedulerTrackedFeature::kSpeechRecognizer,
          WebSchedulerTrackedFeature::kUnloadHandler,
          WebSchedulerTrackedFeature::kWebAuthentication,
          WebSchedulerTrackedFeature::kWebBluetooth,
          WebSchedulerTrackedFeature::kWebHID,
          WebSchedulerTrackedFeature::kWebLocks,
          WebSchedulerTrackedFeature::kWebOTPService,
          WebSchedulerTrackedFeature::kWebRTC,
          WebSchedulerTrackedFeature::kWebShare,
          WebSchedulerTrackedFeature::kWebSocket,
          WebSchedulerTrackedFeature::kWebTransport,
          WebSchedulerTrackedFeature::kWebXR,
          WebSchedulerTrackedFeature::kParserAborted,
          WebSchedulerTrackedFeature::kSharedWorkerMessage};
}
WebSchedulerTrackedFeatures GetInjectionWebSchedulerTrackedFeatures() {
  return {WebSchedulerTrackedFeature::kInjectedJavascript,
          WebSchedulerTrackedFeature::kInjectedStyleSheet};
}
WebSchedulerTrackedFeatures GetNetworkWebSchedulerTrackedFeatures() {
  return {WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers,
          WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch,
          WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR};
}
// A list of WebSchedulerTrackedFeatures that should never block back/forward
// cache unless the main frame has "Cache-Control: no-store" header.
WebSchedulerTrackedFeatures GetDisallowedForCacheControlNoStoreFeatures() {
  return {WebSchedulerTrackedFeature::kWebSocketSticky,
          WebSchedulerTrackedFeature::kWebRTCSticky,
          WebSchedulerTrackedFeature::kWebTransportSticky,
          WebSchedulerTrackedFeature::
              kJsNetworkRequestReceivedCacheControlNoStoreResource};
}
// A list of WebSchedulerTrackedFeatures that should never block back/forward
// cache.
WebSchedulerTrackedFeatures GetAllowedWebSchedulerTrackedFeatures() {
  return {
      WebSchedulerTrackedFeature::kDocumentLoaded,
      WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache,
      // This is handled in |UpdateCanStoreToIncludeCacheControlNoStore()|,
      // and no need to include in |GetDisallowedFeatures()|.
      WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore,
      // TODO(crbug.com/40236669): Figure out if these two should be allowed.
      WebSchedulerTrackedFeature::kOutstandingNetworkRequestDirectSocket,
      WebSchedulerTrackedFeature::kRequestedStorageAccessGrant,
      // We don't block on subresource cache-control:no-store or no-cache.
      WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache,
      WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore,
      // TODO(crbug.com/40236669): Figure out if this should be allowed.
      WebSchedulerTrackedFeature::kWebNfc,
  };
}

// WebSchedulerTrackedFeatures that do not affect back/forward cache, but
// affects other scheduling policies (e.g. aggressive throttling).
WebSchedulerTrackedFeatures
GetNonBackForwardCacheAffectingWebSchedulerTrackedFeatures() {
  return {WebSchedulerTrackedFeature::kWebSerial};
}

// The BackForwardCache feature is controlled via an experiment. This function
// returns the allowed URL list where it is enabled.
std::string GetAllowedURLList() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled()) {
    if (base::FeatureList::IsEnabled(
            kRecordBackForwardCacheMetricsWithoutEnabling)) {
      return base::GetFieldTrialParamValueByFeature(
          kRecordBackForwardCacheMetricsWithoutEnabling, "allowed_websites");
    }
    return "";
  }

  return base::GetFieldTrialParamValueByFeature(features::kBackForwardCache,
                                                "allowed_websites");
}

// This function returns the blocked URL list.
std::string GetBlockedURLList() {
  return IsBackForwardCacheEnabled()
             ? base::GetFieldTrialParamValueByFeature(
                   features::kBackForwardCache, "blocked_websites")
             : "";
}

// Returns the list of blocked CGI params
std::string GetBlockedCgiParams() {
  return IsBackForwardCacheEnabled()
             ? base::GetFieldTrialParamValueByFeature(
                   features::kBackForwardCache, "blocked_cgi_params")
             : "";
}

// Parses the “allowed_websites” and "blocked_websites" field trial parameters
// and creates a map to represent hosts and corresponding path prefixes.
base::flat_map<std::string, std::vector<std::string>> ParseCommaSeparatedURLs(
    std::string_view comma_separated_urls) {
  base::flat_map<std::string, std::vector<std::string>> urls;
  for (auto& it :
       base::SplitString(comma_separated_urls, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL)) {
    GURL url = GURL(it);
    urls[url.GetHost()].push_back(url.GetPath());
  }
  return urls;
}

// Parses the "cgi_params" field trial parameter into a set by splitting on "|".
base::flat_set<std::string> ParseBlockedCgiParams(
    std::string_view cgi_params_string) {
  return base::SplitString(cgi_params_string, "|", base::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

BackForwardCacheTestDelegate* g_bfcache_disabled_test_observer = nullptr;

void RestoreBrowserControlsState(RenderFrameHostImpl* cached_rfh) {
  auto* current_rfh =
      cached_rfh->frame_tree_node()->render_manager()->current_frame_host();

  DCHECK_NE(current_rfh, cached_rfh);

  float prev_top_controls_shown_ratio = current_rfh->GetRenderWidgetHost()
                                            ->render_frame_metadata_provider()
                                            ->LastRenderFrameMetadata()
                                            .top_controls_shown_ratio;
  if (prev_top_controls_shown_ratio < 1) {
    // Make sure the state in the restored renderer matches the current one.
    // If we currently aren't showing the controls let the cached renderer
    // know, so that it then reacts correctly to the SHOW controls message
    // that might follow during DidCommitNavigation.
    cached_rfh->GetPage().UpdateBrowserControlsState(
        cc::BrowserControlsState::kBoth, cc::BrowserControlsState::kHidden,
        // Do not animate as we want this to happen "instantaneously"
        false, std::nullopt);
  }
}

void RequestRecordTimeToVisible(RenderFrameHostImpl* rfh,
                                base::TimeTicks navigation_start) {
  // Make sure we record only when the frame is not in hidden state to avoid
  // cases like page navigating back with window.history.back(), while being
  // hidden.
  if (rfh->delegate()->GetVisibility() != Visibility::HIDDEN) {
    rfh->GetRenderWidgetHost()->GetVisibleTimeRequestTrigger().UpdateRequest(
        navigation_start, /*destination_is_loaded=*/false,
        /*show_reason_tab_switching=*/false,
        /*show_reason_bfcache_restore=*/true);
  }
}

// Returns true if any of the processes associated with the RenderViewHosts in
// this Entry are foregrounded.
bool HasForegroundedProcess(BackForwardCacheImpl::Entry& entry) {
  for (const auto& rvh : entry.render_view_hosts()) {
    if (rvh->GetProcess()->GetPriority() !=
        base::Process::Priority::kBestEffort) {
      return true;
    }
  }
  return false;
}

// Returns true if all of the RenderViewHosts in this Entry have received the
// acknowledgement from renderer.
bool AllRenderViewHostsReceivedAckFromRenderer(
    BackForwardCacheImpl::Entry& entry) {
  for (const auto& rvh : entry.render_view_hosts()) {
    if (!rvh->DidReceiveBackForwardCacheAck()) {
      return false;
    }
  }
  return true;
}

// Behavior on pages with cache-control:no-store specified by flags.
enum class CacheControlNoStoreExperimentLevel {
  // No experiments for cache-control:no-store are running.
  kDoNotStore = 1,
  // Only the metrics gathering experiment is on.
  kStoreAndEvictUponRestore = 2,
  // Restore the entry only when cookies have not changed while in cache.
  kStoreAndRestoreUnlessCookieChange = 3,
  // Restore the entry only when HTTP Only cookies have not changed while in
  // cache.
  kStoreAndRestoreUnlessHTTPOnlyCookieChange = 4,
};

const char kCacheControlNoStoreExperimentLevelName[] = "level";

static constexpr base::FeatureParam<CacheControlNoStoreExperimentLevel>::Option
    cache_control_levels[] = {
        {CacheControlNoStoreExperimentLevel::kStoreAndEvictUponRestore,
         "store-and-evict"},
        {CacheControlNoStoreExperimentLevel::kStoreAndRestoreUnlessCookieChange,
         "restore-unless-cookie-change"},
        {CacheControlNoStoreExperimentLevel::
             kStoreAndRestoreUnlessHTTPOnlyCookieChange,
         "restore-unless-http-only-cookie-change"},
};
const base::FeatureParam<CacheControlNoStoreExperimentLevel>
    cache_control_level{
        &features::kCacheControlNoStoreEnterBackForwardCache,
        kCacheControlNoStoreExperimentLevelName,
        CacheControlNoStoreExperimentLevel::kStoreAndRestoreUnlessCookieChange,
        &cache_control_levels};

CacheControlNoStoreExperimentLevel GetCacheControlNoStoreLevel() {
  if (!IsBackForwardCacheEnabled() ||
      !base::FeatureList::IsEnabled(
          features::kCacheControlNoStoreEnterBackForwardCache)) {
    return CacheControlNoStoreExperimentLevel::kDoNotStore;
  }
  return cache_control_level.Get();
}

const char kCacheControlNoStoreTimeToLiveName[] = "ttl";

// This param controls the TTL for pages with "Cache-Control: no-store".
const base::FeatureParam<base::TimeDelta> cache_control_no_store_ttl{
    &features::kCacheControlNoStoreEnterBackForwardCache,
    kCacheControlNoStoreTimeToLiveName, base::Minutes(3)};

base::TimeDelta GetCacheControlNoStoreTTL() {
  return cache_control_no_store_ttl.Get();
}

// Describes the behavior of BackForwardCacheImpl when handling the prioritized
// entry. The prioritized entry is the BFCache entry that will be kept alive
// even if it is out of the cache limit.
enum class BackForwardCachePrioritizedEntryExperimentLevel {
  // No special prioritization logic.
  kDoNotPrioritize = 0,
  // Allow one extra prioritized entry to avoid eviction.
  // If the cache limit enforced is 0 (e.g. due to critical memory pressure),
  // the prioritized entry will be evicted as well.
  kPrioritizeUnlessShouldClearAll = 1,
  // Allow one extra prioritized entry to avoid eviction.
  // If the cache limit enforced is 0 (e.g. due to critical memory pressure),
  // and there is no other entry to evict, the prioritized entry will be evicted
  // as well.
  kPrioritizeUnlessShouldClearAllAndNoEviction = 2,
};

const char kBackForwardCachePrioritizedEntryExperimentLevelName[] = "level";

static constexpr base::FeatureParam<
    BackForwardCachePrioritizedEntryExperimentLevel>::Option
    prioritized_entry_levels[] = {
        {BackForwardCachePrioritizedEntryExperimentLevel::kDoNotPrioritize,
         "do-not-prioritize"},
        {BackForwardCachePrioritizedEntryExperimentLevel::
             kPrioritizeUnlessShouldClearAll,
         "prioritize-unless-should-clear-all"},
        {BackForwardCachePrioritizedEntryExperimentLevel::
             kPrioritizeUnlessShouldClearAllAndNoEviction,
         "prioritize-unless-should-clear-all-and-no-eviction"},
};
const base::FeatureParam<BackForwardCachePrioritizedEntryExperimentLevel>
    prioritized_entry_level{
        &kBackForwardCachePrioritizedEntry,
        kBackForwardCachePrioritizedEntryExperimentLevelName,
        BackForwardCachePrioritizedEntryExperimentLevel::kDoNotPrioritize,
        &prioritized_entry_levels};

BackForwardCachePrioritizedEntryExperimentLevel
GetBackForwardCachePrioritizedEntryExperimentLevel() {
  if (!IsBackForwardCacheEnabled() ||
      !base::FeatureList::IsEnabled(kBackForwardCachePrioritizedEntry)) {
    return BackForwardCachePrioritizedEntryExperimentLevel::kDoNotPrioritize;
  }
  return prioritized_entry_level.Get();
}

bool IsSameOriginForTreeResult(RenderFrameHostImpl* rfh,
                               const url::Origin& main_document_origin) {
  // Treat any frame inside a fenced frame as cross origin so we don't leak
  // any information.
  if (rfh->IsNestedWithinFencedFrame()) {
    return false;
  }

  return rfh->GetLastCommittedOrigin().IsSameOriginWith(main_document_origin);
}

// Mark the result with No due to a single feature without JavaScript details.
void MarkNoWithSingleFeature(BackForwardCacheCanStoreDocumentResult* result,
                             WebSchedulerTrackedFeature feature) {
  BackForwardCacheCanStoreDocumentResult::BlockingDetailsMap map;
  auto details_ptr = blink::mojom::BlockingDetails::New();
  details_ptr->feature = feature;
  map[feature].push_back(std::move(details_ptr));
  result->NoDueToFeatures(std::move(map));
}

// Mark the result with No due to multiple features for `rfh`.
void MarkNoWithMultipleFeatures(BackForwardCacheCanStoreDocumentResult* result,
                                RenderFrameHostImpl* rfh,
                                WebSchedulerTrackedFeatures features) {
  BackForwardCacheCanStoreDocumentResult::BlockingDetailsMap map;
  WebSchedulerTrackedFeatures features_added;
  for (const auto& details : rfh->GetBackForwardCacheBlockingDetails()) {
    auto feature = static_cast<blink::scheduler::WebSchedulerTrackedFeature>(
        details->feature);
    // Some features might be recorded but not banned. Do not save the details
    // in this case.
    if (!features.Has(feature)) {
      continue;
    }
    map[feature].push_back(details.Clone());
    features_added.Put(feature);
  }
  result->NoDueToFeatures(std::move(map));
  DCHECK(features == features_added);
}

}  // namespace

// static
BlockListedFeatures BackForwardCacheImpl::GetAllowedFeatures(
    RequestedFeatures requested_features,
    CacheControlNoStoreContext ccns_context) {
  WebSchedulerTrackedFeatures result =
      Union(GetAllowedWebSchedulerTrackedFeatures(),
            GetNonBackForwardCacheAffectingWebSchedulerTrackedFeatures());
  result.PutAll(GetInjectionWebSchedulerTrackedFeatures());
  if (IgnoresOutstandingNetworkRequestForTesting()) {
    result.PutAll(GetNetworkWebSchedulerTrackedFeatures());
  }
  result.PutAll(SupportedFeatures());
  if (requested_features == RequestedFeatures::kOnlySticky) {
    // Add non-sticky disallowed features.
    WebSchedulerTrackedFeatures non_sticky =
        Difference(GetDisallowedWebSchedulerTrackedFeatures(),
                   blink::scheduler::StickyFeatures());
    if (!IgnoresOutstandingNetworkRequestForTesting()) {
      non_sticky.PutAll(Difference(GetNetworkWebSchedulerTrackedFeatures(),
                                   blink::scheduler::StickyFeatures()));
    }
    result.PutAll(non_sticky);
  }
  if (IsUnloadAllowed()) {
    result.Put(WebSchedulerTrackedFeature::kUnloadHandler);
  }
  // When not under "Cache-Control: no-store" context, the features listed in
  // `GetDisallowedForCacheControlNoStoreFeatures()` should be considered as
  // allowed features.
  if (ccns_context == kNotInCCNSContext) {
    result.PutAll(GetDisallowedForCacheControlNoStoreFeatures());
  }
  return result;
}

// static
BlockListedFeatures BackForwardCacheImpl::GetDisallowedFeatures(
    RequestedFeatures requested_features,
    CacheControlNoStoreContext ccns_context) {
  WebSchedulerTrackedFeatures result =
      GetDisallowedWebSchedulerTrackedFeatures();
  if (!IgnoresOutstandingNetworkRequestForTesting()) {
    result.PutAll(GetNetworkWebSchedulerTrackedFeatures());
  }
  result.RemoveAll(SupportedFeatures());
  if (requested_features == RequestedFeatures::kOnlySticky) {
    // Remove all non-sticky features from |result|.
    result = Intersection(result, blink::scheduler::StickyFeatures());
  }
  if (IsUnloadAllowed()) {
    result.Remove(WebSchedulerTrackedFeature::kUnloadHandler);
  }
  // When under "Cache-Control: no-store" context, the features listed in
  // `GetDisallowedForCacheControlNoStoreFeatures()` should be considered as
  // disallowed features.
  if (ccns_context == kInCCNSContext) {
    result.PutAll(GetDisallowedForCacheControlNoStoreFeatures());
  }
  return result;
}

// static
BackForwardCacheImpl::MessageHandlingPolicyWhenCached
BackForwardCacheImpl::GetChannelAssociatedMessageHandlingPolicy() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled()) {
    return kMessagePolicyNone;
  }

  static constexpr char kFieldTrialParam[] = "message_handling_when_cached";
  auto param = base::GetFieldTrialParamValueByFeature(
      features::kBackForwardCache, kFieldTrialParam);
  if (param.empty() || param == "log") {
    return kMessagePolicyLog;
  } else if (param == "none") {
    return kMessagePolicyNone;
  } else if (param == "dump") {
    return kMessagePolicyDump;
  } else {
    DLOG(WARNING) << "Failed to parse field trial param " << kFieldTrialParam
                  << " with string value " << param
                  << " under feature kBackForwardCache"
                  << features::kBackForwardCache.name;
    return kMessagePolicyLog;
  }
}

BackForwardCacheImpl::Entry::Entry(std::unique_ptr<StoredPage> stored_page)
    : stored_page_(std::move(stored_page)) {}

BackForwardCacheImpl::Entry::~Entry() = default;

void BackForwardCacheImpl::Entry::WriteIntoTrace(
    perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("render_frame_host", render_frame_host());
}

void BackForwardCacheImpl::RenderProcessPriorityChanged(
    RenderProcessHostImpl* host) {
  EnforceCacheSizeLimit();
}

BackForwardCacheTestDelegate::BackForwardCacheTestDelegate() {
  DCHECK(!g_bfcache_disabled_test_observer);
  g_bfcache_disabled_test_observer = this;
}

BackForwardCacheTestDelegate::~BackForwardCacheTestDelegate() {
  DCHECK_EQ(g_bfcache_disabled_test_observer, this);
  g_bfcache_disabled_test_observer = nullptr;
}

BackForwardCacheImpl::BackForwardCacheImpl(BrowserContext* browser_context)
    : allowed_urls_(ParseCommaSeparatedURLs(GetAllowedURLList())),
      blocked_urls_(ParseCommaSeparatedURLs(GetBlockedURLList())),
      blocked_cgi_params_(ParseBlockedCgiParams(GetBlockedCgiParams())),
      weak_factory_(this) {
  should_allow_storing_pages_with_cache_control_no_store_ =
      browser_context &&
      GetContentClient()
          ->browser()
          ->ShouldAllowBackForwardCacheForCacheControlNoStorePage(
              browser_context) &&
      GetCacheControlNoStoreLevel() >
          CacheControlNoStoreExperimentLevel::kDoNotStore;
}

BackForwardCacheImpl::~BackForwardCacheImpl() {
  Shutdown();
}

std::optional<int> GetFieldTrialParamByFeatureAsOptionalInt(
    const base::Feature& feature,
    const std::string& param_name) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  int value_as_int = 0;
  if (base::StringToInt(value_as_string, &value_as_int)) {
    return std::optional<int>(value_as_int);
  }
  return std::optional<int>();
}

base::TimeDelta BackForwardCacheImpl::GetTimeToLiveInBackForwardCache(
    CacheControlNoStoreContext ccns_context) {
  // We use the following order of priority if multiple values exist:
  // - The embedder-supplied time to live.
  // - The TTL set in `kBackForwardCacheTimeToLiveControl` takes precedence over
  //   the default value.
  // - Infinite if kBackForwardCacheNoTimeEviction is enabled.
  // - Default value otherwise, kDefaultTimeToLiveInBackForwardCacheInSeconds or
  // kDefaultTimeForCacheControlNoStorePageToLiveInBackForwardCacheInSeconds
  // depending on if the page's main frame has "Cache-Control: no-store" header
  // or not.
  if (embedder_supplied_time_to_live_.has_value()) {
    return embedder_supplied_time_to_live_.value();
  }

  if (base::FeatureList::IsEnabled(
          features::kBackForwardCacheTimeToLiveControl)) {
    std::optional<int> time_to_live = GetFieldTrialParamByFeatureAsOptionalInt(
        features::kBackForwardCacheTimeToLiveControl, "time_to_live_seconds");
    if (time_to_live.has_value()) {
      return base::Seconds(time_to_live.value());
    }
  }

  if (base::FeatureList::IsEnabled(kBackForwardCacheNoTimeEviction)) {
    return base::TimeDelta::Max();
  }

  if (ccns_context == kInCCNSContext) {
    return GetCacheControlNoStoreTTL();
  } else {
    return base::Seconds(kDefaultTimeToLiveInBackForwardCacheInSeconds);
  }
}

size_t BackForwardCacheImpl::GetCacheSize() {
  if (!IsBackForwardCacheEnabled()) {
    return 0;
  }

  if (embedder_supplied_cache_size_.has_value()) {
    return embedder_supplied_cache_size_.value();
  }

  if (base::FeatureList::IsEnabled(kBackForwardCacheSize)) {
    return kBackForwardCacheSizeCacheSize.Get();
  }

  return 0;
}

size_t BackForwardCacheImpl::GetForegroundedEntriesCacheSize() {
  if (!IsBackForwardCacheEnabled()) {
    return 0;
  }

  if (embedder_supplied_cache_size_.has_value()) {
    // If the embedder supplied a limit (which should affect `GetCacheSize()`),
    // don't use a foreground-specific limit.
    return 0;
  }

  if (base::FeatureList::IsEnabled(kBackForwardCacheSize)) {
    return kBackForwardCacheSizeForegroundCacheSize.Get();
  }
  return 0;
}

bool BackForwardCacheImpl::UsingForegroundBackgroundCacheSizeLimit() {
  return GetForegroundedEntriesCacheSize() > 0;
}

BackForwardCacheImpl::Entry* BackForwardCacheImpl::FindMatchingEntry(
    PageImpl& page) {
  RenderFrameHostImpl* render_frame_host = &page.GetMainDocument();
  Entry* matching_entry = nullptr;
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (render_frame_host == entry->render_frame_host()) {
      matching_entry = entry.get();
      break;
    }
  }
  return matching_entry;
}

void BackForwardCacheImpl::UpdateCanStoreToIncludeCacheControlNoStore(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* render_frame_host) {
  // If the feature is disabled, do nothing.
  if (!should_allow_storing_pages_with_cache_control_no_store()) {
    return;
  }
  // If the page didn't have cache-control: no-store, do nothing.
  if (!render_frame_host->LoadedWithCacheControlNoStoreHeader()) {
    return;
  }

  // Note that kCacheControlNoStoreHTTPOnlyCookieModified,
  // kCacheControlNoStoreCookieModified and kCacheControlNoStore are mutually
  // exclusive.
  if (render_frame_host->IsDeviceBoundSessionTerminated() &&
      base::FeatureList::IsEnabled(
          features::kDeviceBoundSessionTerminationEvictBackForwardCache)) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::
                  kCacheControlNoStoreDeviceBoundSessionTerminated);
    return;
  }

  const RenderFrameHostImpl::CookieChangeListener::CookieChangeInfo&
      cookie_change_info = render_frame_host->GetCookieChangeInfo();
  const std::string kCookieCHangeInfoMetricName =
      "BackForwardCache.CCNS.CookieChangeInfo.";
  base::UmaHistogramCounts1000(
      base::StrCat({kCookieCHangeInfoMetricName, "AllCookies"}),
      cookie_change_info.cookie_modification_count);
  base::UmaHistogramCounts1000(
      base::StrCat(
          {kCookieCHangeInfoMetricName, "AllCookiesFromMainFrameNavigation"}),
      cookie_change_info.cookie_modification_removing_count);
  base::UmaHistogramCounts1000(
      base::StrCat({kCookieCHangeInfoMetricName, "HttpOnlyCookies"}),
      cookie_change_info.http_only_cookie_modification_count);
  base::UmaHistogramCounts1000(
      base::StrCat({kCookieCHangeInfoMetricName,
                    "HttpOnlyCookiesFromMainFrameNavigation"}),
      cookie_change_info.http_only_cookie_modification_removing_count);

  if (cookie_change_info.http_only_cookie_modification_count >
      cookie_change_info.http_only_cookie_modification_removing_count) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::
                  kCacheControlNoStoreHTTPOnlyCookieModified);
  } else if (cookie_change_info.cookie_modification_count >
             cookie_change_info.cookie_modification_removing_count) {
    // JavaScript cookies are modified but not HTTP cookies. Only restore based
    // on the experiment level.
    if (GetCacheControlNoStoreLevel() <=
        CacheControlNoStoreExperimentLevel::
            kStoreAndRestoreUnlessCookieChange) {
      result.No(BackForwardCacheMetrics::NotRestoredReason::
                    kCacheControlNoStoreCookieModified);
    }
  } else if (GetCacheControlNoStoreLevel() ==
             CacheControlNoStoreExperimentLevel::kStoreAndEvictUponRestore) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kCacheControlNoStore);
  }
}

namespace {
void LogAndTraceResult(
    const RenderFrameHostImpl& rfh,
    const BackForwardCacheCanStoreDocumentResult& flattened_result,
    const perfetto::StaticString& caller) {
  VLOG(1) << caller.value << ": " << rfh.GetLastCommittedURL() << " : "
          << flattened_result.ToString();
  TRACE_EVENT("navigation", caller,
              ChromeTrackEvent::kBackForwardCacheCanStoreDocumentResult,
              flattened_result);
}
}  // namespace

BackForwardCacheCanStoreDocumentResultWithTree
BackForwardCacheImpl::GetCurrentBackForwardCacheEligibility(
    RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult flattened;
  auto result =
      PopulateReasonsForPage(rfh, flattened, RequestedFeatures::kAllIfAcked);
  LogAndTraceResult(
      *rfh, result.flattened_reasons,
      "BackForwardCacheImpl::GetCurrentBackForwardCacheEligibility");
  return result;
}

BackForwardCacheCanStoreDocumentResultWithTree
BackForwardCacheImpl::GetCompleteBackForwardCacheEligibilityForReporting(
    RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult flattened;
  auto result = PopulateReasonsForPage(rfh, flattened, RequestedFeatures::kAll);
  LogAndTraceResult(*rfh, result.flattened_reasons,
                    "BackForwardCacheImpl::"
                    "GetCompleteBackForwardCacheEligibilityForReporting");
  return result;
}

BackForwardCacheCanStoreDocumentResultWithTree
BackForwardCacheImpl::GetFutureBackForwardCacheEligibilityPotential(
    RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult flattened;
  auto result =
      PopulateReasonsForPage(rfh, flattened, RequestedFeatures::kOnlySticky);
  LogAndTraceResult(
      *rfh, result.flattened_reasons,
      "BackForwardCacheImpl::GetFutureBackForwardCacheEligibilityPotential");
  return result;
}

BackForwardCacheCanStoreDocumentResultWithTree
BackForwardCacheImpl::PopulateReasonsForPage(
    RenderFrameHostImpl* rfh,
    BackForwardCacheCanStoreDocumentResult& flattened_result,
    RequestedFeatures requested_features) {
  // TODO(crbug.com/40207294): This function should only be called when |rfh| is
  // the primary main frame. Fix |ShouldProactivelySwapBrowsingInstance()| and
  // |UnloadOldFrame()| so that it will not check bfcache eligibility if not
  // primary main frame.
  BackForwardCacheCanStoreDocumentResult main_document_specific_result;
  // This function can be called during eviction, and |rfh| can be in
  // back/forward cache, which is considered as non primary main frame.
  bool main_frame_in_bfcache =
      rfh->IsInBackForwardCache() && !rfh->GetParentOrOuterDocumentOrEmbedder();

  // Call the recursive function that adds the reasons from the subtree to the
  // flattened list, and return the tree if needed.
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> result_tree;
  if (!rfh->IsInPrimaryMainFrame() && !main_frame_in_bfcache) {
    // When |rfh| is not the primary main frame and is not the bfcache main
    // frame, e.g. when |rfh| is prerendering, fenced frame root or is not the
    // main frame, we can reach this block.
    // We do not need to check the subframes' reasons because callers that reach
    // here only care about whether can_store is true or false, not about the
    // reasons.
    main_document_specific_result.No(
        BackForwardCacheMetrics::NotRestoredReason::kNotPrimaryMainFrame);
    result_tree = BackForwardCacheCanStoreTreeResult::CreateEmptyTree(rfh);
  } else {
    // Populate main document specific reasons.
    PopulateReasonsForMainDocument(main_document_specific_result, rfh);
    NotRestoredReasonBuilder builder(rfh, requested_features);
    result_tree = builder.GetTreeResult();
    flattened_result.AddReasonsFrom(builder.GetFlattenedResult());
  }
  // Add the reasons for main document to the flattened list.
  flattened_result.AddReasonsFrom(main_document_specific_result);
  // |result_tree| does not have main document specific reasons such as
  // "disabled via command line", and we have to manually add them.
  result_tree->AddReasonsToSubtreeRootFrom(main_document_specific_result);
  DCHECK_EQ(flattened_result, result_tree->FlattenTree());
  return BackForwardCacheCanStoreDocumentResultWithTree(flattened_result,
                                                        std::move(result_tree));
}

void BackForwardCacheImpl::PopulateReasonsForMainDocument(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* rfh) {
  bool main_frame_in_bfcache =
      rfh->IsInBackForwardCache() && !rfh->GetParentOrOuterDocumentOrEmbedder();
  DCHECK(rfh->IsInPrimaryMainFrame() || main_frame_in_bfcache);

  // If the the delegate doesn't support back forward cache, disable it.
  if (!rfh->delegate()->IsBackForwardCacheSupported()) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::
                  kBackForwardCacheDisabledForDelegate);
  }

  if (!IsBackForwardCacheEnabled() || is_disabled_for_testing_) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kBackForwardCacheDisabled);

    // In addition to the general "BackForwardCacheDisabled" reason above, also
    // track more specific reasons on why BackForwardCache is disabled.
    if (IsBackForwardCacheDisabledByCommandLine()) {
      result.No(BackForwardCacheMetrics::NotRestoredReason::
                    kBackForwardCacheDisabledByCommandLine);
    }

    if (!DeviceHasEnoughMemoryForBackForwardCache()) {
      result.No(BackForwardCacheMetrics::NotRestoredReason::
                    kBackForwardCacheDisabledByLowMemory);
    }
  }

  // If this function is called after we navigated to a new RenderFrameHost,
  // then |rfh| must already be replaced by the new RenderFrameHost. If this
  // function is called before we navigated, then |rfh| must be an active
  // RenderFrameHost.
  bool is_active_rfh = rfh->IsActive();

  // Two pages in the same BrowsingInstance can script each other. When a page
  // can be scripted from outside, it can't enter the BackForwardCache.
  //
  // This check makes sure the old and new document aren't sharing the same
  // BrowsingInstance. Note that the existence of related active contents might
  // change in the future, but we are checking this in
  // GetFutureBackForwardCacheEligibilityPotential instead of
  // GetCurrentBackForwardCacheEligibility because it's needed to determine
  // whether to do a proactive BrowsingInstance swap or not, which should not be
  // done if the page has related active contents.
  // TODO(crbug.com/40922919): The check below prevents usage of the
  // BackForwardCache for navigations that result in a browsing context group
  // swap in the same CoopRelatedGroup. The check below should probably be
  // adapted, to allow usage of the BackForwardCache in those cases.
  //
  // If the `rfh` is still the "active" RenderFrameHost, then it will be
  // included in the "related active contents" count, so we expect the count to
  // be 1 when there's no other related active contents. When `rfh` is no longer
  // an active RenderFrameHost, it means another RenderFrameHost had taken its
  // place as the primary main frame. The new RenderFrameHost might reuse the
  // same BrowsingInstance as `rfh` though, so we should account for that being
  // included in the related active contents count, to not correctly misclassify
  // the case as "not BFCached due to related active contents" (which is
  // reserved for cases where there are active pages in other WebContents in the
  // same BrowsingInstance).
  unsigned expected_related_active_contents_count = is_active_rfh ? 1 : 0;
  if (!is_active_rfh) {
    auto* current_rfh =
        rfh->frame_tree_node()->render_manager()->current_frame_host();
    if (current_rfh->GetSiteInstance()->IsRelatedSiteInstance(
            rfh->GetSiteInstance())) {
      // A new RenderFrameHost replaced `rfh` as the primary main frame, but
      // uses the same BrowsingInstance. Currently we cannot BFCache this case
      // because this means we did not do a proactive BrowsingInstance swap.
      result.No(BackForwardCacheMetrics::NotRestoredReason::
                    kBrowsingInstanceNotSwapped);
      expected_related_active_contents_count++;
    }
  }

  // We should never have fewer than expected.
  DCHECK_GE(rfh->GetSiteInstance()->GetRelatedActiveContentsCount(),
            expected_related_active_contents_count);
  if (rfh->GetSiteInstance()->GetRelatedActiveContentsCount() >
      expected_related_active_contents_count) {
    std::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result;
    if (auto* metrics = rfh->GetBackForwardCacheMetrics()) {
      browsing_instance_swap_result = metrics->browsing_instance_swap_result();
    }
    result.NoDueToRelatedActiveContents(browsing_instance_swap_result);
  }

  // Only store documents that have successful http status code.
  // Note that for error pages, |last_http_status_code| is equal to 0.
  if (rfh->last_http_status_code() != net::HTTP_OK) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK);
  }

  // Interstitials and other internal error pages should set an error status
  // code but there's no guarantee, e.g. https://crbug/1274308,
  // https://crbug/1287996. This catches those cases. It might also make the
  // kHTTPStatusNotOK check redundant.
  if (rfh->IsErrorDocument()) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kErrorDocument);
  }

  // Only store documents that were fetched via HTTP GET method.
  if (rfh->last_http_method() != net::HttpRequestHeaders::kGetMethod) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPMethodNotGET);
  }

  // Do not store main document with non HTTP/HTTPS URL scheme. Among other
  // things, this excludes the new tab page and all WebUI pages.
  if (!rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kSchemeNotHTTPOrHTTPS);
  }

  // We should not cache pages with Cache-control: no-store. Note that
  // even though this is categorized as a "feature", we will check this within
  // GetFutureBackForwardCacheEligibilityPotential as it's not possible to
  // change the HTTP headers, so if it's not possible to cache this page now due
  // to this, it's impossible to cache this page later.
  if (rfh->LoadedWithCacheControlNoStoreHeader()) {
    if (!should_allow_storing_pages_with_cache_control_no_store()) {
      // Block pages with cache-control: no-store when
      // |should_cache_control_no_store_enter| flag is false.
      MarkNoWithSingleFeature(
          &result,
          WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
    } else {
      // Even if `should_cache_control_no_store_enter` is true, we may still
      // block pages with cache-control: no-store if the cookie is disabled.
      // This is because the site is likely to rely on schemes other than
      // cookies to store user credentials.
      // Note that this only covers the case that the cookie is already disabled
      // when the reasons are being populated. If the cookie is disabled after
      // the procedure, it's still possible for the pages with cache-control: no
      // store to be BFCached.
      if (!rfh->IsFullCookieAccessAllowed()) {
        result.No(
            BackForwardCacheMetrics::NotRestoredReason::kCacheControlNoStore);
        result.No(BackForwardCacheMetrics::NotRestoredReason::kCookieDisabled);
      }
    }
  }

  // Only store documents that have URLs allowed through experiment.
  if (!IsAllowed(rfh->GetLastCommittedURL())) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kDomainNotAllowed);
  }
}

void BackForwardCacheImpl::NotRestoredReasonBuilder::
    PopulateStickyReasonsForDocument(
        BackForwardCacheCanStoreDocumentResult& result,
        RenderFrameHostImpl* rfh) {
  if (rfh->IsBackForwardCacheDisabled() && !ShouldIgnoreBlocklists()) {
    result.NoDueToDisableForRenderFrameHostCalled(
        rfh->back_forward_cache_disabled_reasons());
  }

  // Do not store documents if they have inner WebContents. Inner frame trees
  // that are based on MPArch are allowed to be stored. To determine if this
  // is an inner WebContents we check the inner frame tree's type to see if
  // it is `kPrimary`.
  // We also make MPArch based GuestViews ineligible. While supporting them
  // is probably feasible, other than the case of https://crbug.com/330282443 ,
  // there is little benefit to doing so.
  if ((rfh->frame_tree()->delegate()->GetOuterDelegateFrameTreeNodeId() &&
       rfh->frame_tree()->is_primary()) ||
      rfh->frame_tree()->is_guest()) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHaveInnerContents);
  }

  if (!IsUnloadAllowed()) {
    const bool has_unload_handler = rfh->has_unload_handler();
    if (has_unload_handler) {
      // Note that pages with unload handlers are cached on android.
      result.No(rfh->GetParent() ? BackForwardCacheMetrics::NotRestoredReason::
                                       kUnloadHandlerExistsInSubFrame
                                 : BackForwardCacheMetrics::NotRestoredReason::
                                       kUnloadHandlerExistsInMainFrame);
    }
  }

  // When it's not the final decision for putting a page in the back-forward
  // cache, we should only consider "sticky" features here - features that
  // will always result in a page becoming ineligible for back-forward cache
  // since the first time it's used.
  // If the main document had CCNS and this document is same-origin with the
  // main document, we should consider the features that are only disallowed for
  // CCNS documents.
  // This does not use `IsSameOriginForTreeResult` because we
  // want to be more conservative and react to *any* same-origin frame using it.
  CacheControlNoStoreContext ccns_context = kNotInCCNSContext;
  if (root_rfh_->LoadedWithCacheControlNoStoreHeader() &&
      rfh->GetLastCommittedOrigin().IsSameOriginWith(
          root_rfh_->GetLastCommittedOrigin())) {
    ccns_context = kInCCNSContext;
  }
  WebSchedulerTrackedFeatures banned_features = Intersection(
      GetDisallowedFeatures(RequestedFeatures::kOnlySticky, ccns_context),
      rfh->GetBackForwardCacheDisablingFeatures());
  // If the document has any features that are only disallowed for CCNS,
  // we should explicitly record `kMainResourceHasCacheControlNoStore`
  // as well.
  if (!Intersection(banned_features,
                    GetDisallowedForCacheControlNoStoreFeatures())
           .empty()) {
    banned_features.Put(
        WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
    // Record the feature usage in `rfh`. This is needed because all
    // `banned_features` have to be recorded in `rfh` so that the blocking
    // details are also recorded.
    // Call `RecordBackForwardCacheDisablingReason()` instead of
    // `OnBackForwardCacheDisablingStickyFeatureUsed()` so that it won't
    // accidentally trigger eviction of rfh, which would otherwise be a
    // confusing side effect of this function.
    rfh->RecordBackForwardCacheDisablingReason(
        WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
  }
  if (!banned_features.empty()) {
    if (!ShouldIgnoreBlocklists()) {
      MarkNoWithMultipleFeatures(&result, rfh, banned_features);
    }
  }
}

void BackForwardCacheImpl::NotRestoredReasonBuilder::
    PopulateNonStickyReasonsForDocument(
        BackForwardCacheCanStoreDocumentResult& result,
        RenderFrameHostImpl* rfh,
        RequestedFeatures requested_features) {
  DCHECK_NE(requested_features, RequestedFeatures::kOnlySticky);
  // The DOM content must have finished loading, except when there is
  // no DOM content to load when the RFH has not committed any navigation.
  if (!rfh->IsDOMContentLoaded() && rfh->has_committed_any_navigation()) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kLoading);
  }

  // Check for non-sticky features that are present at the moment, ignoring CCNS
  // related logic since they will be handled separately.
  WebSchedulerTrackedFeatures banned_features = Intersection(
      GetDisallowedFeatures(RequestedFeatures::kAll, kNotInCCNSContext),
      rfh->GetBackForwardCacheDisablingFeatures());
  if (!banned_features.empty() && !ShouldIgnoreBlocklists()) {
    if (requested_features == RequestedFeatures::kAll ||
        (requested_features == RequestedFeatures::kAllIfAcked &&
         rfh->render_view_host()->DidReceiveBackForwardCacheAck())) {
      MarkNoWithMultipleFeatures(&result, rfh, banned_features);
    }
  }

  // Handle ongoing navigations in subframes.
  // - We allow the following cases to be cached:
  //   - 1) Subframe navigations that don't need URLLoaders and haven't reached
  //        the pending commit stage.
  //   - 2) Subframe navigations that need URLLoaders and haven't sent any
  //        network requests.
  //
  // If there are other type of navigations in any of the subframes, we disallow
  // BFCache.
  if (rfh->GetParentOrOuterDocument()) {
    NavigationRequest* nav_request =
        rfh->frame_tree_node()->navigation_request();
    // Prevent BFCache if the navigation needs a URLLoader and already sent a
    // network request. It is not enough to check that URLLoader exists,
    // because it is reset when the request receives its response, so we must
    // check if navigation state has already passed `WillStartRequest` to
    // cover the navigations between sending request and starting commit.
    if ((nav_request && nav_request->NeedsUrlLoader() &&
         (nav_request->HasLoader() ||
          nav_request->state() >
              NavigationRequest::NavigationState::WILL_START_REQUEST)) ||
        rfh->frame_tree_node()->HasPendingCommitNavigation()) {
      result.No(
          BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating);
    }
  }

  if (base::FeatureList::IsEnabled(blink::features::kBFCacheWithSharedWorker)) {
    // If this frame is a client of a Shared Worker and all other clients are in
    // this frame's frame-tree, block this frame from entering BFCache. Also,
    // other clients of the same worker that are already in BFCache will be
    // evicted.
    // TODO(crbug.com/406420935): Freeze the worker instead of blocking BFCache.

    // If rfh is already in BFCache then we are evicting it and there is no need
    // to check if it's the last active client.
    if (!rfh->IsInBackForwardCache()) {
      SharedWorkerServiceImpl* service = static_cast<SharedWorkerServiceImpl*>(
          rfh->GetStoragePartition()->GetSharedWorkerService());
      if (service && service->EvictBFCachedClientsIfLastActive(rfh)) {
        result.No(BackForwardCacheMetrics::NotRestoredReason::
                      kSharedWorkerWithNoActiveClient);
      }
    }
  }
}

void BackForwardCacheImpl::NotRestoredReasonBuilder::PopulateReasonsForDocument(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* rfh,
    RequestedFeatures requested_features) {
  PopulateStickyReasonsForDocument(result, rfh);
  if (requested_features != RequestedFeatures::kOnlySticky) {
    PopulateNonStickyReasonsForDocument(result, rfh, requested_features);
  }
}

BackForwardCacheCanStoreDocumentResultWithTree
BackForwardCacheImpl::CreateEvictionBackForwardCacheCanStoreTreeResult(
    RenderFrameHostImpl& rfh,
    BackForwardCacheCanStoreDocumentResult& eviction_reason) {
  // At this point the page already has some NotRestoredReasons for eviction, so
  // we should always record cache_control:no-store related reasons.
  BackForwardCacheImpl::NotRestoredReasonBuilder builder(
      rfh.GetOutermostMainFrame(),
      /* requested_features = */ RequestedFeatures::kOnlySticky,
      BackForwardCacheImpl::NotRestoredReasonBuilder::EvictionInfo(
          rfh, &eviction_reason));

  BackForwardCacheCanStoreDocumentResult flattened_result =
      builder.GetFlattenedResult();
  LogAndTraceResult(
      rfh, flattened_result,
      "BackForwardCacheImpl::CreateEvictionBackForwardCacheCanStoreTreeResult");
  return BackForwardCacheCanStoreDocumentResultWithTree(
      flattened_result, builder.GetTreeResult());
}

BackForwardCacheImpl::NotRestoredReasonBuilder::NotRestoredReasonBuilder(
    RenderFrameHostImpl* root_rfh,
    RequestedFeatures requested_features)
    : NotRestoredReasonBuilder(root_rfh,
                               requested_features,
                               /* eviction_info = */ std::nullopt) {}

BackForwardCacheImpl::NotRestoredReasonBuilder::NotRestoredReasonBuilder(
    RenderFrameHostImpl* root_rfh,
    RequestedFeatures requested_features,
    std::optional<EvictionInfo> eviction_info)
    : root_rfh_(root_rfh),
      bfcache_(root_rfh_->frame_tree_node()
                   ->navigator()
                   .controller()
                   .GetBackForwardCache()),
      requested_features_(requested_features),
      eviction_info_(eviction_info) {
  // |root_rfh_| should be either primary main frame or back/forward cached
  // page's outermost main frame.
  DCHECK(root_rfh_->IsInPrimaryMainFrame() ||
         (root_rfh_->IsInBackForwardCache() &&
          !root_rfh_->GetParentOrOuterDocumentOrEmbedder()));
  // Populate the reasons and build the tree.
  std::map<RenderFrameHostImpl*, BackForwardCacheCanStoreTreeResult*>
      parent_map;
  root_rfh_->ForEachRenderFrameHostImpl([&](RenderFrameHostImpl* rfh) {
    auto rfh_result = PopulateReasons(rfh);
    parent_map[rfh] = rfh_result.get();

    if (rfh == root_rfh_) {
      tree_result_ = std::move(rfh_result);
    } else {
      RenderFrameHostImpl* parent = rfh->GetParentOrOuterDocumentOrEmbedder();
      DCHECK(parent);
      parent_map[parent]->AppendChild(std::move(rfh_result));
    }
  });
}

BackForwardCacheImpl::NotRestoredReasonBuilder::~NotRestoredReasonBuilder() =
    default;

std::unique_ptr<BackForwardCacheCanStoreTreeResult>
BackForwardCacheImpl::NotRestoredReasonBuilder::PopulateReasons(
    RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult result_for_rfh;
  if (eviction_info_.has_value()) {
    // When |eviction_info_| is set, that means that we are populating the
    // reasons for eviction. In that case, we do not need to check each frame's
    // eligibility, but only mark |rfh_to_be_evicted| with |reasons|, as it is
    // the cause of eviction.
    if (rfh == eviction_info_->rfh_to_be_evicted) {
      result_for_rfh.AddReasonsFrom(*(eviction_info_->reasons));
    }
  } else {
    // Populate |result_for_rfh| by checking the bfcache eligibility of |rfh|.
    PopulateReasonsForDocument(result_for_rfh, rfh, requested_features_);
  }
  bfcache_->UpdateCanStoreToIncludeCacheControlNoStore(result_for_rfh, rfh);
  flattened_result_.AddReasonsFrom(result_for_rfh);

  std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree(
      new BackForwardCacheCanStoreTreeResult(
          rfh, root_rfh_->GetLastCommittedOrigin(), rfh->GetLastCommittedURL(),
          result_for_rfh));
  return tree;
}

void BackForwardCacheImpl::StoreEntry(
    std::unique_ptr<BackForwardCacheImpl::Entry> entry) {
  TRACE_EVENT("navigation", "BackForwardCache::StoreEntry", "entry", entry);
  DCHECK(GetCurrentBackForwardCacheEligibility(entry->render_frame_host())
             .CanStore());

#if BUILDFLAG(IS_ANDROID)
  if (!IsProcessBindingEnabled()) {
    // Set the priority of the main frame on entering the back-forward cache to
    // make sure the page gets evicted instead of foreground tab. This might not
    // become the effective priority of the process if it owns other higher
    // priority RenderWidgetHost. We don't need to reset the priority in
    // RestoreEntry as it is taken care by WebContentsImpl::NotifyFrameSwapped
    // on restoration.
    RenderWidgetHostImpl* rwh =
        entry->render_frame_host()->GetRenderWidgetHost();
    ChildProcessImportance current_importance = rwh->importance();
    rwh->SetImportance(
        std::min(current_importance, kChildProcessImportanceParam.Get()));
  }
#endif

  entry->render_frame_host()->DidEnterBackForwardCache();
  entry->SetStoredPageDelegate(this);
  entries_.push_front(std::move(entry));
  AddProcessesForEntry(*entries_.front());
  EnforceCacheSizeLimit();
}

void BackForwardCacheImpl::EnforceCacheSizeLimit() {
  if (!IsBackForwardCacheEnabled()) {
    return;
  }

  if (UsingForegroundBackgroundCacheSizeLimit()) {
    // First enforce the foregrounded limit. The idea is that we need to
    // strictly enforce the limit on pages using foregrounded processes because
    // Android will not kill a foregrounded process, however it will kill a
    // backgrounded process if there is memory pressure, so we can allow more of
    // those to be kept in the cache.
    EnforceCacheSizeLimitInternal(
        GetForegroundedEntriesCacheSize(),
        BackForwardCacheMetrics::NotRestoredReason::kForegroundCacheLimit);
  }
  EnforceCacheSizeLimitInternal(
      GetCacheSize(), BackForwardCacheMetrics::NotRestoredReason::kCacheLimit);
}

size_t BackForwardCacheImpl::Prune(size_t limit, NotRestoredReason reason) {
  return EnforceCacheSizeLimitInternal(limit, reason);
}

size_t BackForwardCacheImpl::EnforceCacheSizeLimitInternal(
    size_t limit,
    BackForwardCacheMetrics::NotRestoredReason reason) {
  using NotRestoredReason = BackForwardCacheMetrics::NotRestoredReason;
  using Level = BackForwardCachePrioritizedEntryExperimentLevel;

  size_t count = 0;
  bool did_evict_any_entry = false;
  auto prioritized_level = GetBackForwardCachePrioritizedEntryExperimentLevel();
  // Indicates whether we should skip the initial rounds of eviction (the
  // for-loop below). Note that even if this boolean value is true, the
  // prioritized entry may still be evicted if the level is
  // `kPrioritizeUnlessShouldClearAllAndNoEviction` and there is no other
  // eviction.
  bool should_skip_eviction_for_prioritized_entry =
      (prioritized_level == Level::kPrioritizeUnlessShouldClearAll &&
       limit > 0) ||
      prioritized_level == Level::kPrioritizeUnlessShouldClearAllAndNoEviction;

  for (auto stored_entry_iter = entries_.begin();
       stored_entry_iter != entries_.end(); stored_entry_iter++) {
    Entry* stored_entry = stored_entry_iter->get();
    RenderFrameHostImpl* rfh = stored_entry->render_frame_host();
    // Skip the entry if it's already evicted.
    if (rfh->is_evicted_from_back_forward_cache()) {
      continue;
    }
    // Skip the entry if it doesn't have foregrounded progress and the eviction
    // is against foreground limit.
    if (reason == NotRestoredReason::kForegroundCacheLimit &&
        !HasForegroundedProcess(*stored_entry)) {
      continue;
    }
    // Skip the entry if any of the RenderViewHosts in this Entry haven't
    // received the BFCache acknowledgement yet. The current method will be
    // called again once the acknowledgements are all received later.
    // Note: this only applies to the case where the latest entry triggers the
    // cache limit, for the pruning case, the entry will be counted and might be
    // evicted.
    if (reason !=
            NotRestoredReason::kCacheLimitPrunedOnModerateMemoryPressure &&
        reason !=
            NotRestoredReason::kCacheLimitPrunedOnCriticalMemoryPressure &&
        !AllRenderViewHostsReceivedAckFromRenderer(*stored_entry)) {
      continue;
    }

    if (++count > limit) {
      if (should_skip_eviction_for_prioritized_entry) {
        // Handle the latest prioritized entry outside the limit specially and
        // keep it not-evicted.
        if (GetContentClient()->browser()->ShouldPrioritizeForBackForwardCache(
                rfh->GetBrowserContext(), rfh->GetLastCommittedURL())) {
          // If the prioritized_entry_ is already taken by some other entry,
          // we have to evict the current one.
          if (prioritized_entry_ == entries_.end() ||
              prioritized_entry_ == stored_entry_iter) {
            prioritized_entry_ = stored_entry_iter;
            continue;
          }
        }
      }
      did_evict_any_entry = true;
      rfh->EvictFromBackForwardCacheWithReason(reason);
    }
  }

  // The `prioritized_entry_` should be evicted if
  if (
      // the prioritized entry is set
      prioritized_entry_ != entries_.end() &&
      // the cache limit is 0 (e.g. due to critical memory pressure)
      limit == 0 &&
      // the level is `prioritize-unless-should-clear-all-and-no-eviction`
      prioritized_level ==
          Level::kPrioritizeUnlessShouldClearAllAndNoEviction &&
      // no other entry was evicted
      !did_evict_any_entry) {
    prioritized_entry_->get()
        ->render_frame_host()
        ->EvictFromBackForwardCacheWithReason(reason);
    prioritized_entry_ = entries_.end();
  }
  return count;
}

void BackForwardCacheImpl::SetEmbedderSuppliedCacheSize(
    size_t embedder_supplied_cache_size) {
  if (embedder_supplied_cache_size == GetCacheSize()) {
    return;
  }
  embedder_supplied_cache_size_ = embedder_supplied_cache_size;
  EnforceCacheSizeLimit();
}

void BackForwardCacheImpl::SetEmbedderSuppliedTimeToLive(
    base::TimeDelta embedder_supplied_time_to_live) {
  if (embedder_supplied_time_to_live ==
      GetTimeToLiveInBackForwardCache(
          CacheControlNoStoreContext::kNotInCCNSContext)) {
    return;
  }
  embedder_supplied_time_to_live_ = embedder_supplied_time_to_live;
  Flush();
}

std::unique_ptr<BackForwardCacheImpl::Entry> BackForwardCacheImpl::RestoreEntry(
    int navigation_entry_id,
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  TRACE_EVENT0("navigation", "BackForwardCache::RestoreEntry");

  // Select the RenderFrameHostImpl matching the navigation entry.
  auto matching_entry = std::ranges::find(
      entries_, navigation_entry_id, [](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host()->nav_entry_id();
      });

  // Not found.
  if (matching_entry == entries_.end()) {
    return nullptr;
  }

  // Don't restore an evicted frame.
  if ((*matching_entry)
          ->render_frame_host()
          ->is_evicted_from_back_forward_cache()) {
    return nullptr;
  }

  std::unique_ptr<Entry> entry = std::move(*matching_entry);
  TRACE_EVENT_INSTANT("navigation",
                      "BackForwardCache::RestoreEntry_matched_entry", "entry",
                      entry);

  entry->SetStoredPageDelegate(nullptr);
  if (prioritized_entry_ == matching_entry) {
    prioritized_entry_ = entries_.end();
  }
  entries_.erase(matching_entry);
  RemoveProcessesForEntry(*entry);
  base::TimeTicks start_time = page_restore_params->navigation_start;
  entry->SetPageRestoreParams(std::move(page_restore_params));
  RequestRecordTimeToVisible(entry->render_frame_host(), start_time);
  entry->render_frame_host()->WillLeaveBackForwardCache();

  RestoreBrowserControlsState(entry->render_frame_host());

  return entry;
}

void BackForwardCacheImpl::Flush() {
  Flush(NotRestoredReason::kCacheFlushed);
}

void BackForwardCacheImpl::Flush(NotRestoredReason reason) {
  TRACE_EVENT0("navigation", "BackForwardCache::Flush");
  for (std::unique_ptr<Entry>& entry : entries_) {
    entry->render_frame_host()->EvictFromBackForwardCacheWithReason(reason);
  }
}

void BackForwardCacheImpl::Flush(
    const StoragePartition::StorageKeyMatcherFunction& storage_key_filter) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (storage_key_filter.Run(blink::StorageKey::CreateFirstParty(
            entry->render_frame_host()->GetLastCommittedOrigin()))) {
      entry->render_frame_host()->EvictFromBackForwardCacheWithReason(
          BackForwardCacheMetrics::NotRestoredReason::kCacheFlushed);
    }
  }
}

void BackForwardCacheImpl::FlushCacheControlNoStoreEntries(
    const StoragePartition::StorageKeyMatcherFunction& storage_key_filter) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    RenderFrameHostImpl* rfh = entry->render_frame_host();
    if (rfh->LoadedWithCacheControlNoStoreHeader() &&
        storage_key_filter.Run(blink::StorageKey::CreateFirstParty(
            rfh->GetLastCommittedOrigin()))) {
      BackForwardCacheCanStoreDocumentResult flattened_reasons;
      flattened_reasons.No(
          BackForwardCacheMetrics::NotRestoredReason::kCacheControlNoStore);
      flattened_reasons.No(
          BackForwardCacheMetrics::NotRestoredReason::kCookieFlushed);
      rfh->EvictFromBackForwardCacheWithFlattenedReasons(flattened_reasons);
    }
  }
}

void BackForwardCacheImpl::Shutdown() {
  if (UsingForegroundBackgroundCacheSizeLimit()) {
    for (auto& entry : entries_) {
      RemoveProcessesForEntry(*entry.get());
    }
  }
  entries_.clear();
}

void BackForwardCacheImpl::EvictFramesInRelatedSiteInstances(
    SiteInstance* site_instance) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (entry->render_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
            site_instance)) {
      entry->render_frame_host()->EvictFromBackForwardCacheWithReason(
          BackForwardCacheMetrics::NotRestoredReason::
              kConflictingBrowsingInstance);
    }
  }
}

void BackForwardCacheImpl::PostTaskToDestroyEvictedFrames() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BackForwardCacheImpl::DestroyEvictedFrames,
                                weak_factory_.GetWeakPtr()));
}

// static
bool BackForwardCache::IsBackForwardCacheFeatureEnabled() {
  return IsBackForwardCacheEnabled();
}

// static
void BackForwardCache::DisableForRenderFrameHost(
    RenderFrameHost* render_frame_host,
    DisabledReason reason,
    std::optional<ukm::SourceId> source_id) {
  DisableForRenderFrameHost(render_frame_host->GetGlobalId(), reason,
                            source_id);
}

// static
void BackForwardCache::DisableForRenderFrameHost(
    GlobalRenderFrameHostId id,
    DisabledReason reason,
    std::optional<ukm::SourceId> source_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_bfcache_disabled_test_observer) {
    g_bfcache_disabled_test_observer->OnDisabledForFrameWithReason(id, reason);
  }

  if (auto* rfh = RenderFrameHostImpl::FromID(id)) {
    rfh->DisableBackForwardCache(reason, source_id);
  }
}

// static
void BackForwardCache::SetHadFormDataAssociated(Page& page) {
  BackForwardCacheMetrics* metrics =
      static_cast<RenderFrameHostImpl*>(&page.GetMainDocument())
          ->GetBackForwardCacheMetrics();
  if (metrics) {
    metrics->SetHadFormDataAssociated(true);
  }
}

void BackForwardCacheImpl::DisableForTesting(DisableForTestingReason reason) {
  is_disabled_for_testing_ = true;

  // Flush all the entries to make sure there are no entries in the cache after
  // DisableForTesting() is called.
  Flush();
}

const std::list<std::unique_ptr<BackForwardCacheImpl::Entry>>&
BackForwardCacheImpl::GetEntries() {
  return entries_;
}

std::list<BackForwardCacheImpl::Entry*>
BackForwardCacheImpl::GetEntriesForRenderViewHostImpl(
    const RenderViewHostImpl* rvhi) const {
  std::list<BackForwardCacheImpl::Entry*> entries_for_rvhi;
  for (auto& entry : entries_) {
    for (const auto& rvh : entry->render_view_hosts()) {
      if (&*rvh == rvhi) {
        entries_for_rvhi.push_back(entry.get());
        break;
      }
    }
  }
  return entries_for_rvhi;
}

base::expected<BackForwardCacheImpl::Entry*,
               BackForwardCacheImpl::GetEntryFailureCase>
BackForwardCacheImpl::GetOrEvictEntry(int navigation_entry_id) {
  auto matching_entry = std::ranges::find(
      entries_, navigation_entry_id, [](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host()->nav_entry_id();
      });

  if (matching_entry == entries_.end()) {
    return base::unexpected(
        BackForwardCacheImpl::GetEntryFailureCase::kEntryNotFound);
  }

  auto* render_frame_host = (*matching_entry)->render_frame_host();
  // Don't return the entry if it was evicted.
  if (render_frame_host->is_evicted_from_back_forward_cache()) {
    return base::unexpected(
        BackForwardCacheImpl::GetEntryFailureCase::kEntryEvictedBefore);
  }

  // If we are in the experiments to allow pages with cache-control:no-store
  // in back/forward cache and the page has cache-control:no-store, we should
  // record them as reasons. It might not be possible to restore the entry even
  // if it hasn't been evicted up until this point, e.g. due to cache-control:
  // no-store preventing restoration but not storage
  BackForwardCacheCanStoreDocumentResultWithTree bfcache_eligibility =
      GetCurrentBackForwardCacheEligibility(render_frame_host);
  if (!bfcache_eligibility.CanRestore()) {
    render_frame_host->EvictFromBackForwardCacheWithFlattenedAndTreeReasons(
        bfcache_eligibility);
    return base::unexpected(
        BackForwardCacheImpl::GetEntryFailureCase::kEntryIneligibleAndEvicted);
  }

  return (*matching_entry).get();
}

bool BackForwardCacheImpl::HasPotentiallyMatchingEntry(
    const RenderFrameHostImpl& commiting_rfh,
    const std::optional<url::Origin>& initiator_origin,
    bool require_no_subframes) const {
  if (commiting_rfh.GetSiteInstance()->GetRelatedActiveContentsCount() > 1) {
    // If the committing RFH has relation to other pages/WebContents, it can't
    // possibly restore BFCached pages, as BFCached pages use a separate
    // BrowsingInstance (and thus will sever relation to other pages).
    return false;
  }
  for (auto& entry : entries_) {
    auto* bfcached_rfh = entry->render_frame_host();
    if (require_no_subframes && bfcached_rfh->child_count() > 0) {
      continue;
    }
    // If the URL, origin, and security properties match, the navigation is
    // targeting the same page as `bfcached_rfh`, so theoretically it could
    // just use `bfcached_rfh` instead of creating a new RenderFrameHost to
    // commit in. We don't currently do that, but track these cases in metrics.
    if (commiting_rfh.GetLastCommittedURL() ==
            bfcached_rfh->GetLastCommittedURL() &&
        commiting_rfh.GetLastCommittedOrigin() ==
            bfcached_rfh->GetLastCommittedOrigin() &&
        bfcached_rfh->last_committed_frame_entry()->initiator_origin() ==
            initiator_origin &&
        commiting_rfh.policy_container_host()->policies() ==
            bfcached_rfh->policy_container_host()->policies()) {
      return true;
    }
  }
  return false;
}

void BackForwardCacheImpl::RenderViewHostNoLongerStored(
    RenderViewHostImpl* rvh) {
  // `AddProcessesForEntry` are gated on
  // `UsingForegroundBackgroundCacheSizeLimit` in adding entries to the
  // `observed_processes_` list so we have the same conditional here.
  if (!UsingForegroundBackgroundCacheSizeLimit()) {
    return;
  }
  RenderViewHostNoLongerStoredInternal(rvh);
}

void BackForwardCacheImpl::RenderViewHostNoLongerStoredInternal(
    RenderViewHostImpl* rvh) {
  RenderProcessHostImpl* process =
      static_cast<RenderProcessHostImpl*>(rvh->GetProcess());
  // Remove 1 instance of this process from the multiset.
  observed_processes_.erase(observed_processes_.find(process));
  if (observed_processes_.find(process) == observed_processes_.end()) {
    process->RemoveInternalObserver(this);
  }
}

void BackForwardCacheImpl::AddProcessesForEntry(Entry& entry) {
  if (!UsingForegroundBackgroundCacheSizeLimit()) {
    return;
  }
  for (const auto& rvh : entry.render_view_hosts()) {
    RenderProcessHostImpl* process =
        static_cast<RenderProcessHostImpl*>(rvh->GetProcess());
    if (observed_processes_.find(process) == observed_processes_.end()) {
      process->AddInternalObserver(this);
    }
    observed_processes_.insert(process);
  }
}

void BackForwardCacheImpl::RemoveProcessesForEntry(Entry& entry) {
  if (!UsingForegroundBackgroundCacheSizeLimit()) {
    return;
  }
  for (const auto& rvh : entry.render_view_hosts()) {
    RenderViewHostNoLongerStoredInternal(&*rvh);
  }
}

void BackForwardCacheImpl::DestroyEvictedFrames() {
  TRACE_EVENT0("navigation", "BackForwardCache::DestroyEvictedFrames");
  if (entries_.empty()) {
    return;
  }

  std::erase_if(entries_, [this](std::unique_ptr<Entry>& entry) {
    if (entry->render_frame_host()->is_evicted_from_back_forward_cache()) {
      if (prioritized_entry_->get() == entry.get()) {
        prioritized_entry_ = entries_.end();
      }
      RemoveProcessesForEntry(*entry);
      return true;
    }
    return false;
  });
}

bool BackForwardCacheImpl::IsAllowed(const GURL& current_url) {
  return IsHostPathAllowed(current_url) && IsQueryAllowed(current_url);
}

bool BackForwardCacheImpl::IsHostPathAllowed(const GURL& current_url) {
  // If the current_url matches the blocked host and path, current_url is
  // not allowed to be cached.
  const auto& it = blocked_urls_.find(current_url.GetHost());
  if (it != blocked_urls_.end()) {
    for (const std::string& blocked_path : it->second) {
      if (base::StartsWith(current_url.path(), blocked_path)) {
        return false;
      }
    }
  }

  // By convention, when |allowed_urls_| is empty, it means there are no
  // restrictions about what RenderFrameHost can enter the BackForwardCache.
  if (allowed_urls_.empty()) {
    return true;
  }

  // Checking for each url in the |allowed_urls_|, if the current_url matches
  // the corresponding host and path is the prefix of the allowed url path. We
  // only check for host and path and not any other components including url
  // scheme here.
  const auto& entry = allowed_urls_.find(current_url.GetHost());
  if (entry != allowed_urls_.end()) {
    for (const std::string& allowed_path : entry->second) {
      if (base::StartsWith(current_url.path(), allowed_path)) {
        return true;
      }
    }
  }
  return false;
}

bool BackForwardCacheImpl::IsQueryAllowed(const GURL& current_url) {
  std::vector<std::string> cgi_params =
      base::SplitString(current_url.query(), "&", base::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (const std::string& cgi_param : cgi_params) {
    if (base::Contains(blocked_cgi_params_, cgi_param)) {
      return false;
    }
  }
  return true;
}

void BackForwardCacheImpl::WillCommitNavigationToCachedEntry(
    Entry& bfcache_entry,
    base::OnceClosure done_callback) {
  // Disable JS eviction in renderers and defer the navigation commit until
  // we've received confirmation that eviction is disabled from renderers.
  auto cb = base::BarrierClosure(
      bfcache_entry.render_view_hosts().size(),
      base::BindOnce([](base::OnceClosure cb) { std::move(cb).Run(); },
                     std::move(done_callback)));

  for (const auto& rvh : bfcache_entry.render_view_hosts()) {
    rvh->PrepareToLeaveBackForwardCache(cb);
  }
}

bool BackForwardCacheImpl::
    IsRenderFrameHostWithSIGInBackForwardCacheForDebugging(
        SiteInstanceGroupId site_instance_group_id) {
  bool found = false;
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (entry->render_frame_host()->is_evicted_from_back_forward_cache()) {
      continue;
    }
    entry->render_frame_host()->ForEachRenderFrameHostImplWithAction(
        [&found, site_instance_group_id](RenderFrameHostImpl* rfh) {
          if (rfh->GetSiteInstance()->group()->GetId() ==
              site_instance_group_id) {
            found = true;
            return RenderFrameHost::FrameIterationAction::kStop;
          }
          return RenderFrameHost::FrameIterationAction::kContinue;
        });
  }
  return found;
}

bool BackForwardCacheImpl::IsRelatedSiteInstanceInBackForwardCacheForDebugging(
    SiteInstance& site_instance) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (!entry->render_frame_host()->is_evicted_from_back_forward_cache() &&
        entry->render_frame_host()->GetSiteInstance()->IsRelatedSiteInstance(
            &site_instance)) {
      return true;
    }
  }
  return false;
}

bool BackForwardCacheImpl::
    IsRenderFrameProxyHostWithSIGInBackForwardCacheForDebugging(
        SiteInstanceGroupId site_instance_group_id) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    for (const auto& entry_rfph : entry->proxy_hosts()) {
      if (!entry->render_frame_host()->is_evicted_from_back_forward_cache() &&
          entry_rfph.second->site_instance_group()->GetId() ==
              site_instance_group_id) {
        return true;
      }
    }
  }
  return false;
}

bool BackForwardCacheImpl::
    IsRenderViewHostWithMapIdInBackForwardCacheForDebugging(
        const RenderViewHostImpl& rvh) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    for (const auto& entry_rvh : entry->render_view_hosts()) {
      if (!entry->render_frame_host()->is_evicted_from_back_forward_cache() &&
          entry_rvh->rvh_map_id() == rvh.rvh_map_id()) {
        return true;
      }
    }
  }
  return false;
}

// Static
bool BackForwardCacheImpl::IsUnloadAllowed() {
  return base::FeatureList::IsEnabled(kBackForwardCacheUnloadAllowed);
}

// static
void BackForwardCacheImpl::VlogUnexpectedRendererToBrowserMessage(
    const char* interface_name,
    uint32_t message_name,
    RenderFrameHostImpl* rfh) {
  VLOG(1) << "BackForwardCacheMessageFilter::WillDispatch bad_message "
          << "interface_name " << interface_name << " message_name "
          << message_name;
  // TODO(crbug.com/40244391): Remove these when bug is fixed.
  PageLifecycleStateManager* page_lifecycle_state_manager =
      rfh->render_view_host()->GetPageLifecycleStateManager();
  VLOG(1) << "URL: " << rfh->GetLastCommittedURL() << " current "
          << page_lifecycle_state_manager->IsInBackForwardCache() << " acked "
          << page_lifecycle_state_manager->last_acknowledged_state()
                 .is_in_back_forward_cache;
}

BackForwardCache::DisabledReason::DisabledReason(
    content::BackForwardCache::DisabledSource source,
    content::BackForwardCache::DisabledReasonType id,
    std::string description,
    std::string context,
    std::string report_string)
    : source(source),
      id(id),
      description(description),
      context(context),
      report_string(report_string) {}

BackForwardCache::DisabledReason::DisabledReason(
    const BackForwardCache::DisabledReason& reason) = default;
std::weak_ordering BackForwardCache::DisabledReason::operator<=>(
    const DisabledReason& other) const {
  return std::tie(source, id) <=> std::tie(other.source, other.id);
}
bool BackForwardCache::DisabledReason::operator==(
    const DisabledReason& other) const {
  return std::tie(source, id) == std::tie(other.source, other.id);
}

BackForwardCacheCanStoreTreeResult::BackForwardCacheCanStoreTreeResult(
    RenderFrameHostImpl* rfh,
    const url::Origin& main_document_origin,
    const GURL& url,
    BackForwardCacheCanStoreDocumentResult& result_for_this_document)
    : document_result_(std::move(result_for_this_document)),
      is_same_origin_(IsSameOriginForTreeResult(rfh, main_document_origin)),
      is_root_outermost_main_frame_(!rfh->GetParentOrOuterDocumentOrEmbedder()),
      id_(rfh->frame_tree_node()->html_id()),
      name_(rfh->frame_tree_node()->html_name()),
      src_(rfh->frame_tree_node()->html_src()),
      url_(url) {}

BackForwardCacheCanStoreTreeResult::BackForwardCacheCanStoreTreeResult(
    bool is_same_origin,
    const GURL& url)
    : is_same_origin_(is_same_origin),
      is_root_outermost_main_frame_(true),
      id_(""),
      name_(""),
      src_(""),
      url_(url) {}

BackForwardCacheCanStoreTreeResult::~BackForwardCacheCanStoreTreeResult() =
    default;

void BackForwardCacheCanStoreTreeResult::AddReasonsToSubtreeRootFrom(
    const BackForwardCacheCanStoreDocumentResult& result) {
  document_result_.AddReasonsFrom(result);
}

void BackForwardCacheCanStoreTreeResult::AppendChild(
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> child) {
  children_.push_back(std::move(child));
}

const BackForwardCacheCanStoreDocumentResult
BackForwardCacheCanStoreTreeResult::FlattenTree() {
  BackForwardCacheCanStoreDocumentResult document_result;
  FlattenTreeHelper(&document_result);
  return document_result;
}

void BackForwardCacheCanStoreTreeResult::FlattenTreeHelper(
    BackForwardCacheCanStoreDocumentResult* document_result) {
  document_result->AddReasonsFrom(document_result_);
  for (const auto& subtree : GetChildren()) {
    subtree->FlattenTreeHelper(document_result);
  }
}

std::unique_ptr<BackForwardCacheCanStoreTreeResult>
BackForwardCacheCanStoreTreeResult::CreateEmptyTreeForNavigation(
    NavigationRequest* navigation) {
  DCHECK(BackForwardCacheMetrics::IsCrossDocumentMainFrameHistoryNavigation(
      navigation));
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> empty_tree(
      new BackForwardCacheCanStoreTreeResult(
          /*is_same_origin=*/true, navigation->GetURL()));
  return empty_tree;
}

std::unique_ptr<BackForwardCacheCanStoreTreeResult>
BackForwardCacheCanStoreTreeResult::CreateEmptyTree(RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult empty_result;
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> empty_tree(
      new BackForwardCacheCanStoreTreeResult(rfh, rfh->GetLastCommittedOrigin(),
                                             rfh->GetLastCommittedURL(),
                                             empty_result));
  return empty_tree;
}

blink::mojom::BackForwardCacheNotRestoredReasonsPtr
BackForwardCacheCanStoreTreeResult::GetWebExposedNotRestoredReasons() {
  DCHECK(is_root_outermost_main_frame_);
  uint32_t count = GetCrossOriginReachableFrameCount();
  int exposed_cross_origin_iframe_index =
      count == 0 ? 0 : base::RandInt(0, count - 1);
  return GetWebExposedNotRestoredReasonsInternal(
      exposed_cross_origin_iframe_index);
}

blink::mojom::BackForwardCacheNotRestoredReasonsPtr
BackForwardCacheCanStoreTreeResult::GetWebExposedNotRestoredReasonsInternal(
    int& exposed_cross_origin_iframe_index) {
  blink::mojom::BackForwardCacheNotRestoredReasonsPtr not_restored_reasons =
      blink::mojom::BackForwardCacheNotRestoredReasons::New();
  if (IsSameOrigin()) {
    // Add same_origin_details for documents that are same-origin with
    // the main document. Stop recursion as soon as we hit a cross-origin
    // document.
    not_restored_reasons->same_origin_details =
        blink::mojom::SameOriginBfcacheNotRestoredDetails::New();
    not_restored_reasons->same_origin_details->url = url_;
    // Populate the reasons for same-origin frames.
    auto& map = GetDocumentResult().reason_to_source_map();
    for (const auto& [reason, sources] : map) {
      if (base::FeatureList::IsEnabled(
              blink::features::kBackForwardCacheUpdateNotRestoredReasonsName) &&
          reason == "session-restored") {
        // Session restore should return nullptr just like non-history
        // navigations.
        return nullptr;
      }
      if (sources.empty()) {
        not_restored_reasons->reasons.push_back(
            blink::mojom::BFCacheBlockingDetailedReason::New(
                reason, /*source=*/nullptr));
      } else {
        for (const auto& source : sources) {
          not_restored_reasons->reasons.push_back(
              blink::mojom::BFCacheBlockingDetailedReason::New(reason,
                                                               source.Clone()));
        }
      }
    }
    if (is_root_outermost_main_frame_) {
      int index_copy = exposed_cross_origin_iframe_index;
      bool no_masked_reason =
          std::find_if(
              not_restored_reasons->reasons.begin(),
              not_restored_reasons->reasons.end(),
              [](const blink::mojom::BFCacheBlockingDetailedReasonPtr& reason) {
                return reason->name == "masked";
              }) == not_restored_reasons->reasons.end();
      if (HasUnexposedCrossOriginBlockingIframe(index_copy) &&
          no_masked_reason) {
        // If any cross-origin iframe is blocking and does not have "masked" in
        // its own reasons, we need to add "masked" to the outermost main
        // frame's reasons. Note that we need to add "masked" only when the
        // reasons do not have it yet.
        blink::mojom::BFCacheBlockingDetailedReasonPtr masked_reason =
            blink::mojom::BFCacheBlockingDetailedReason::New();
        masked_reason->name = "masked";
        not_restored_reasons->reasons.push_back(std::move(masked_reason));
      }
    }
    for (const auto& subtree : GetChildren()) {
      not_restored_reasons->same_origin_details->children.push_back(
          subtree->GetWebExposedNotRestoredReasonsInternal(
              exposed_cross_origin_iframe_index));
    }
  } else {
    // This is a cross-origin document. This might or might not be the randomly
    // selected document that is going to be exposed.
    if (!FlattenTree().CanRestore() && exposed_cross_origin_iframe_index == 0 &&
        base::FeatureList::IsEnabled(kAllowCrossOriginNotRestoredReasons)) {
      // This is the randomly selected cross-origin iframe / subtree
      // blocking bfcache.
      // Note that we need to flatten the tree in order to check the eligibility
      // of the cross-origin subtree. Add "masked" to this frame to signal that
      // this is the blocking frame.
      blink::mojom::BFCacheBlockingDetailedReasonPtr masked_reason =
          blink::mojom::BFCacheBlockingDetailedReason::New();
      masked_reason->name = "masked";
      not_restored_reasons->reasons.push_back(std::move(masked_reason));
    }
    // Decrease the index now that we saw a cross-origin iframe.
    exposed_cross_origin_iframe_index--;
    // Do not iterate through the children now that we have encountered a
    // cross-origin iframe.
  }
  // Report src, id and name for both cross-origin and same-origin frames. This
  // information is only sent to the main frame's renderer, which already knew
  // it on the previous visit. We send this because the frame tree could have
  // changed by the time the page is navigated away, and sending this
  // information would help identify which frames caused restore to fail.
  not_restored_reasons->src = src_;
  not_restored_reasons->id = id_;
  not_restored_reasons->name = name_;
  return not_restored_reasons;
}

bool BackForwardCacheCanStoreTreeResult::HasUnexposedCrossOriginBlockingIframe(
    int& exposed_cross_origin_iframe_index) {
  if (!IsSameOrigin()) {
    // This is a cross-origin subtree.
    // Check if this document is the randomly selected one.
    bool randomly_selected =
        exposed_cross_origin_iframe_index == 0 &&
        base::FeatureList::IsEnabled(kAllowCrossOriginNotRestoredReasons);
    exposed_cross_origin_iframe_index--;
    if (!FlattenTree().CanRestore() && !randomly_selected) {
      // When this cross-origin subtree is blocking and is not randomly
      // selected, this is an unexposed cross-origin blocking iframe.
      return true;
    } else {
      // We do not have unexposed blocking frame in this cross-origin subtree.
      return false;
    }
    // Note that we do not go into cross-origin iframe's subframes.
  } else {
    // Recursively check the subtrees for the same origin iframes.
    for (const auto& subtree : GetChildren()) {
      if (subtree->HasUnexposedCrossOriginBlockingIframe(
              exposed_cross_origin_iframe_index)) {
        return true;
      }
    }
    // If none of the subtree has unexposed cross-origin blocking iframe, return
    // false.
    return false;
  }
}

uint32_t
BackForwardCacheCanStoreTreeResult::GetCrossOriginReachableFrameCount() {
  // If the document is cross-origin, we cannot reach any further. Only count
  // the one we have reached and return.
  if (!IsSameOrigin()) {
    return 1;
  }
  uint32_t count = 0;
  for (const auto& subtree : GetChildren()) {
    count += subtree->GetCrossOriginReachableFrameCount();
  }
  return count;
}

BackForwardCacheCanStoreDocumentResultWithTree::
    BackForwardCacheCanStoreDocumentResultWithTree(
        BackForwardCacheCanStoreDocumentResult& flattened_reasons,
        std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree_reasons)
    : flattened_reasons(std::move(flattened_reasons)),
      tree_reasons(std::move(tree_reasons)) {}

BackForwardCacheCanStoreDocumentResultWithTree::
    BackForwardCacheCanStoreDocumentResultWithTree(
        BackForwardCacheCanStoreDocumentResultWithTree&& other)
    : flattened_reasons(other.flattened_reasons),
      tree_reasons(std::move(other.tree_reasons)) {}

BackForwardCacheCanStoreDocumentResultWithTree::
    ~BackForwardCacheCanStoreDocumentResultWithTree() = default;

}  // namespace content
