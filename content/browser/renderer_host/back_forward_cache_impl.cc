// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_impl.h"

#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/visible_time_request_trigger.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/visibility.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-shared.h"
#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace content {

class RenderProcessHostInternalObserver;

// Allows overriding the sizes of back/forward cache.
// Sizes set via this feature's parameters take precedence over others.
BASE_FEATURE(kBackForwardCacheSize,
             "BackForwardCacheSize",
// Sets the BackForwardCache size for desktop.
// See crbug.com/1291435.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
// Sets BackForwardCache cache_size=6 per crbug.com/1291435.
const base::FeatureParam<int> kBackForwardCacheSizeCacheSize{
    &kBackForwardCacheSize, "cache_size", 6};
// Disables EnforceCacheSizeLimitInternal() with foreground_cache_size=0, as
// the BFCachePolicy manager takes care of pruning for foreground tabs as well.
const base::FeatureParam<int> kBackForwardCacheSizeForegroundCacheSize{
    &kBackForwardCacheSize, "foreground_cache_size", 0};

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;
using blink::scheduler::WebSchedulerTrackedFeatures;

// The default number of entries the BackForwardCache can hold per tab.
static constexpr size_t kDefaultBackForwardCacheSize = 1;

// The default number value for the "foreground_cache_size" field trial
// parameter. This parameter controls the numbers of entries associated with
// foregrounded process the BackForwardCache can hold per tab, when using the
// foreground/background cache-limiting strategy. This strategy is enabled if
// the parameter values is non-zero.
static constexpr size_t kDefaultForegroundBackForwardCacheSize = 0;

// The default time to live in seconds for documents in BackForwardCache.
// See also crbug.com/1305878.
static constexpr int kDefaultTimeToLiveInBackForwardCacheInSeconds = 600;

#if BUILDFLAG(IS_ANDROID)
bool IsProcessBindingEnabled() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled())
    return false;
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

bool IsContentInjectionSupported() {
  if (!IsBackForwardCacheEnabled())
    return false;
  static constexpr base::FeatureParam<bool> content_injection_supported(
      &features::kBackForwardCache, "content_injection_supported", true);
  return content_injection_supported.Get();
}

WebSchedulerTrackedFeatures SupportedFeaturesImpl() {
  WebSchedulerTrackedFeatures features;
  if (!IsBackForwardCacheEnabled())
    return features;

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
  if (!IsBackForwardCacheEnabled())
    return false;
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
  if (!IsBackForwardCacheEnabled())
    return false;
  static constexpr base::FeatureParam<bool> should_ignore_blocklists(
      &features::kBackForwardCache, "should_ignore_blocklists", false);
  return should_ignore_blocklists.Get();
}

// A list of WebSchedulerTrackedFeatures that always block back/forward
// cache. Some of these features are listed as blocking back/forward cache
// when actually the blocking is flag controlled and they are not registered
// as being used if we don't want them to block.
constexpr WebSchedulerTrackedFeatures kDisallowedFeatures(
    WebSchedulerTrackedFeature::kBroadcastChannel,
    WebSchedulerTrackedFeature::kContainsPlugins,
    WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet,
    WebSchedulerTrackedFeature::kDummy,
    WebSchedulerTrackedFeature::kIdleManager,
    WebSchedulerTrackedFeature::kIndexedDBConnection,
    WebSchedulerTrackedFeature::kIndexedDBEvent,
    WebSchedulerTrackedFeature::kKeyboardLock,
    WebSchedulerTrackedFeature::kKeepaliveRequest,
    WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction,
    WebSchedulerTrackedFeature::kPaymentManager,
    WebSchedulerTrackedFeature::kPictureInPicture,
    WebSchedulerTrackedFeature::kPortal,
    WebSchedulerTrackedFeature::kPrinting,
    WebSchedulerTrackedFeature::kRequestedAudioCapturePermission,
    WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors,
    WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission,
    WebSchedulerTrackedFeature::kRequestedMIDIPermission,
    WebSchedulerTrackedFeature::kRequestedVideoCapturePermission,
    WebSchedulerTrackedFeature::kSharedWorker,
    WebSchedulerTrackedFeature::kWebDatabase,
    WebSchedulerTrackedFeature::kWebOTPService,
    WebSchedulerTrackedFeature::kSpeechRecognizer,
    WebSchedulerTrackedFeature::kSpeechSynthesis,
    WebSchedulerTrackedFeature::kWebDatabase,
    WebSchedulerTrackedFeature::kWebHID,
    WebSchedulerTrackedFeature::kWebLocks,
    WebSchedulerTrackedFeature::kWebRTC,
    WebSchedulerTrackedFeature::kWebShare,
    WebSchedulerTrackedFeature::kWebSocket,
    WebSchedulerTrackedFeature::kWebTransport,
    WebSchedulerTrackedFeature::kWebXR);
constexpr WebSchedulerTrackedFeatures kInjectionFeatures(
    WebSchedulerTrackedFeature::kInjectedJavascript,
    WebSchedulerTrackedFeature::kInjectedStyleSheet);
constexpr WebSchedulerTrackedFeatures kNetworkFeatures(
    WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers,
    WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch,
    WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR);
