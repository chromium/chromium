// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/frame/event_page_show_persisted.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-shared.h"
#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace content {

class RenderProcessHostInternalObserver;

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
static constexpr int kDefaultTimeToLiveInBackForwardCacheInSeconds = 180;

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

enum class HeaderPresence {
  kNotPresent,
  kPresent,
  kUnsure,
};

constexpr base::FeatureParam<BackForwardCacheImpl::UnloadSupportStrategy>::
    Option kUnloadSupportStrategyOptions[] = {
        {BackForwardCacheImpl::UnloadSupportStrategy::kAlways, "always"},
        {BackForwardCacheImpl::UnloadSupportStrategy::kNo, "no"},
};

BackForwardCacheImpl::UnloadSupportStrategy GetUnloadSupportStrategy() {
  constexpr auto kDefaultStrategy =
#if BUILDFLAG(IS_ANDROID)
      BackForwardCacheImpl::UnloadSupportStrategy::kAlways;
#else
      BackForwardCacheImpl::UnloadSupportStrategy::kNo;
#endif

  if (!IsBackForwardCacheEnabled())
    return kDefaultStrategy;

  static constexpr base::FeatureParam<
      BackForwardCacheImpl::UnloadSupportStrategy>
      unload_support(&features::kBackForwardCache, "unload_support",
                     kDefaultStrategy, &kUnloadSupportStrategyOptions);
  return unload_support.Get();
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
    } else {
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

enum RequestedFeatures { kAll, kOnlySticky };

BlockListedFeatures GetDisallowedFeatures(
    RenderFrameHostImpl* rfh,
    RequestedFeatures requested_features) {
  // TODO(https://crbug.com/1015784): Finalize disallowed feature list, and test
  // for each disallowed feature.
  constexpr WebSchedulerTrackedFeatures kAlwaysDisallowedFeatures(
      WebSchedulerTrackedFeature::kAppBanner,
      WebSchedulerTrackedFeature::kBroadcastChannel,
      WebSchedulerTrackedFeature::kContainsPlugins,
      WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet,
      WebSchedulerTrackedFeature::kDummy,
      WebSchedulerTrackedFeature::kIdleManager,
      WebSchedulerTrackedFeature::kIndexedDBConnection,
      WebSchedulerTrackedFeature::kKeyboardLock,
      WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction,
      WebSchedulerTrackedFeature::kPaymentManager,
      WebSchedulerTrackedFeature::kPictureInPicture,
      WebSchedulerTrackedFeature::kPortal,
      WebSchedulerTrackedFeature::kPrinting,
      WebSchedulerTrackedFeature::kRequestedAudioCapturePermission,
      WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors,
      WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission,
      WebSchedulerTrackedFeature::kRequestedMIDIPermission,
      WebSchedulerTrackedFeature::kRequestedNotificationsPermission,
      WebSchedulerTrackedFeature::kRequestedVideoCapturePermission,
      WebSchedulerTrackedFeature::kSharedWorker,
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

  WebSchedulerTrackedFeatures result = kAlwaysDisallowedFeatures;

  if (!IsContentInjectionSupported()) {
    result.Put(WebSchedulerTrackedFeature::kInjectedJavascript);
    result.Put(WebSchedulerTrackedFeature::kInjectedStyleSheet);
  }

  if (!IgnoresOutstandingNetworkRequestForTesting()) {
    result.Put(WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers);
    result.Put(WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch);
    result.Put(WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR);
  }

  if (requested_features == RequestedFeatures::kOnlySticky) {
    // Remove all non-sticky features from |result|.
    result = Intersection(result, blink::scheduler::StickyFeatures());
  }

  result.RemoveAll(SupportedFeatures());

  return result;
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
    auto* trigger = rfh->GetRenderWidgetHost()->GetVisibleTimeRequestTrigger();
    // The only way this should be null is if there is no RenderWidgetHostView.
    DCHECK(rfh->GetView());
    DCHECK(trigger);
    trigger->UpdateRequest(navigation_start, false /* destination_is_loaded */,
                           false /* show_reason_tab_switching */,
                           false /* show_reason_unoccluded */,
                           true /* show_reason_bfcache_restore */);
  }
}

// Returns true if any of the processes associated with the RenderViewHosts in
// this Entry are foregrounded.
bool HasForegroundedProcess(BackForwardCacheImpl::Entry& entry) {
  for (auto* rvh : entry.render_view_hosts()) {
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
  for (auto* rvh : entry.render_view_hosts()) {
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

}  // namespace

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
  RenderFrameHostImpl* rfh = stored_page_->render_frame_host.get();
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
      unload_strategy_(GetUnloadSupportStrategy()),
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
  // - The programmatical value set in params. Used in specific tests.
  //   The TTL set in BackForwardCacheTimeToLiveControl takes precedence over
  //   the TTL set in the main BackForwardCache feature if both are present.
  // - Infinite if kBackForwardCacheNoTimeEviction is enabled.
  // - Default value otherwise, kDefaultTimeToLiveInBackForwardCacheInSeconds.

  if (base::FeatureList::IsEnabled(kBackForwardCacheTimeToLiveControl)) {
    absl::optional<int> time_to_live = GetFieldTrialParamByFeatureAsOptionalInt(
        kBackForwardCacheTimeToLiveControl, "time_to_live_seconds");
    if (time_to_live.has_value()) {
      return base::Seconds(time_to_live.value());
    }
  }

  absl::optional<int> old_time_to_live =
      GetFieldTrialParamByFeatureAsOptionalInt(
          features::kBackForwardCache, "TimeToLiveInBackForwardCacheInSeconds");
  if (old_time_to_live.has_value()) {
    return base::Seconds(old_time_to_live.value());
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
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "cache_size", kDefaultBackForwardCacheSize);
}

// static
size_t BackForwardCacheImpl::GetForegroundedEntriesCacheSize() {
  if (!IsBackForwardCacheEnabled())
    return 0;
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
  // |CanStorePageNow()|, at which point |rfh| may not have a matching entry
  // yet.
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

BackForwardCacheCanStoreDocumentResultWithTree
BackForwardCacheImpl::CanStorePageNow(RenderFrameHostImpl* rfh,
                                      bool include_ccns) {
  BackForwardCacheCanStoreDocumentResult flattened_result;
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree =
      PopulateReasonsForPage(rfh, flattened_result,
                             /*include_non_sticky=*/true,
                             /*create_tree=*/true);

  // TODO(https://crbug.com/1280150): Call
  // UpdateCanStoreToIncludeCacheControlNoStore() for tree structure.
  // Include cache-control:no-store related reasons only when requested.
  if (include_ccns) {
    DCHECK(AllowStoringPagesWithCacheControlNoStore());
    UpdateCanStoreToIncludeCacheControlNoStore(flattened_result, rfh);
  }
  DVLOG(1) << "CanStorePageNow: " << rfh->GetLastCommittedURL() << " : "
           << flattened_result.ToString();
  TRACE_EVENT("navigation", "BackForwardCacheImpl::CanPotentiallyStorePageNow",
              ChromeTrackEvent::kBackForwardCacheCanStoreDocumentResult,
              flattened_result);

  return BackForwardCacheCanStoreDocumentResultWithTree(flattened_result,
                                                        std::move(tree));
}

BackForwardCacheCanStoreDocumentResult
BackForwardCacheImpl::CanPotentiallyStorePageLater(RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult result;
  PopulateReasonsForPage(rfh, result,
                         /*include_non_sticky = */ false,
                         /*create_tree = */ false);
  DVLOG(1) << "CanPotentiallyStorePageLater: " << rfh->GetLastCommittedURL()
           << " : " << result.ToString();
  TRACE_EVENT(
      "navigation", "BackForwardCacheImpl::CanPotentiallyStorePageLater",
      ChromeTrackEvent::kBackForwardCacheCanStoreDocumentResult, result);
  return result;
}

std::unique_ptr<BackForwardCacheCanStoreTreeResult>
BackForwardCacheImpl::PopulateReasonsForPage(
    RenderFrameHostImpl* rfh,
    BackForwardCacheCanStoreDocumentResult& flattened_result,
    bool include_non_sticky,
    bool create_tree) {
  // TODO(crbug.com/1275977): This function should only be called when |rfh| is
  // the primary main frame. Fix |ShouldProactivelySwapBrowsingInstance()| and
  // |UnloadOldFrame()| so that it will not check bfcache eligibility if not
  // primary main frame.
  BackForwardCacheCanStoreDocumentResult main_document_specific_result;
  // This function can be called during eviction, and |rfh| can be in
  // back/forward cache, which is considered as non primary main frame.
  bool main_frame_in_bfcache =
      rfh->IsInBackForwardCache() && rfh->is_main_frame();

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
    NotRestoredReasonBuilder builder(rfh, include_non_sticky, create_tree);
    result_tree = builder.GetTreeResult();
    flattened_result.AddReasonsFrom(builder.GetFlattenedResult());
  } else {
    result_tree = BackForwardCacheCanStoreTreeResult::CreateEmptyTree(rfh);
  }
  if (!create_tree)
    return nullptr;
  // |result_tree| does not have main document specific reasons such as
  // "disabled via command line", and we have to manually add them.
  result_tree->AddReasonsToSubtreeRootFrom(main_document_specific_result);
  return result_tree;
}

void BackForwardCacheImpl::PopulateReasonsForMainDocument(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* rfh) {
  bool main_frame_in_bfcache =
      rfh->IsInBackForwardCache() && rfh->is_main_frame();
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
  // CanPotentiallyStorePageLater instead of CanStorePageNow because it's needed
  // to determine whether to do a proactive BrowsingInstance swap or not, which
  // should not be done if the page has related active contents.
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
  // CanPotentiallyStorePageLater as it's not possible to change the HTTP
  // headers, so if it's not possible to cache this page now due to this, it's
  // impossible to cache this page later.
  // TODO(rakina): Once we move cache-control tracking to RenderFrameHostImpl,
  // change this part to use the information stored in RenderFrameHostImpl
  // instead.

  BlockListedFeatures cache_control_no_store_feature(
      WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
  if (!Intersection(rfh->GetBackForwardCacheDisablingFeatures(),
                    cache_control_no_store_feature)
           .Empty()) {
    if (!AllowStoringPagesWithCacheControlNoStore()) {
      // Block pages with cache-control: no-store only when
      // |should_cache_control_no_store_enter| flag is false. If true, put the
      // page in and evict later.
      result.NoDueToFeatures(cache_control_no_store_feature);
    }
  }

  // Only store documents that have URLs allowed through experiment.
  if (!IsAllowed(rfh->GetLastCommittedURL()))
    result.No(BackForwardCacheMetrics::NotRestoredReason::kDomainNotAllowed);
}

void BackForwardCacheImpl::PopulateStickyReasonsForDocument(
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

  // Do not store documents if they have inner WebContents.
  if (rfh->inner_tree_main_frame_tree_node_id() !=
      FrameTreeNode::kFrameTreeNodeInvalidId) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHaveInnerContents);
  }

  const bool has_unload_handler = rfh->has_unload_handler();
  DCHECK(!has_unload_handler || !rfh->IsNestedWithinFencedFrame());
  switch (unload_strategy_) {
    case BackForwardCacheImpl::UnloadSupportStrategy::kAlways:
      break;
    case BackForwardCacheImpl::UnloadSupportStrategy::kOptInHeaderRequired:
    case BackForwardCacheImpl::UnloadSupportStrategy::kNo:
      if (has_unload_handler) {
        result.No(rfh->GetParent()
                      ? BackForwardCacheMetrics::NotRestoredReason::
                            kUnloadHandlerExistsInSubFrame
                      : BackForwardCacheMetrics::NotRestoredReason::
                            kUnloadHandlerExistsInMainFrame);
      }
      break;
  }

  // When it's not the final decision for putting a page in the back-forward
  // cache, we should only consider "sticky" features here - features that
  // will always result in a page becoming ineligible for back-forward cache
  // since the first time it's used.
  WebSchedulerTrackedFeatures banned_features =
      Intersection(GetDisallowedFeatures(rfh, RequestedFeatures::kOnlySticky),
                   rfh->GetBackForwardCacheDisablingFeatures());
  if (!banned_features.Empty()) {
    if (!ShouldIgnoreBlocklists()) {
      result.NoDueToFeatures(banned_features);
    }
  }
}

void BackForwardCacheImpl::PopulateNonStickyReasonsForDocument(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* rfh) {
  if (!rfh->IsDOMContentLoaded())
    result.No(BackForwardCacheMetrics::NotRestoredReason::kLoading);

  // Check for banned features currently being used. Note that unlike the check
  // in CanStoreRenderFrameHostLater, we are checking all banned features here
  // (not only the "sticky" features), because this time we're making a decision
  // on whether we should store a page in the back-forward cache or not.
  WebSchedulerTrackedFeatures banned_features =
      Intersection(GetDisallowedFeatures(rfh, RequestedFeatures::kAll),
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

  // TODO(https://crbug.com/1251387): Frames embedding FencedFrames are not
  // supported.
  if (!rfh->GetFencedFrames().empty()) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kFencedFramesEmbedder);
  }
}