// A list of WebSchedulerTrackedFeatures that should never block back/forward
// cache.
constexpr WebSchedulerTrackedFeatures kAllowedFeatures(
    WebSchedulerTrackedFeature::kDocumentLoaded,
    WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache,
    // This is handled in |UpdateCanStoreToIncludeCacheControlNoStore()|, and no
    // need to include in |GetDisallowedFeatures()|.
    WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore,
    // TODO(crbug.com/1357482): Figure out if these two should be allowed.
    WebSchedulerTrackedFeature::kOutstandingNetworkRequestDirectSocket,
    WebSchedulerTrackedFeature::kRequestedStorageAccessGrant,
    // We don't block on subresource cache-control:no-store or no-cache.
    WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache,
    WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore,
    // We only record this if "Cache-Control: no-store" header is present on the
    // main frame.
    WebSchedulerTrackedFeature::kAuthorizationHeader,
    // TODO(crbug.com/1357482): Figure out if this should be allowed.
    WebSchedulerTrackedFeature::kWebNfc);

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
std::map<std::string, std::vector<std::string>> ParseCommaSeparatedURLs(
    base::StringPiece comma_separated_urls) {
  std::map<std::string, std::vector<std::string>> urls;
  for (auto& it :
       base::SplitString(comma_separated_urls, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL)) {
    GURL url = GURL(it);
    urls[url.host()].push_back(url.path());
  }
  return urls;
}

// Parses the "cgi_params" field trial parameter into a set by splitting on "|".
std::unordered_set<std::string> ParseBlockedCgiParams(
    base::StringPiece cgi_params_string) {
  std::vector<std::string> split =
      base::SplitString(cgi_params_string, "|", base::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  std::unordered_set<std::string> cgi_params;
  cgi_params.insert(split.begin(), split.end());
  return cgi_params;
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
        false);
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
    if (!rvh->GetProcess()->IsProcessBackgrounded()) {
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
    cache_control_level{&kCacheControlNoStoreEnterBackForwardCache,
                        kCacheControlNoStoreExperimentLevelName,
                        CacheControlNoStoreExperimentLevel::kDoNotStore,
                        &cache_control_levels};

CacheControlNoStoreExperimentLevel GetCacheControlNoStoreLevel() {
  if (!IsBackForwardCacheEnabled() ||
      !base::FeatureList::IsEnabled(
          kCacheControlNoStoreEnterBackForwardCache)) {
    return CacheControlNoStoreExperimentLevel::kDoNotStore;
  }
  return cache_control_level.Get();
}

bool IsSameOriginForTreeResult(RenderFrameHostImpl* rfh,
                               const GURL& url,
                               const url::Origin& main_document_origin) {
  // Treat any frame inside a fenced frame as cross origin so we don't leak
  // any information.
  if (rfh->IsNestedWithinFencedFrame())
    return false;
  return url::Origin::Create(url).IsSameOriginWith(main_document_origin);
}

}  // namespace

// static
BlockListedFeatures BackForwardCacheImpl::GetAllowedFeatures(
    RequestedFeatures requested_features) {
  WebSchedulerTrackedFeatures result = kAllowedFeatures;
  if (IsContentInjectionSupported()) {
    result.PutAll(kInjectionFeatures);
  }
  if (IgnoresOutstandingNetworkRequestForTesting()) {
    result.PutAll(kNetworkFeatures);
  }
  result.PutAll(SupportedFeatures());
  if (requested_features == RequestedFeatures::kOnlySticky) {
    // Add non-sticky disallowed features.
    WebSchedulerTrackedFeatures non_sticky =
        Difference(kDisallowedFeatures, blink::scheduler::StickyFeatures());
    if (!IsContentInjectionSupported()) {
      non_sticky.PutAll(
          Difference(kInjectionFeatures, blink::scheduler::StickyFeatures()));
    }
    if (!IgnoresOutstandingNetworkRequestForTesting()) {
      non_sticky.PutAll(
          Difference(kNetworkFeatures, blink::scheduler::StickyFeatures()));
    }
    result.PutAll(non_sticky);
  }
  return result;
}

// static
BlockListedFeatures BackForwardCacheImpl::GetDisallowedFeatures(
    RequestedFeatures requested_features) {
  WebSchedulerTrackedFeatures result = kDisallowedFeatures;
  if (!IsContentInjectionSupported()) {
    result.PutAll(kInjectionFeatures);
  }
  if (!IgnoresOutstandingNetworkRequestForTesting()) {
    result.PutAll(kNetworkFeatures);
  }
  result.RemoveAll(SupportedFeatures());
  if (requested_features == RequestedFeatures::kOnlySticky) {
    // Remove all non-sticky features from |result|.
    result = Intersection(result, blink::scheduler::StickyFeatures());
  }
  return result;
}

// static
BackForwardCacheImpl::MessageHandlingPolicyWhenCached
BackForwardCacheImpl::GetChannelAssociatedMessageHandlingPolicy() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled())
    return kMessagePolicyNone;

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
    : stored_page_(std::move(stored_page)) {
  if (BackForwardCacheImpl::AllowStoringPagesWithCacheControlNoStore()) {
    cookie_modified_ = {/*http_only_cookie_modified*/ false,
                        /*cookie_modified*/ false};
  }
}

BackForwardCacheImpl::Entry::~Entry() = default;

void BackForwardCacheImpl::Entry::WriteIntoTrace(
    perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("render_frame_host", render_frame_host());
}