void BackForwardCacheImpl::PopulateReasonsForDocument(
    BackForwardCacheCanStoreDocumentResult& result,
    RenderFrameHostImpl* rfh,
    bool include_non_sticky) {
  PopulateStickyReasonsForDocument(result, rfh);
  if (include_non_sticky) {
    PopulateNonStickyReasonsForDocument(result, rfh);
  }
}

std::unique_ptr<BackForwardCacheCanStoreTreeResult>
BackForwardCacheImpl::CreateEvictionBackForwardCacheCanStoreTreeResult(
    RenderFrameHostImpl& rfh,
    BackForwardCacheCanStoreDocumentResult& eviction_reason) {
  BackForwardCacheImpl::NotRestoredReasonBuilder builder(
      rfh.GetMainFrame(),
      /* include_non_sticky = */ false,
      /* create_tree = */ true,
      BackForwardCacheImpl::NotRestoredReasonBuilder::EvictionInfo(
          rfh, &eviction_reason));
  return builder.GetTreeResult();
}

BackForwardCacheImpl::NotRestoredReasonBuilder::NotRestoredReasonBuilder(
    RenderFrameHostImpl* root_rfh,
    bool include_non_sticky,
    bool create_tree)
    : NotRestoredReasonBuilder(root_rfh,
                               include_non_sticky,
                               create_tree,
                               /* eviction_info = */ absl::nullopt) {}

BackForwardCacheImpl::NotRestoredReasonBuilder::NotRestoredReasonBuilder(
    RenderFrameHostImpl* root_rfh,
    bool include_non_sticky,
    bool create_tree,
    absl::optional<EvictionInfo> eviction_info)
    : root_rfh_(root_rfh),
      bfcache_(root_rfh_->frame_tree_node()
                   ->navigator()
                   .controller()
                   .GetBackForwardCache()),
      include_non_sticky_(include_non_sticky),
      create_tree_(create_tree),
      eviction_info_(eviction_info) {
  // |root_rfh_| should be either primary main frame or back/forward cached
  // page's main frame.
  DCHECK(root_rfh_->IsInPrimaryMainFrame() ||
         (root_rfh_->IsInBackForwardCache() && root_rfh_->is_main_frame()));
  // Populate the reasons and build the tree if needed.
  tree_result_ = PopulateReasonsAndReturnSubtreeIfNeededFor(root_rfh_);
}

BackForwardCacheImpl::NotRestoredReasonBuilder::~NotRestoredReasonBuilder() =
    default;

std::unique_ptr<BackForwardCacheCanStoreTreeResult> BackForwardCacheImpl::
    NotRestoredReasonBuilder::PopulateReasonsAndReturnSubtreeIfNeededFor(
        RenderFrameHostImpl* rfh) {
  // TODO(https://crbug.com/1280150): Add cache-control:no-store reasons to the
  // tree.

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
    bfcache_.PopulateReasonsForDocument(result_for_rfh, rfh,
                                        include_non_sticky_);
  }
  flattened_result_.AddReasonsFrom(result_for_rfh);

  // Finds the reasons recursively and create the reason subtree for the
  // children if needed.
  BackForwardCacheCanStoreTreeResult::ChildrenVector children_result;
  for (size_t i = 0; i < rfh->child_count(); i++) {
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> child =
        PopulateReasonsAndReturnSubtreeIfNeededFor(
            rfh->child_at(i)->current_frame_host());
    if (create_tree_) {
      children_result.emplace_back(std::move(child));
    }
  }

  if (!create_tree_)
    return nullptr;

  std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree(
      new BackForwardCacheCanStoreTreeResult(
          rfh, root_rfh_->GetLastCommittedOrigin(), result_for_rfh,
          std::move(children_result)));
  return tree;
}