void BackForwardCacheImpl::Entry::StartMonitoringCookieChange() {
  RenderFrameHostImpl* rfh = stored_page_->render_frame_host();
  StoragePartition* storage_partition = rfh->GetStoragePartition();
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  if (!cookie_listener_receiver_.is_bound()) {
    // Listening only to the main document's URL, not the documents inside the
    // subframes.
    cookie_manager->AddCookieChangeListener(
        rfh->GetLastCommittedURL(), absl::nullopt,
        cookie_listener_receiver_.BindNewPipeAndPassRemote());
  }
}

void BackForwardCacheImpl::Entry::OnCookieChange(
    const net::CookieChangeInfo& change) {
  DCHECK(cookie_modified_.has_value());
  cookie_modified_->http_only_cookie_modified = change.cookie.IsHttpOnly();
  cookie_modified_->cookie_modified = true;
}

void BackForwardCacheImpl::RenderProcessBackgroundedChanged(
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

BackForwardCacheImpl::BackForwardCacheImpl()
    : allowed_urls_(ParseCommaSeparatedURLs(GetAllowedURLList())),
      blocked_urls_(ParseCommaSeparatedURLs(GetBlockedURLList())),
      blocked_cgi_params_(ParseBlockedCgiParams(GetBlockedCgiParams())),
      weak_factory_(this) {}

BackForwardCacheImpl::~BackForwardCacheImpl() {
  Shutdown();
}

absl::optional<int> GetFieldTrialParamByFeatureAsOptionalInt(
    const base::Feature& feature,
    const std::string& param_name) {
  std::string value_as_string =
      GetFieldTrialParamValueByFeature(feature, param_name);
  int value_as_int = 0;
  if (base::StringToInt(value_as_string, &value_as_int)) {
    return absl::optional<int>(value_as_int);
  }
  return absl::optional<int>();
}

base::TimeDelta BackForwardCacheImpl::GetTimeToLiveInBackForwardCache() {
  // We use the following order of priority if multiple values exist:
  // - The TTL set in `kBackForwardCacheTimeToLiveControl` takes precedence over
  //   the default value.
  // - Infinite if kBackForwardCacheNoTimeEviction is enabled.
  // - Default value otherwise, kDefaultTimeToLiveInBackForwardCacheInSeconds.

  if (base::FeatureList::IsEnabled(
          features::kBackForwardCacheTimeToLiveControl)) {
    absl::optional<int> time_to_live = GetFieldTrialParamByFeatureAsOptionalInt(
        features::kBackForwardCacheTimeToLiveControl, "time_to_live_seconds");
    if (time_to_live.has_value()) {
      return base::Seconds(time_to_live.value());
    }
  }

  if (base::FeatureList::IsEnabled(kBackForwardCacheNoTimeEviction)) {
    return base::TimeDelta::Max();
  }

  return base::Seconds(kDefaultTimeToLiveInBackForwardCacheInSeconds);
}

// static
size_t BackForwardCacheImpl::GetCacheSize() {
  if (!IsBackForwardCacheEnabled())
    return 0;
  if (base::FeatureList::IsEnabled(kBackForwardCacheSize)) {
    return kBackForwardCacheSizeCacheSize.Get();
  }
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "cache_size", kDefaultBackForwardCacheSize);
}

// static
size_t BackForwardCacheImpl::GetForegroundedEntriesCacheSize() {
  if (!IsBackForwardCacheEnabled())
    return 0;
  if (base::FeatureList::IsEnabled(kBackForwardCacheSize)) {
    return kBackForwardCacheSizeForegroundCacheSize.Get();
  }
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "foreground_cache_size",
      kDefaultForegroundBackForwardCacheSize);
}

// static
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
  if (!AllowStoringPagesWithCacheControlNoStore())
    return;
  // If the page didn't have cache-control: no-store, do nothing.
  if (!render_frame_host->GetBackForwardCacheDisablingFeatures().Has(
          WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore)) {
    return;
  }

  auto* matching_entry = FindMatchingEntry(render_frame_host->GetPage());
  // |matching_entry| can be nullptr for tests because this can be called from
  // |GetCurrentBackForwardCacheEligibility()|, at which point |rfh| may not
  // have a matching entry yet.
  if (!matching_entry)
    return;

  // Note that kCacheControlNoStoreHTTPOnlyCookieModified,
  // kCacheControlNoStoreCookieModified and kCacheControlNoStore are mutually
  // exclusive.
  if (matching_entry->cookie_modified_->http_only_cookie_modified) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::
                  kCacheControlNoStoreHTTPOnlyCookieModified);
  } else if (matching_entry->cookie_modified_->cookie_modified) {
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
  BackForwardCacheCanStoreDocumentResult flattened_result;
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree =
      PopulateReasonsForPage(rfh, flattened_result,
                             /*include_non_sticky=*/true);
  LogAndTraceResult(
      *rfh, flattened_result,
      "BackForwardCacheImpl::GetCurrentBackForwardCacheEligibility");
  DCHECK(tree->FlattenTree() == flattened_result);
  return BackForwardCacheCanStoreDocumentResultWithTree(flattened_result,
                                                        std::move(tree));
}

BackForwardCacheCanStoreDocumentResultWithTree
BackForwardCacheImpl::GetFutureBackForwardCacheEligibilityPotential(
    RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult flattened;
  auto tree = PopulateReasonsForPage(rfh, flattened,
                                     /*include_non_sticky = */ false);
  LogAndTraceResult(
      *rfh, flattened,
      "BackForwardCacheImpl::GetFutureBackForwardCacheEligibilityPotential");
  DCHECK(tree->FlattenTree() == flattened);
  return BackForwardCacheCanStoreDocumentResultWithTree(flattened,
                                                        std::move(tree));
}

std::unique_ptr<BackForwardCacheCanStoreTreeResult>
BackForwardCacheImpl::PopulateReasonsForPage(
    RenderFrameHostImpl* rfh,
    BackForwardCacheCanStoreDocumentResult& flattened_result,
    bool include_non_sticky) {
  // TODO(crbug.com/1275977): This function should only be called when |rfh| is
  // the primary main frame. Fix |ShouldProactivelySwapBrowsingInstance()| and
  // |UnloadOldFrame()| so that it will not check bfcache eligibility if not
  // primary main frame.
  BackForwardCacheCanStoreDocumentResult main_document_specific_result;
  // This function can be called during eviction, and |rfh| can be in
  // back/forward cache, which is considered as non primary main frame.
  bool main_frame_in_bfcache =
      rfh->IsInBackForwardCache() && rfh->IsOutermostMainFrame();

  if (!rfh->IsInPrimaryMainFrame() && !main_frame_in_bfcache) {
    // When |rfh| is not the primary main frame and is not the bfcache main
    // frame, e.g. when |rfh| is prerendering, fenced frame root or is not the
    // main frame, we can reach this block.
    // We do not need to check the subframes' reasons because callers that reach
    // here only care about whether can_store is true or false, not about the
    // reasons.
    main_document_specific_result.No(
        BackForwardCacheMetrics::NotRestoredReason::kNotPrimaryMainFrame);
  } else {
    // Populate main document specific reasons.
    PopulateReasonsForMainDocument(main_document_specific_result, rfh);
  }
  // Add the reasons for main document to the flattened list.
  flattened_result.AddReasonsFrom(main_document_specific_result);

  // Call the recursive function that adds the reasons from the subtree to the
  // flattened list, and return the tree if needed.
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> result_tree;
  if (rfh->IsInPrimaryMainFrame() || main_frame_in_bfcache) {
    NotRestoredReasonBuilder builder(rfh, include_non_sticky);
    result_tree = builder.GetTreeResult();
    flattened_result.AddReasonsFrom(builder.GetFlattenedResult());
  } else {
    result_tree = BackForwardCacheCanStoreTreeResult::CreateEmptyTree(rfh);
  }
  // |result_tree| does not have main document specific reasons such as
  // "disabled via command line", and we have to manually add them.
  result_tree->AddReasonsToSubtreeRootFrom(main_document_specific_result);
  return result_tree;
}

void BackForwardCacheImpl::PopulateReasonsForMainDocument(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* rfh) {
  bool main_frame_in_bfcache =
      rfh->IsInBackForwardCache() && rfh->IsOutermostMainFrame();
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
  // If the |rfh| is not an "active" RenderFrameHost anymore, the
  // "RelatedActiveContentsCount" below is compared against 0, not 1. This is
  // because |rfh| is not "active" itself.
  //
  // This check makes sure the old and new document aren't sharing the same
  // BrowsingInstance. Note that the existence of related active contents might
  // change in the future, but we are checking this in
  // GetFutureBackForwardCacheEligibilityPotential instead of
  // GetCurrentBackForwardCacheEligibility because it's needed to determine
  // whether to do a proactive BrowsingInstance swap or not, which should not be
  // done if the page has related active contents.
  unsigned expected_related_active_contents_count = is_active_rfh ? 1 : 0;
  // We should never have fewer than expected.
  DCHECK_GE(rfh->GetSiteInstance()->GetRelatedActiveContentsCount(),
            expected_related_active_contents_count);
  if (rfh->GetSiteInstance()->GetRelatedActiveContentsCount() >
      expected_related_active_contents_count) {
    absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result;
    if (auto* metrics = rfh->GetBackForwardCacheMetrics())
      browsing_instance_swap_result = metrics->browsing_instance_swap_result();
    result.NoDueToRelatedActiveContents(browsing_instance_swap_result);
  }

  // Only store documents that have successful http status code.
  // Note that for error pages, |last_http_status_code| is equal to 0.
  if (rfh->last_http_status_code() != net::HTTP_OK)
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK);

  // Interstitials and other internal error pages should set an error status
  // code but there's no guarantee, e.g. https://crbug/1274308,
  // https://crbug/1287996. This catches those cases. It might also make the
  // kHTTPStatusNotOK check redundant.
  if (rfh->IsErrorDocument())
    result.No(BackForwardCacheMetrics::NotRestoredReason::kErrorDocument);

  // Only store documents that were fetched via HTTP GET method.
  if (rfh->last_http_method() != net::HttpRequestHeaders::kGetMethod)
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPMethodNotGET);

  // Only store documents that have a valid network::mojom::URLResponseHead.
  // We actually don't know the actual case this reason is solely set without
  // kHTTPStatusNotOK and kSchemeNotHTTPOrHTTPS, but crash reports imply it
  // happens.
  // TODO(https://crbug.com/1216997): Understand the case and remove
  // DebugScenario::kDebugNoResponseHeadForHTTPOrHTTPS.
  if (!rfh->last_response_head())
    result.No(BackForwardCacheMetrics::NotRestoredReason::kNoResponseHead);

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
  // TODO(rakina): Once we move cache-control tracking to RenderFrameHostImpl,
  // change this part to use the information stored in RenderFrameHostImpl
  // instead.

  if (rfh->GetBackForwardCacheDisablingFeatures().Has(
          WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore)) {
    if (!AllowStoringPagesWithCacheControlNoStore()) {
      // Block pages with cache-control: no-store only when
      // |should_cache_control_no_store_enter| flag is false. If true, put the
      // page in and evict later.
      result.NoDueToFeatures(
          WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
    }
  }

  // Only store documents that have URLs allowed through experiment.
  if (!IsAllowed(rfh->GetLastCommittedURL()))
    result.No(BackForwardCacheMetrics::NotRestoredReason::kDomainNotAllowed);
}