void BackForwardCacheImpl::StoreEntry(
    std::unique_ptr<BackForwardCacheImpl::Entry> entry) {
  TRACE_EVENT("navigation", "BackForwardCache::StoreEntry", "entry", entry);
  BackForwardCacheCanStoreDocumentResultWithTree result =
      CanStorePageNow(entry->render_frame_host());
  DCHECK(result);

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
  blink::RecordUMAEventPageShowPersisted(
      blink::EventPageShowPersisted::
          kYesInBrowser_BackForwardCache_RestoreEntry_Attempt);

  // Select the RenderFrameHostImpl matching the navigation entry.
  auto matching_entry =
      std::find_if(entries_.begin(), entries_.end(),
                   [navigation_entry_id](std::unique_ptr<Entry>& entry) {
                     return entry->render_frame_host()->nav_entry_id() ==
                            navigation_entry_id;
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

  entries_.erase(matching_entry);
  RemoveProcessesForEntry(*entry);
  base::TimeTicks start_time = page_restore_params->navigation_start;
  entry->SetPageRestoreParams(std::move(page_restore_params));
  RequestRecordTimeToVisible(entry->render_frame_host(), start_time);
  entry->render_frame_host()->WillLeaveBackForwardCache();

  RestoreBrowserControlsState(entry->render_frame_host());

  blink::RecordUMAEventPageShowPersisted(
      blink::EventPageShowPersisted::
          kYesInBrowser_BackForwardCache_RestoreEntry_Succeed);
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
bool BackForwardCache::IsSameSiteBackForwardCacheFeatureEnabled() {
  return IsSameSiteBackForwardCacheEnabled();
}

// static
void BackForwardCache::DisableForRenderFrameHost(
    RenderFrameHost* render_frame_host,
    BackForwardCache::DisabledReason reason) {
  DisableForRenderFrameHost(render_frame_host->GetGlobalId(), reason);
}

// static
void BackForwardCache::ClearDisableReasonForRenderFrameHost(
    GlobalRenderFrameHostId id,
    BackForwardCache::DisabledReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (auto* rfh = RenderFrameHostImpl::FromID(id))
    rfh->ClearDisableBackForwardCache(reason);
}

// static
void BackForwardCache::DisableForRenderFrameHost(
    GlobalRenderFrameHostId id,
    BackForwardCache::DisabledReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_bfcache_disabled_test_observer)
    g_bfcache_disabled_test_observer->OnDisabledForFrameWithReason(id, reason);

  if (auto* rfh = RenderFrameHostImpl::FromID(id))
    rfh->DisableBackForwardCache(reason);
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
  auto matching_entry =
      std::find_if(entries_.begin(), entries_.end(),
                   [navigation_entry_id](std::unique_ptr<Entry>& entry) {
                     return entry->render_frame_host()->nav_entry_id() ==
                            navigation_entry_id;
                   });

  if (matching_entry == entries_.end())
    return nullptr;

  if (AllowStoringPagesWithCacheControlNoStore() &&
      (*matching_entry)
          ->render_frame_host()
          ->GetBackForwardCacheDisablingFeatures()
          .Has(WebSchedulerTrackedFeature::
                   kMainResourceHasCacheControlNoStore)) {
    auto* render_frame_host = (*matching_entry)->render_frame_host();
    // If we are in the experiments to allow pages with cache-control:no-store
    // in back/forward cache and the page has cache-control:no-store, we should
    // record them as reasons.
    BackForwardCacheCanStoreDocumentResultWithTree can_store =
        CanStorePageNow(render_frame_host, /* include_ccns = */ true);
    if (!can_store) {
      (*matching_entry)
          ->render_frame_host()
          ->EvictFromBackForwardCacheWithFlattenedAndTreeReasons(can_store);
    }
  }

  // Don't return the frame if it is evicted.
  if ((*matching_entry)
          ->render_frame_host()
          ->is_evicted_from_back_forward_cache())
    return nullptr;

  return (*matching_entry).get();
}

void BackForwardCacheImpl::AddProcessesForEntry(Entry& entry) {
  if (!UsingForegroundBackgroundCacheSizeLimit())
    return;
  for (auto* rvh : entry.render_view_hosts()) {
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
  for (auto* rvh : entry.render_view_hosts()) {
    RenderProcessHostImpl* process =
        static_cast<RenderProcessHostImpl*>(rvh->GetProcess());
    // Remove 1 instance of this process from the multiset.
    observed_processes_.erase(observed_processes_.find(process));
    if (observed_processes_.find(process) == observed_processes_.end())
      process->RemoveInternalObserver(this);
  }
}

void BackForwardCacheImpl::DestroyEvictedFrames() {
  TRACE_EVENT0("navigation", "BackForwardCache::DestroyEvictedFrames");
  if (entries_.empty())
    return;

  base::EraseIf(entries_, [this](std::unique_ptr<Entry>& entry) {
    if (entry->render_frame_host()->is_evicted_from_back_forward_cache()) {
      // We need to update the not restored reasons to include cache-control:
      // no-store related reasons before evicting. Because at this point, we
      // have not recorded cache-control:no-store related reasons so that the
      // page can temporarily enter bfcache.
      BackForwardCacheCanStoreDocumentResult can_store;
      UpdateCanStoreToIncludeCacheControlNoStore(can_store,
                                                 entry->render_frame_host());
      if (auto* metrics =
              entry->render_frame_host()->GetBackForwardCacheMetrics()) {
        metrics->MarkNotRestoredWithReason(can_store);
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

  blink::RecordUMAEventPageShowPersisted(
      blink::EventPageShowPersisted::
          kYesInBrowser_BackForwardCache_WillCommitNavigationToCachedEntry);
  for (auto* rvh : bfcache_entry.render_view_hosts()) {
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

bool BackForwardCacheImpl::IsMediaSessionPlaybackStateChangedAllowed() {
  return base::FeatureList::IsEnabled(
      kBackForwardCacheMediaSessionPlaybackStateChange);
}

bool BackForwardCacheImpl::IsMediaSessionServiceAllowed() {
  return base::FeatureList::IsEnabled(
      features::kBackForwardCacheMediaSessionService);
}

bool BackForwardCacheImpl::IsScreenReaderAllowed() {
  return base::FeatureList::IsEnabled(
      features::kEnableBackForwardCacheForScreenReader);
}

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
    BackForwardCacheCanStoreDocumentResult& result_for_this_document,
    BackForwardCacheCanStoreTreeResult::ChildrenVector children)
    : document_result_(std::move(result_for_this_document)),
      children_(std::move(children)),
      is_same_origin_(
          rfh->GetLastCommittedOrigin().IsSameOriginWith(main_document_origin)),
      url_(rfh->GetLastCommittedURL()) {}

BackForwardCacheCanStoreTreeResult::~BackForwardCacheCanStoreTreeResult() =
    default;

void BackForwardCacheCanStoreTreeResult::AddReasonsToSubtreeRootFrom(
    const BackForwardCacheCanStoreDocumentResult& result) {
  document_result_.AddReasonsFrom(result);
}

std::unique_ptr<BackForwardCacheCanStoreTreeResult>
BackForwardCacheCanStoreTreeResult::CreateEmptyTree(RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult empty_result;
  BackForwardCacheCanStoreTreeResult::ChildrenVector empty_vector;
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> empty_tree(
      new BackForwardCacheCanStoreTreeResult(rfh, rfh->GetLastCommittedOrigin(),
                                             empty_result,
                                             std::move(empty_vector)));
  return empty_tree;
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