void BackForwardCacheImpl::NotRestoredReasonBuilder::
    PopulateStickyReasonsForDocument(
        BackForwardCacheCanStoreDocumentResult& result,
        RenderFrameHostImpl* rfh) {
  // If the rfh has ever granted media access, prevent it from entering cache.
  // TODO(crbug.com/989379): Consider only blocking when there's an active
  //                         media stream.
  if (rfh->was_granted_media_access()) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kWasGrantedMediaAccess);
  }

  if (rfh->IsBackForwardCacheDisabled() && !ShouldIgnoreBlocklists()) {
    result.NoDueToDisableForRenderFrameHostCalled(
        rfh->back_forward_cache_disabled_reasons());
  }

  // Do not store documents if they have inner WebContents. Inner frame trees
  // that are based on MPArch are allowed to be stored. To determine if this
  // is an inner WebContents we check the inner frame tree's type to see if
  // it is `kPrimary`.
  if (rfh->frame_tree()->delegate()->GetOuterDelegateFrameTreeNodeId() !=
          FrameTreeNode::kFrameTreeNodeInvalidId &&
      rfh->frame_tree()->type() == FrameTree::Type::kPrimary) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHaveInnerContents);
  }

#if !BUILDFLAG(IS_ANDROID)
  const bool has_unload_handler = rfh->has_unload_handler();
  if (has_unload_handler) {
    // Note that pages with unload handlers are cached on android.
    result.No(rfh->GetParent() ? BackForwardCacheMetrics::NotRestoredReason::
                                     kUnloadHandlerExistsInSubFrame
                               : BackForwardCacheMetrics::NotRestoredReason::
                                     kUnloadHandlerExistsInMainFrame);
  }
#endif

  // When it's not the final decision for putting a page in the back-forward
  // cache, we should only consider "sticky" features here - features that
  // will always result in a page becoming ineligible for back-forward cache
  // since the first time it's used.
  WebSchedulerTrackedFeatures banned_features =
      Intersection(GetDisallowedFeatures(RequestedFeatures::kOnlySticky),
                   rfh->GetBackForwardCacheDisablingFeatures());
  if (!banned_features.Empty()) {
    if (!ShouldIgnoreBlocklists()) {
      result.NoDueToFeatures(banned_features);
    }
  }
  // If the main document had CCNS and this document is same-origin with the
  // main document and used the "Authorization" header then add that reason.
  // This does not use `IsSameOriginForTreeResult` because we want to be more
  // conservative and react to *any* same-origin frame using it.
  if (root_rfh_->GetBackForwardCacheDisablingFeatures().Has(
          WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore) &&
      rfh->GetLastCommittedOrigin().IsSameOriginWith(
          root_rfh_->GetLastCommittedOrigin()) &&
      rfh->GetBackForwardCacheDisablingFeatures().Has(
          WebSchedulerTrackedFeature::kAuthorizationHeader)) {
    result.NoDueToFeatures(WebSchedulerTrackedFeature::kAuthorizationHeader);
  }
}

void BackForwardCacheImpl::NotRestoredReasonBuilder::
    PopulateNonStickyReasonsForDocument(
        BackForwardCacheCanStoreDocumentResult& result,
        RenderFrameHostImpl* rfh) {
  if (!rfh->IsDOMContentLoaded())
    result.No(BackForwardCacheMetrics::NotRestoredReason::kLoading);

  // Check for banned features currently being used. Note that unlike the check
  // in CanStoreRenderFrameHostLater, we are checking all banned features here
  // (not only the "sticky" features), because this time we're making a decision
  // on whether we should store a page in the back-forward cache or not.
  WebSchedulerTrackedFeatures banned_features =
      Intersection(GetDisallowedFeatures(RequestedFeatures::kAll),
                   rfh->GetBackForwardCacheDisablingFeatures());
  if (!banned_features.Empty() && !ShouldIgnoreBlocklists() &&
      rfh->render_view_host()->DidReceiveBackForwardCacheAck()) {
    result.NoDueToFeatures(banned_features);
  }

  // Do not cache if we have navigations in any of the subframes.
  if (rfh->GetParentOrOuterDocument() &&
      rfh->frame_tree_node()->HasNavigation()) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating);
  }
}

void BackForwardCacheImpl::NotRestoredReasonBuilder::PopulateReasonsForDocument(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* rfh,
    bool include_non_sticky) {
  PopulateStickyReasonsForDocument(result, rfh);
  if (include_non_sticky) {
    PopulateNonStickyReasonsForDocument(result, rfh);
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
      /* include_non_sticky = */ false,
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
    bool include_non_sticky)
    : NotRestoredReasonBuilder(root_rfh,
                               include_non_sticky,
                               /* eviction_info = */ absl::nullopt) {}

BackForwardCacheImpl::NotRestoredReasonBuilder::NotRestoredReasonBuilder(
    RenderFrameHostImpl* root_rfh,
    bool include_non_sticky,
    absl::optional<EvictionInfo> eviction_info)
    : root_rfh_(root_rfh),
      bfcache_(root_rfh_->frame_tree_node()
                   ->navigator()
                   .controller()
                   .GetBackForwardCache()),
      include_non_sticky_(include_non_sticky),
      eviction_info_(eviction_info) {
  // |root_rfh_| should be either primary main frame or back/forward cached
  // page's outermost main frame.
  DCHECK(
      root_rfh_->IsInPrimaryMainFrame() ||
      (root_rfh_->IsInBackForwardCache() && root_rfh_->IsOutermostMainFrame()));
  // Populate the reasons and build the tree.
  std::map<RenderFrameHostImpl*, BackForwardCacheCanStoreTreeResult*>
      parent_map;
  root_rfh_->ForEachRenderFrameHost([&](RenderFrameHostImpl* rfh) {
    auto rfh_result = PopulateReasons(rfh);
    parent_map[rfh] = rfh_result.get();

    if (rfh == root_rfh_) {
      tree_result_ = std::move(rfh_result);
    } else {
      RenderFrameHostImpl* parent = rfh->GetParentOrOuterDocumentOrEmbedder();
      // TODO(https://crbug.com/1257276): parent can return null for unattached
      // guests.
      if (!parent)
        parent = root_rfh_;
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
    PopulateReasonsForDocument(result_for_rfh, rfh, include_non_sticky_);
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
  if (AllowStoringPagesWithCacheControlNoStore()) {
    if (entry->render_frame_host()->GetBackForwardCacheDisablingFeatures().Has(
            WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore)) {
      // Start monitoring the cookie change only when cache-control:no-store
      // header is present.
      entry->StartMonitoringCookieChange();
    }
  }
  entry->SetStoredPageDelegate(this);
  entries_.push_front(std::move(entry));
  AddProcessesForEntry(*entries_.front());
  EnforceCacheSizeLimit();
}

void BackForwardCacheImpl::EnforceCacheSizeLimit() {
  if (!IsBackForwardCacheEnabled())
    return;

  if (UsingForegroundBackgroundCacheSizeLimit()) {
    // First enforce the foregrounded limit. The idea is that we need to
    // strictly enforce the limit on pages using foregrounded processes because
    // Android will not kill a foregrounded process, however it will kill a
    // backgrounded process if there is memory pressue, so we can allow more of
    // those to be kept in the cache.
    EnforceCacheSizeLimitInternal(GetForegroundedEntriesCacheSize(),
                                  /*foregrounded_only=*/true);
  }
  EnforceCacheSizeLimitInternal(GetCacheSize(),
                                /*foregrounded_only=*/false);
}

void BackForwardCacheImpl::Prune(size_t limit) {
  EnforceCacheSizeLimitInternal(limit,
                                /*foregrounded_only=*/false);
}

size_t BackForwardCacheImpl::EnforceCacheSizeLimitInternal(
    size_t limit,
    bool foregrounded_only) {
  size_t count = 0;
  size_t not_received_ack_count = 0;
  for (auto& stored_entry : entries_) {
    if (stored_entry->render_frame_host()->is_evicted_from_back_forward_cache())
      continue;
    if (foregrounded_only && !HasForegroundedProcess(*stored_entry))
      continue;
    if (!AllRenderViewHostsReceivedAckFromRenderer(*stored_entry)) {
      not_received_ack_count++;
      continue;
    }
    if (++count > limit) {
      stored_entry->render_frame_host()->EvictFromBackForwardCacheWithReason(
          foregrounded_only
              ? BackForwardCacheMetrics::NotRestoredReason::
                    kForegroundCacheLimit
              : BackForwardCacheMetrics::NotRestoredReason::kCacheLimit);
    }
  }
  UMA_HISTOGRAM_COUNTS_100(
      "BackForwardCache.AllSites.HistoryNavigationOutcome."
      "CountEntriesWithoutRendererAck",
      not_received_ack_count);
  return count;
}

std::unique_ptr<BackForwardCacheImpl::Entry> BackForwardCacheImpl::RestoreEntry(
    int navigation_entry_id,
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  TRACE_EVENT0("navigation", "BackForwardCache::RestoreEntry");

  // Select the RenderFrameHostImpl matching the navigation entry.
  auto matching_entry = base::ranges::find(
      entries_, navigation_entry_id, [](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host()->nav_entry_id();
      });

  // Not found.
  if (matching_entry == entries_.end())
    return nullptr;

  // Don't restore an evicted frame.
  if ((*matching_entry)
          ->render_frame_host()
          ->is_evicted_from_back_forward_cache())
    return nullptr;

  std::unique_ptr<Entry> entry = std::move(*matching_entry);
  TRACE_EVENT_INSTANT("navigation",
                      "BackForwardCache::RestoreEntry_matched_entry", "entry",
                      entry);

  entry->SetStoredPageDelegate(nullptr);
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
  TRACE_EVENT0("navigation", "BackForwardCache::Flush");
  for (std::unique_ptr<Entry>& entry : entries_) {
    entry->render_frame_host()->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::kCacheFlushed);
  }
}

void BackForwardCacheImpl::Shutdown() {
  if (UsingForegroundBackgroundCacheSizeLimit()) {
    for (auto& entry : entries_)
      RemoveProcessesForEntry(*entry.get());
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
    absl::optional<ukm::SourceId> source_id) {
  DisableForRenderFrameHost(render_frame_host->GetGlobalId(), reason,
                            source_id);
}

// static
void BackForwardCache::DisableForRenderFrameHost(
    GlobalRenderFrameHostId id,
    DisabledReason reason,
    absl::optional<ukm::SourceId> source_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_bfcache_disabled_test_observer)
    g_bfcache_disabled_test_observer->OnDisabledForFrameWithReason(id, reason);

  if (auto* rfh = RenderFrameHostImpl::FromID(id))
    rfh->DisableBackForwardCache(reason, source_id);
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

BackForwardCacheImpl::Entry* BackForwardCacheImpl::GetEntry(
    int navigation_entry_id) {
  auto matching_entry = base::ranges::find(
      entries_, navigation_entry_id, [](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host()->nav_entry_id();
      });

  if (matching_entry == entries_.end())
    return nullptr;

  auto* render_frame_host = (*matching_entry)->render_frame_host();
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
  }

  // Don't return the frame if it is evicted.
  if (render_frame_host->is_evicted_from_back_forward_cache())
    return nullptr;

  return (*matching_entry).get();
}

void BackForwardCacheImpl::RenderViewHostNoLongerStored(
    RenderViewHostImpl* rvh) {
  // `AddProcessesForEntry` are gated on
  // `UsingForegroundBackgroundCacheSizeLimit` in adding entries to the
  // `observed_processes_` list so we have the same conditional here.
  if (!UsingForegroundBackgroundCacheSizeLimit())
    return;
  RenderViewHostNoLongerStoredInternal(rvh);
}

void BackForwardCacheImpl::RenderViewHostNoLongerStoredInternal(
    RenderViewHostImpl* rvh) {
  RenderProcessHostImpl* process =
      static_cast<RenderProcessHostImpl*>(rvh->GetProcess());
  // Remove 1 instance of this process from the multiset.
  observed_processes_.erase(observed_processes_.find(process));
  if (observed_processes_.find(process) == observed_processes_.end())
    process->RemoveInternalObserver(this);
}

void BackForwardCacheImpl::AddProcessesForEntry(Entry& entry) {
  if (!UsingForegroundBackgroundCacheSizeLimit())
    return;
  for (const auto& rvh : entry.render_view_hosts()) {
    RenderProcessHostImpl* process =
        static_cast<RenderProcessHostImpl*>(rvh->GetProcess());
    if (observed_processes_.find(process) == observed_processes_.end())
      process->AddInternalObserver(this);
    observed_processes_.insert(process);
  }
}

void BackForwardCacheImpl::RemoveProcessesForEntry(Entry& entry) {
  if (!UsingForegroundBackgroundCacheSizeLimit())
    return;
  for (const auto& rvh : entry.render_view_hosts()) {
    RenderViewHostNoLongerStoredInternal(&*rvh);
  }
}

void BackForwardCacheImpl::DestroyEvictedFrames() {
  TRACE_EVENT0("navigation", "BackForwardCache::DestroyEvictedFrames");
  if (entries_.empty())
    return;

  base::EraseIf(entries_, [this](std::unique_ptr<Entry>& entry) {
    if (entry->render_frame_host()->is_evicted_from_back_forward_cache()) {
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
  const auto& it = blocked_urls_.find(current_url.host());
  if (it != blocked_urls_.end()) {
    for (const std::string& blocked_path : it->second) {
      if (base::StartsWith(current_url.path_piece(), blocked_path))
        return false;
    }
  }

  // By convention, when |allowed_urls_| is empty, it means there are no
  // restrictions about what RenderFrameHost can enter the BackForwardCache.
  if (allowed_urls_.empty())
    return true;

  // Checking for each url in the |allowed_urls_|, if the current_url matches
  // the corresponding host and path is the prefix of the allowed url path. We
  // only check for host and path and not any other components including url
  // scheme here.
  const auto& entry = allowed_urls_.find(current_url.host());
  if (entry != allowed_urls_.end()) {
    for (const std::string& allowed_path : entry->second) {
      if (base::StartsWith(current_url.path_piece(), allowed_path))
        return true;
    }
  }
  return false;
}

bool BackForwardCacheImpl::IsQueryAllowed(const GURL& current_url) {
  std::vector<std::string> cgi_params =
      base::SplitString(current_url.query_piece(), "&", base::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (const std::string& cgi_param : cgi_params) {
    if (base::Contains(blocked_cgi_params_, cgi_param))
      return false;
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
      base::BindOnce(
          [](base::TimeTicks ipc_start_time, base::OnceClosure cb) {
            std::move(cb).Run();
            base::UmaHistogramTimes(
                "BackForwardCache.Restore.DisableEvictionDelay",
                base::TimeTicks::Now() - ipc_start_time);
          },
          base::TimeTicks::Now(), std::move(done_callback)));

  for (const auto& rvh : bfcache_entry.render_view_hosts()) {
    rvh->PrepareToLeaveBackForwardCache(cb);
  }
}

bool BackForwardCacheImpl::AllowStoringPagesWithCacheControlNoStore() {
  return GetCacheControlNoStoreLevel() >
         CacheControlNoStoreExperimentLevel::kDoNotStore;
}

bool BackForwardCacheImpl::IsBrowsingInstanceInBackForwardCacheForDebugging(
    BrowsingInstanceId browsing_instance_id) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (entry->render_frame_host()
            ->GetSiteInstance()
            ->GetBrowsingInstanceId() == browsing_instance_id) {
      return true;
    }
  }
  return false;
}

bool BackForwardCacheImpl::IsProxyInBackForwardCacheForDebugging(
    RenderFrameProxyHost* proxy) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    for (auto& proxy_map_entry : entry->proxy_hosts()) {
      if (proxy_map_entry.second.get() == proxy) {
        return true;
      }
    }
  }
  return false;
}

bool BackForwardCacheImpl::IsMediaSessionServiceAllowed() {
  return base::FeatureList::IsEnabled(
      features::kBackForwardCacheMediaSessionService);
}

bool BackForwardCacheImpl::IsScreenReaderAllowed() {
  return base::FeatureList::IsEnabled(
      features::kEnableBackForwardCacheForScreenReader);
}

// static
void BackForwardCacheImpl::VlogUnexpectedRendererToBrowserMessage(
    const char* interface_name,
    uint32_t message_name,
    RenderFrameHostImpl* rfh) {
  VLOG(1) << "BackForwardCacheMessageFilter::WillDispatch bad_message "
          << "interface_name " << interface_name << " message_name "
          << message_name;
  // TODO(https://crbug.com/1379490): Remove these when bug is fixed.
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
bool BackForwardCache::DisabledReason::operator<(
    const DisabledReason& other) const {
  return std::tie(source, id) < std::tie(other.source, other.id);
}
bool BackForwardCache::DisabledReason::operator==(
    const DisabledReason& other) const {
  return std::tie(source, id) == std::tie(other.source, other.id);
}
bool BackForwardCache::DisabledReason::operator!=(
    const DisabledReason& other) const {
  return !(*this == other);
}

BackForwardCacheCanStoreTreeResult::BackForwardCacheCanStoreTreeResult(
    RenderFrameHostImpl* rfh,
    const url::Origin& main_document_origin,
    const GURL& url,
    BackForwardCacheCanStoreDocumentResult& result_for_this_document)
    : document_result_(std::move(result_for_this_document)),
      is_same_origin_(
          IsSameOriginForTreeResult(rfh, url, main_document_origin)),
      is_root_outermost_main_frame_(rfh->IsOutermostMainFrame()),
      id_(rfh->frame_tree_node()->html_id()),
      name_(rfh->frame_tree_node()->html_name()),
      src_(rfh->frame_tree_node()->html_src()),
      url_(url) {}

BackForwardCacheCanStoreTreeResult::BackForwardCacheCanStoreTreeResult(
    bool is_same_origin,
    const GURL& url)
    : document_result_(BackForwardCacheCanStoreDocumentResult()),
      is_same_origin_(is_same_origin),
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
  int index = count == 0 ? 0 : base::RandInt(0, count - 1);
  return GetWebExposedNotRestoredReasonsInternal(index);
}

blink::mojom::BackForwardCacheNotRestoredReasonsPtr
BackForwardCacheCanStoreTreeResult::GetWebExposedNotRestoredReasonsInternal(
    int& index) {
  blink::mojom::BackForwardCacheNotRestoredReasonsPtr not_restored_reasons =
      blink::mojom::BackForwardCacheNotRestoredReasons::New();
  if (IsSameOrigin()) {
    // Only include same_origin_details for documents that are same-origin with
    // the main document. Stop recursion as soon as we hit a cross-origin
    // document.
    not_restored_reasons->same_origin_details =
        blink::mojom::SameOriginBfcacheNotRestoredDetails::New();
    not_restored_reasons->same_origin_details->url = url_.spec();
    not_restored_reasons->same_origin_details->reasons =
        GetDocumentResult().GetStringReasons();

    not_restored_reasons->blocked = GetDocumentResult().CanRestore()
                                        ? blink::mojom::BFCacheBlocked::kNo
                                        : blink::mojom::BFCacheBlocked::kYes;
    for (const auto& subtree : GetChildren()) {
      not_restored_reasons->same_origin_details->children.push_back(
          subtree->GetWebExposedNotRestoredReasonsInternal(index));
    }
  } else {
    // If the subtree's root document is cross-origin from the main frame
    // document, and if this is the randomly selected cross-origin iframe,
    // report whether or not this entire subtree is blocking back/forward cache.
    if (index == 0) {
      not_restored_reasons->blocked =
          (!GetDocumentResult().CanRestore() || !FlattenTree().CanRestore())
              ? blink::mojom::BFCacheBlocked::kYes
              : blink::mojom::BFCacheBlocked::kNo;
    } else {
      not_restored_reasons->blocked = blink::mojom::BFCacheBlocked::kMasked;
    }
    // Decrease the index now that we saw a cross-origin iframe.
    index--;
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

uint32_t
BackForwardCacheCanStoreTreeResult::GetCrossOriginReachableFrameCount() {
  // If the document is cross-origin, we cannot reach any further. Only count
  // the one we have reached and return.
  if (!IsSameOrigin())
    return 1;
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
    ~BackForwardCacheCanStoreDocumentResultWithTree() = default;

}  // namespace content
