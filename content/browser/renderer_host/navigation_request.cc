// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_request.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/safe_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/state_transitions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/base_tracing.h"
#include "base/types/optional_util.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/agent_cluster_key.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browsing_topics/header_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/ad_auction_headers_util.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/loader/cached_navigation_url_loader.h"
#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/object_navigation_fallback_body_loader.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/network/cross_origin_embedder_policy_reporter.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/origin_trials/origin_trials_utils.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/preloading/prerender/prerender_navigation_utils.h"
#include "content/browser/process_lock.h"
#include "content/browser/reduce_accept_language/reduce_accept_language_utils.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/concurrent_navigations_commit_deferring_condition.h"
#include "content/browser/renderer_host/cookie_utils.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/renderer_host/navigation_state_keep_alive.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/page_delegate.h"
#include "content/browser/renderer_host/private_network_access_util.h"
#include "content/browser/renderer_host/render_frame_host_csp_context.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/scoped_view_transition_resources.h"
#include "content/browser/renderer_host/subframe_history_navigation_throttle.h"
#include "content/browser/renderer_host/system_entropy_utils.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/security/coop/cross_origin_opener_policy_reporter.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/shared_storage/shared_storage_header_observer.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_params_helper.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/debug_utils.h"
#include "content/common/features.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/peak_gpu_memory_tracker_factory.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/filename_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_access_result.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/redirect_info.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/supports_loading_mode/supports_loading_mode_parser.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/supports_loading_mode.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "services/network/public/mojom/web_client_hints_types.mojom.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/common/frame/fenced_frame_permissions_policies.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/navigation/navigation_params_mojom_traits.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/permissions_policy/document_policy.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/common/security/address_space_feature.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/loader/transferrable_url_loader.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-shared.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"
#include "third_party/blink/public/mojom/runtime_feature_state/runtime_feature.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "ui/compositor/compositor_lock.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#endif

namespace content {

namespace {

// Default timeout for the READY_TO_COMMIT -> COMMIT transition. Chosen
// initially based on the Navigation.ReadyToCommitUntilCommit UMA, and then
// refined based on feedback based on CrashExitCodes.Renderer/RESULT_CODE_HUNG.
constexpr base::TimeDelta kDefaultCommitTimeout = base::Seconds(30);

// Timeout for the READY_TO_COMMIT -> COMMIT transition.
// Overrideable via SetCommitTimeoutForTesting.
base::TimeDelta g_commit_timeout = kDefaultCommitTimeout;

#if BUILDFLAG(IS_ANDROID)
// Timeout for locking the compositor at the beginning of navigation.
constexpr base::TimeDelta kCompositorLockTimeout = base::Milliseconds(150);
#endif

// crbug.com/954271: This feature is a part of an ablation study which makes
// history navigations slower.
// TODO(altimin): Clean this up after the study finishes.
BASE_FEATURE(kHistoryNavigationDoNotUseCacheAblationStudy,
             "HistoryNavigationDoNotUseCacheAblationStudy",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<double> kDoNotUseCacheProbability{
    &kHistoryNavigationDoNotUseCacheAblationStudy, "probability", 0.0};

const char kSecSharedStorageWritableRequestHeaderKey[] =
    "Sec-Shared-Storage-Writable";

constexpr char kNavigationRequestScope[] = "NavigationRequestScope";

// Flag to control whether redirect URLs are being sanitized before sending
// them to the renderer process as part of the navigation.
// See https://crbug.com/40095391.
BASE_FEATURE(kSanitizeRedirectUrlsDuringNavigation,
             "SanitizeRedirectUrlsDuringNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Denotes the type of user agent string value sent in the User-Agent request
// header.
//
// Corresponds to the "UserAgentStringType" histogram enumeration type in
// tools/metrics/histograms/enums.xml.
//
// PLEASE DO NOT REORDER, REMOVE, OR CHANGE THE MEANING OF THESE VALUES.
enum class UserAgentStringType {
  kFullVersion,
  kReducedVersion,
  kOverriden,
  kMaxValue = kOverriden
};

// Returns the net load flags to use based on the navigation type.
// TODO(clamy): Remove the blink code that sets the caching flags.
void UpdateLoadFlagsWithCacheFlags(int* load_flags,
                                   blink::mojom::NavigationType navigation_type,
                                   bool is_post) {
  switch (navigation_type) {
    case blink::mojom::NavigationType::RELOAD:
      *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE:
      *load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case blink::mojom::NavigationType::RESTORE:
      *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case blink::mojom::NavigationType::RESTORE_WITH_POST:
      *load_flags |=
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case blink::mojom::NavigationType::SAME_DOCUMENT:
    case blink::mojom::NavigationType::DIFFERENT_DOCUMENT:
      if (is_post)
        *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT:
    case blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
      if (is_post) {
        *load_flags |=
            net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      } else if (base::FeatureList::IsEnabled(
                     kHistoryNavigationDoNotUseCacheAblationStudy) &&
                 base::RandDouble() < kDoNotUseCacheProbability.Get()) {
        *load_flags |= net::LOAD_BYPASS_CACHE;
      } else {
        *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      }
      break;
  }
}

// TODO(clamy): This should be function in FrameTreeNode.
bool IsSecureFrame(RenderFrameHostImpl* frame) {
  while (frame) {
    if (!network::IsOriginPotentiallyTrustworthy(
            frame->GetLastCommittedOrigin()))
      return false;
    frame = frame->GetParent();
  }
  return true;
}

// This should match blink::ResourceRequest::needsHTTPOrigin.
bool NeedsHTTPOrigin(net::HttpRequestHeaders* headers,
                     const std::string& method) {
  // Blink version of this function checks if the Origin header might have
  // already been added to |headers|.  This check is not replicated below
  // because:
  // 1. We want to overwrite the old (renderer-provided) header value
  //    with a new, trustworthy (browser-provided) value.
  // 2. The rest of the function matches the Blink version, so there should
  //    be no discrepancies in the Origin value used.

  // Don't send an Origin header for GET or HEAD to avoid privacy issues.
  // For example, if an intranet page has a hyperlink to an external web
  // site, we don't want to include the Origin of the request because it
  // will leak the internal host name. Similar privacy concerns have lead
  // to the widespread suppression of the Referer header at the network
  // layer.
  if (method == "GET" || method == "HEAD")
    return false;

  // For non-GET and non-HEAD methods, always send an Origin header so the
  // server knows we support this feature.
  return true;
}

// Computes the value that should be set for the User-Agent header, if
// `user_agent_override` is non-empty, `user_agent_override` is returned as the
// header value.
std::string ComputeUserAgentValue(const net::HttpRequestHeaders& headers,
                                  const std::string& user_agent_override,
                                  content::BrowserContext* context) {
  if (!user_agent_override.empty()) {
    base::UmaHistogramEnumeration("Navigation.UserAgentStringType",
                                  UserAgentStringType::kOverriden);
    return user_agent_override;
  }

  base::UmaHistogramEnumeration(
      "Navigation.UserAgentStringType",
      base::FeatureList::IsEnabled(
          blink::features::kReduceUserAgentMinorVersion)
          ? UserAgentStringType::kReducedVersion
          : UserAgentStringType::kFullVersion);

  return GetContentClient()->browser()->GetUserAgentBasedOnPolicy(context);
}

void AddAdditionalRequestHeaders(
    net::HttpRequestHeaders* headers,
    const GURL& url,
    blink::mojom::NavigationType navigation_type,
    ui::PageTransition transition,
    BrowserContext* browser_context,
    const std::string& method,
    const std::string& user_agent_override,
    const std::optional<url::Origin>& initiator_origin,
    blink::mojom::Referrer* referrer,
    FrameTreeNode* frame_tree_node) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  bool is_reload = NavigationTypeUtils::IsReload(navigation_type);
  blink::RendererPreferences render_prefs =
      frame_tree_node->current_frame_host()
          ->render_view_host()
          ->GetDelegate()
          ->GetRendererPrefs();
  UpdateAdditionalHeadersForBrowserInitiatedRequest(
      headers, browser_context, is_reload, render_prefs,
      /*is_for_worker_script*=*/false);

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
  headers->SetHeaderIfMissing("Upgrade-Insecure-Requests", "1");

  headers->SetHeaderIfMissing(
      net::HttpRequestHeaders::kUserAgent,
      ComputeUserAgentValue(*headers, user_agent_override, browser_context));

  if (!render_prefs.enable_referrers) {
    *referrer =
        blink::mojom::Referrer(GURL(), network::mojom::ReferrerPolicy::kNever);
  }

  // Next, set the HTTP Origin if needed.
  if (NeedsHTTPOrigin(headers, method)) {
    url::Origin origin_header_value = initiator_origin.value_or(url::Origin());
    origin_header_value = Referrer::SanitizeOriginForRequest(
        url, origin_header_value, referrer->policy);
    headers->SetHeader(net::HttpRequestHeaders::kOrigin,
                       origin_header_value.Serialize());
  }

  if (base::FeatureList::IsEnabled(features::kDocumentPolicyNegotiation)) {
    const blink::DocumentPolicyFeatureState& required_policy =
        frame_tree_node->effective_frame_policy().required_document_policy;
    if (!required_policy.empty()) {
      std::optional<std::string> policy_header =
          blink::DocumentPolicy::Serialize(required_policy);
      DCHECK(policy_header);
      headers->SetHeader("Sec-Required-Document-Policy", policy_header.value());
    }
  }

  // Add the "Sec-Purpose: prefetch;prerender" header to prerender navigations
  // including subframe navigations. Add "Purpose: prefetch" as well for
  // compatibility concerns (See
  // https://github.com/WICG/nav-speculation/issues/133).
  if (frame_tree_node->frame_tree().is_prerendering()) {
    headers->SetHeader("Sec-Purpose", "prefetch;prerender");
    headers->SetHeader("Purpose", "prefetch");
  } else if (frame_tree_node->frame_tree()
                 .page_delegate()
                 ->IsPageInPreviewMode()) {
    // Preview mode sends similar request so that it is compatible with
    // prerendering as we can as possible, but adds `preview` for sites that
    // need to identify the preview case from prerendering.
    // Do not send the `Purpose` header as the preview mode is new and don't
    // need to be careful about the compatibility breakage here.
    headers->SetHeader("Sec-Purpose", "prefetch;prerender;preview");
  }
}

bool ShouldPropagateUserActivation(const url::Origin& previous_origin,
                                   const url::Origin& new_origin) {
  if ((previous_origin.scheme() != "http" &&
       previous_origin.scheme() != "https") ||
      (new_origin.scheme() != "http" && new_origin.scheme() != "https")) {
    return false;
  }

  if (previous_origin.host() == new_origin.host())
    return true;

  std::string previous_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          previous_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string new_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          new_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return !previous_domain.empty() && previous_domain == new_domain;
}

// LOG_NAVIGATION_TIMING_HISTOGRAM logs |value| for "Navigation.<histogram>" UMA
// as well as supplementary UMAs (depending on |transition| and |priority|)
// for BackForward/Reload/NewNavigation variants.
//
// kMaxTime and kBuckets constants are consistent with
// UMA_HISTOGRAM_MEDIUM_TIMES, but a custom kMinTime is used for high fidelity
// near the low end of measured values.
//
// TODO(csharrison,nasko): This macro is incorrect for subframe navigations,
// which will only have subframe-specific transition types. This means that all
// subframes currently are tagged as NewNavigations.
#define LOG_NAVIGATION_TIMING_HISTOGRAM(histogram, transition, priority,      \
                                        duration)                             \
  do {                                                                        \
    const base::TimeDelta kMinTime = base::Milliseconds(1);                   \
    const base::TimeDelta kMaxTime = base::Minutes(3);                        \
    const int kBuckets = 50;                                                  \
    UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram, duration, kMinTime,   \
                               kMaxTime, kBuckets);                           \
    if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {                      \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".BackForward",      \
                                 duration, kMinTime, kMaxTime, kBuckets);     \
    } else if (ui::PageTransitionCoreTypeIs(transition,                       \
                                            ui::PAGE_TRANSITION_RELOAD)) {    \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".Reload", duration, \
                                 kMinTime, kMaxTime, kBuckets);               \
    } else if (ui::PageTransitionIsNewNavigation(transition)) {               \
      UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram ".NewNavigation",    \
                                 duration, kMinTime, kMaxTime, kBuckets);     \
    } else {                                                                  \
      NOTREACHED_IN_MIGRATION() << "Invalid page transition: " << transition; \
    }                                                                         \
    if (priority.has_value()) {                                               \
      if (priority.value() == base::Process::Priority::kBestEffort) {         \
        UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram                    \
                                   ".BackgroundProcessPriority",              \
                                   duration, kMinTime, kMaxTime, kBuckets);   \
      } else {                                                                \
        UMA_HISTOGRAM_CUSTOM_TIMES("Navigation." histogram                    \
                                   ".ForegroundProcessPriority",              \
                                   duration, kMinTime, kMaxTime, kBuckets);   \
      }                                                                       \
    }                                                                         \
  } while (0)

void RecordStartToCommitMetrics(base::TimeTicks navigation_start_time,
                                ui::PageTransition transition,
                                const base::TimeTicks& ready_to_commit_time,
                                std::optional<base::Process::Priority> priority,
                                bool is_same_process,
                                bool is_main_frame) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - navigation_start_time;
  LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit", transition, priority, delta);
  if (is_main_frame) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.MainFrame", transition,
                                    priority, delta);
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.Subframe", transition,
                                    priority, delta);
  }
  if (is_same_process) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess", transition,
                                    priority, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.MainFrame",
                                      transition, priority, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.Subframe",
                                      transition, priority, delta);
    }
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess", transition,
                                    priority, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.MainFrame",
                                      transition, priority, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.Subframe",
                                      transition, priority, delta);
    }
  }
  if (!ready_to_commit_time.is_null()) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("ReadyToCommitUntilCommit2", transition,
                                    priority, now - ready_to_commit_time);
  }
}

void RecordReadyToCommitMetrics(
    RenderFrameHostImpl* old_rfh,
    RenderFrameHostImpl* new_rfh,
    const blink::mojom::CommonNavigationParams& common_params,
    base::TimeTicks ready_to_commit_time,
    NavigationRequest::OriginAgentClusterEndResult
        origin_agent_cluster_end_result,
    bool did_receive_early_hints_before_cross_origin_redirect) {
  bool is_main_frame = !new_rfh->GetParent();
  bool is_same_process =
      old_rfh->GetProcess()->GetID() == new_rfh->GetProcess()->GetID();

  // Navigation.IsSameBrowsingInstance
  if (is_main_frame) {
    bool is_same_browsing_instance =
        old_rfh->GetSiteInstance()->IsRelatedSiteInstance(
            new_rfh->GetSiteInstance());

    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameBrowsingInstance",
                          is_same_browsing_instance);
  }

  // Navigation.IsSameSiteInstance
  {
    bool is_same_site_instance =
        old_rfh->GetSiteInstance() == new_rfh->GetSiteInstance();
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameSiteInstance",
                          is_same_site_instance);
    if (is_main_frame) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameSiteInstance.MainFrame",
                            is_same_site_instance);
    } else {
      UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameSiteInstance.Subframe",
                            is_same_site_instance);
    }
  }

  // Navigation.IsLockedProcess
  {
    ProcessLock process_lock = new_rfh->GetProcess()->GetProcessLock();
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsLockedProcess",
                          process_lock.is_locked_to_site());
    if (common_params.url.SchemeIsHTTPOrHTTPS()) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.IsLockedProcess.HTTPOrHTTPS",
                            process_lock.is_locked_to_site());
    }
  }

  // Navigation.RequiresDedicatedProcess
  {
    UMA_HISTOGRAM_BOOLEAN(
        "Navigation.RequiresDedicatedProcess",
        new_rfh->GetSiteInstance()->RequiresDedicatedProcess());
    if (common_params.url.SchemeIsHTTPOrHTTPS()) {
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.RequiresDedicatedProcess.HTTPOrHTTPS",
          new_rfh->GetSiteInstance()->RequiresDedicatedProcess());
    }
  }

  // TimeToReadyToCommit2
  {
    constexpr std::optional<base::Process::Priority> kPriority = std::nullopt;
    base::TimeDelta delta =
        ready_to_commit_time - common_params.navigation_start;
    ui::PageTransition transition =
        ui::PageTransitionFromInt(common_params.transition);

    LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2", transition,
                                    kPriority, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.MainFrame",
                                      transition, kPriority, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.Subframe",
                                      transition, kPriority, delta);
    }
    if (is_same_process) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.SameProcess",
                                      transition, kPriority, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.CrossProcess",
                                      transition, kPriority, delta);
    }
    if (did_receive_early_hints_before_cross_origin_redirect) {
      LOG_NAVIGATION_TIMING_HISTOGRAM(
          "TimeToReadyToCommit2.CrossOriginRedirectAfterEarlyHints", transition,
          kPriority, delta);
    }
  }

  // Navigation.OriginAgentCluster
  {
    UMA_HISTOGRAM_ENUMERATION("Navigation.OriginAgentCluster.Result",
                              origin_agent_cluster_end_result);
  }

  // Guest (<webview> tag) metrics.
  {
    base::UmaHistogramBoolean("Navigation.IsGuest",
                              new_rfh->GetSiteInstance()->IsGuest());
    if (new_rfh->GetSiteInstance()->IsGuest()) {
      base::UmaHistogramBoolean("Navigation.Guest.IsHTTPOrHTTPS",
                                common_params.url.SchemeIsHTTPOrHTTPS());
      base::UmaHistogramBoolean("Navigation.Guest.IsMainFrame", is_main_frame);
    }
  }
}

// Convert the navigation type to the appropriate cross-document one.
//
// This is currently used when:
// 1) Restarting a same-document navigation as cross-document.
// 2) Failing a navigation and committing an error page.
blink::mojom::NavigationType ConvertToCrossDocumentType(
    blink::mojom::NavigationType type) {
  switch (type) {
    case blink::mojom::NavigationType::SAME_DOCUMENT:
      return blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
    case blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT:
      return blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
    case blink::mojom::NavigationType::RELOAD:
    case blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE:
    case blink::mojom::NavigationType::RESTORE:
    case blink::mojom::NavigationType::RESTORE_WITH_POST:
    case blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
    case blink::mojom::NavigationType::DIFFERENT_DOCUMENT:
      return type;
  }
}

base::debug::CrashKeyString* GetNavigationRequestUrlCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "navigation_request_url", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetNavigationRequestInitiatorCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "navigation_request_initiator", base::debug::CrashKeySize::Size64);
  return crash_key;
}

base::debug::CrashKeyString* GetNavigationRequestIsSameDocumentCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "navigation_request_is_same_document", base::debug::CrashKeySize::Size64);
  return crash_key;
}

// Start a new nested async event with the given name.
void EnterChildTraceEvent(const char* name, NavigationRequest* request) {
  // Passing nullptr as the event name will match the end event with the last
  // unmatched begin event.
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", nullptr,
                                  request->GetNavigationId());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("navigation", name,
                                    request->GetNavigationId());
}

// Start a new nested async event with the given name and args.
template <typename ArgType>
void EnterChildTraceEvent(const char* name,
                          NavigationRequest* request,
                          const char* arg_name,
                          ArgType arg_value) {
  // Passing nullptr as the event name will match the end event with the last
  // unmatched begin event.
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", nullptr,
                                  request->GetNavigationId());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "navigation", name, request->GetNavigationId(), arg_name, arg_value);
}

network::mojom::RequestDestination GetDestinationFromFrameTreeNode(
    FrameTreeNode* frame_tree_node) {
  if (frame_tree_node->IsInFencedFrameTree())
    return network::mojom::RequestDestination::kFencedframe;

  if (frame_tree_node->IsMainFrame()) {
    return network::mojom::RequestDestination::kDocument;
  }

  switch (frame_tree_node->frame_owner_element_type()) {
    case blink::FrameOwnerElementType::kObject:
      return network::mojom::RequestDestination::kObject;
    case blink::FrameOwnerElementType::kEmbed:
      return network::mojom::RequestDestination::kEmbed;
    case blink::FrameOwnerElementType::kIframe:
      return network::mojom::RequestDestination::kIframe;
    case blink::FrameOwnerElementType::kFrame:
      return network::mojom::RequestDestination::kFrame;
    // Main frames are handled above.
    case blink::FrameOwnerElementType::kNone:
      NOTREACHED_IN_MIGRATION();
      return network::mojom::RequestDestination::kDocument;
      // Fenced frames are handled above.
    case blink::FrameOwnerElementType::kFencedframe:
      NOTREACHED_IN_MIGRATION();
      return network::mojom::RequestDestination::kFencedframe;
  }
}

// Returns true if the parent's COEP policy `parent_coep` should block a child
// embedded in an <iframe> loaded with `child_coep` policy. The
// `is_credentialless` parameter reflects whether the child will be loaded as a
// credentialless document.
bool CoepBlockIframe(network::mojom::CrossOriginEmbedderPolicyValue parent_coep,
                     network::mojom::CrossOriginEmbedderPolicyValue child_coep,
                     bool is_credentialless) {
  return !is_credentialless &&
         (network::CompatibleWithCrossOriginIsolated(parent_coep) &&
          !network::CompatibleWithCrossOriginIsolated(child_coep));
}

// Computes the history offset of the new document compared to the current one.
int EstimateHistoryOffset(NavigationController& controller,
                          bool should_replace_current_entry) {
  if (should_replace_current_entry)
    return 0;

  int current_index = controller.GetLastCommittedEntryIndex();
  int pending_index = controller.GetPendingEntryIndex();

  // +1 for non history navigation.
  if (current_index == -1 || pending_index == -1)
    return 1;

  return pending_index - current_index;
}

bool IsDocumentToCommitAnonymous(FrameTreeNode* frame,
                                 bool is_synchronous_about_blank_navigation) {
  // FencedFrame do not propagate the credentialless bit deeper.
  // In particular, it means their future response will have to adhere to COEP.
  if (frame->IsFencedFrameRoot())
    return false;

  RenderFrameHostImpl* current_document = frame->current_frame_host();
  RenderFrameHostImpl* parent_document = frame->parent();

  // The synchronous about:blank navigation preserves the state of the initial
  // empty document.
  // TODO(https://github.com/whatwg/html/issues/6863): Remove the synchronous
  // about:blank navigation.
  if (is_synchronous_about_blank_navigation)
    return current_document->IsCredentialless();

  // The document to commit will be credentialless if either the iframe element
  // has the 'credentialless' attribute set or the parent document is
  // credentialless.
  bool parent_is_credentialless =
      parent_document && parent_document->IsCredentialless();
  return parent_is_credentialless || frame->Credentialless();
}

// Returns the "loading" URL in the renderer. This tries to replicate
// RenderFrameImpl::GetLoadingUrl(). This might return a different URL from
// what we get when calling GetLastCommittedURL() on `rfh`, in case the
// document had changed its URL through document.open() before, or
// when calling last_document_url_in_renderer(), in case of error pages and
// loadDataWithBaseURL documents.
// This function should only be used to preserve calculations that were
// previously done in the renderer but got moved to the browser (e.g. URL
// comparisons to determine if a navigation should do a replacement or not).
const GURL& GetLastLoadingURLInRendererForNavigationReplacement(
    RenderFrameHostImpl* rfh) {
  // Handle some special cases:
  // - The "loading URL" for an error page commit is the URL that it failed to
  // load. This will be retained as long as the document stays the same.
  // - For loadDataWithBaseURL() navigations the "loading URL" will be the
  // last committed URL (the data: URL). This will also be retained as long as
  // the document stays the same.
  if (rfh->IsErrorDocument() ||
      rfh->was_loaded_from_load_data_with_base_url()) {
    return rfh->GetLastCommittedURL();
  }

  // Otherwise, return the last document URL.
  return rfh->last_document_url_in_renderer();
}

bool IsOptedInFencedFrame(const net::HttpResponseHeaders& http_headers) {
  network::mojom::SupportsLoadingModePtr result =
      network::ParseSupportsLoadingMode(http_headers);
  return !result.is_null() &&
         base::Contains(result->supported_modes,
                        network::mojom::LoadingMode::kFencedFrame);
}

// If there are any "Origin-Trial" headers on the |response|, persist those
// that correspond to persistent origin trials, provided the tokens are valid.
void PersistOriginTrialsFromHeaders(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const network::mojom::URLResponseHead* response,
    BrowserContext* browser_context,
    ukm::SourceId source_id) {
  if (!base::FeatureList::IsEnabled(features::kPersistentOriginTrials))
    return;

  // It is not possible to serialize opaque origins, so we cannot save any
  // information for them.
  if (origin.opaque())
    return;

  // Skip About:blank, about:srcdoc and a few other URLs, because they can't
  // have any Origin-Trial header.
  if (!response || !response->headers)
    return;

  OriginTrialsControllerDelegate* origin_trials_delegate =
      browser_context->GetOriginTrialsControllerDelegate();
  if (!origin_trials_delegate)
    return;

  std::vector<std::string> tokens =
      GetOriginTrialHeaderValues(response->headers.get());
  origin_trials_delegate->PersistTrialsFromTokens(
      origin, partition_origin, tokens, base::Time::Now(), source_id);
}

struct TopicsHeaderValueResult {
  bool topics_eligible = false;
  std::optional<std::string> header_value;
};

// Returns the topics header for a navigation request. Returns std::nullopt if
// the request isn't eligible for topics. This should align with the handling in
// `GetTopicsHeaderValueForSubresourceRequest()`.
TopicsHeaderValueResult GetTopicsHeaderValueForNavigationRequest(
    FrameTreeNode* frame_tree_node,
    const GURL& url) {
  // Skip if the <iframe> does not have the "browsingtopics" opt-in attribute.
  if (!frame_tree_node->browsing_topics()) {
    return TopicsHeaderValueResult{};
  }

  RenderFrameHostImpl* rfh = frame_tree_node->current_frame_host();

  // Skip top frame navigation.
  // TODO(crbug.com/40260337): This should be checked at the mojom boundary of
  // RenderFrameHostImpl::DidChangeIframeAttributes, and should be a DCHECK
  // here.
  if (rfh->is_main_frame()) {
    return {};
  }

  // Skip fenced frames.
  if (rfh->IsNestedWithinFencedFrame()) {
    return {};
  }

  // Skip inactive pages (e.g. prerendered pages).
  if (!rfh->GetPage().IsPrimary()) {
    return {};
  }

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque()) {
    return {};
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    return {};
  }

  const blink::PermissionsPolicy* parent_policy =
      rfh->GetParent()->permissions_policy();

  DCHECK(parent_policy);

  if (!parent_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kBrowsingTopics, origin) ||
      !parent_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::
              kBrowsingTopicsBackwardCompatible,
          origin)) {
    return {};
  }

  std::vector<blink::mojom::EpochTopicPtr> topics;
  bool topics_eligible = GetContentClient()->browser()->HandleTopicsWebApi(
      origin, rfh->GetMainFrame(),
      browsing_topics::ApiCallerSource::kIframeAttribute,
      /*get_topics=*/true,
      /*observe=*/false, topics);

  int num_versions_in_epochs =
      topics_eligible
          ? GetContentClient()->browser()->NumVersionsInTopicsEpochs(
                rfh->GetMainFrame())
          : 0;

  return {
      .topics_eligible = topics_eligible,
      .header_value = DeriveTopicsHeaderValue(topics, num_versions_in_epochs)};
}

ukm::SourceId GetPageUkmSourceId(FrameTreeNode* frame_tree_node) {
  CHECK(frame_tree_node);
  RenderFrameHost* render_frame_host = frame_tree_node->current_frame_host();
  CHECK(render_frame_host);
  // Our data collection policy disallows collecting UKMs while prerendering.
  // So, return kInvalidSourceId when the page is in the prerendering state.
  // See //content/browser/preloading/prerender/README.md and ask the team to
  // explore options to record data for prerendering pages.
  if (render_frame_host->IsInLifecycleState(
          RenderFrameHost::LifecycleState::kPrerendering)) {
    return ukm::kInvalidSourceId;
  }
  return render_frame_host->GetPageUkmSourceId();
}

bool IsMhtmlMimeType(const std::string& mime_type) {
  return mime_type == "multipart/related" || mime_type == "message/rfc822";
}

network::mojom::WebSandboxFlags GetSandboxFlagsInitiator(
    const std::optional<blink::LocalFrameToken>& frame_token,
    int initiator_process_id,
    StoragePartitionImpl* storage_partition) {
  if (!frame_token) {
    return network::mojom::WebSandboxFlags::kNone;
  }

  // Even if the navigation was initiated from an unload handler and the
  // RenderFrameHost is gone, its associated PolicyContainerHost should be
  // available by design.
  //
  // Note: See https://crbug.com/1473165. The "design" is currently not 100%
  // achieved. The PolicyContainer might be missing when the navigation is
  // initiated from RenderViewContextMenu::ExecuteCommand(...).
  const PolicyContainerHost* policy_container_host =
      RenderFrameHostImpl::GetPolicyContainerHost(
          base::OptionalToPtr(frame_token), initiator_process_id,
          storage_partition);
  if (!policy_container_host) {
    return network::mojom::WebSandboxFlags::kNone;
  }

  return policy_container_host->policies().sandbox_flags;
}

bool IsSharedStorageWritableEligibleForNavigationRequest(
    FrameTreeNode* frame_tree_node,
    const GURL& url) {
  // False if the <iframe> does not have the "sharedstoragewritable" opt-in
  // attribute.
  if (!frame_tree_node->shared_storage_writable_opted_in()) {
    return false;
  }

  // Only child frames should have the `sharedstoragewritable` attribute set to
  // true.
  CHECK(!frame_tree_node->IsMainFrame());

  // Apart from fenced frames' frame trees, skip non-primary pages (e.g.
  // prerendered pages).
  if (frame_tree_node->fenced_frame_status() !=
          RenderFrameHostImpl::FencedFrameStatus::
              kIframeNestedWithinFencedFrame &&
      (!frame_tree_node->frame_tree().is_primary() ||
       !frame_tree_node->frame_tree().root()->IsOutermostMainFrame())) {
    return false;
  }

  url::Origin origin = url::Origin::Create(url);
  if (origin.opaque()) {
    return false;
  }

  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    return false;
  }

  CHECK(frame_tree_node->parent());
  const blink::PermissionsPolicy* parent_policy =
      frame_tree_node->parent()->permissions_policy();

  DCHECK(parent_policy);
  return parent_policy->IsFeatureEnabledForOrigin(
      blink::mojom::PermissionsPolicyFeature::kSharedStorage, origin);
}

std::optional<base::SafeRef<RenderFrameHostImpl>>
GetRenderFrameHostForBackForwardCacheRestore(FrameTreeNode* frame_tree_node,
                                             NavigationEntryImpl* entry) {
  if (!entry) {
    return std::nullopt;
  }

  auto restored_entry = frame_tree_node->navigator()
                            .controller()
                            .GetBackForwardCache()
                            .GetOrEvictEntry(entry->GetUniqueID());
  if (!restored_entry.has_value()) {
    // If there is no active BFCache entry, we can't use the RFH from the
    // BFCache entry for the history navigation.
    return std::nullopt;
  }

  RenderFrameHostImpl* restored_rfh =
      restored_entry.value()->render_frame_host();

  // If there is an ongoing BFCache NavigationRequest with the same entry, that
  // NavigationRequest will be cancelled, and trigger an eviction from the
  // NavigationRequest destructor (see comment there for details). So, we can't
  // restore the to-be-evicted entry anymore.
  NavigationRequest* previous_navigation_request =
      frame_tree_node->navigation_request();
  if (previous_navigation_request &&
      previous_navigation_request->IsServedFromBackForwardCache() &&
      previous_navigation_request
              ->GetRenderFrameHostRestoredFromBackForwardCache() ==
          restored_rfh) {
    // Since the BFCache entry won't be restored, we evict it here with
    // `kNavigationCancelledWhileRestoring` so that the NavigationRequest
    // won't end up with not restored with no reason (or `Unknown` will be
    // added instead).
    // TODO(crbug.com/40283427): Only evict BFCache if the
    // `BackForwardCacheCommitDeferringCondition`, which unfreezes the
    // page and disables the eviction on the renderer side, is completed.
    restored_rfh->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::
            kNavigationCancelledWhileRestoring);
    return std::nullopt;
  }

  if (!frame_tree_node->IsMainFrame()) {
    // We have a matching BFCache entry for a subframe navigation. This
    // shouldn't happen as we should've triggered deletion of BFCache
    // entries that have the same BrowsingInstance as the current document.
    // See https://crbug.com/1250111.
    CaptureTraceForNavigationDebugScenario(
        DebugScenario::kDebugBackForwardCacheEntryExistsOnSubframeHistoryNav);
    return std::nullopt;
  }

  return restored_rfh->GetSafeRef();
}

void MaybePrewarmHttpDiskCache(
    BrowserContext& browser_context,
    const GURL& url,
    const std::optional<url::Origin>& initiator_origin) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kHttpDiskCachePrewarming) ||
      !blink::features::kHttpDiskCachePrewarmingTriggerOnNavigation.Get()) {
    return;
  }

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  GetContentClient()->browser()->MaybePrewarmHttpDiskCache(
      browser_context, initiator_origin, url);
}

// Returns true in cases where an attempted download will end up replacing the
// current document anyway, due to showing an error page.
bool IsFailedDownload(bool is_download,
                      const net::HttpResponseHeaders* headers) {
  return is_download && headers &&
         !network::IsSuccessfulStatus(headers->response_code());
}

// Returns if the given `rfh` should be evicted from BackForwardCache due to
// ongoing navigation.
bool MaybeEvictFromBackForwardCacheBySubframeNavigation(
    RenderFrameHostImpl* rfh) {
  if (base::FeatureList::IsEnabled(
          features::kEnableBackForwardCacheForOngoingSubframeNavigation) &&
      rfh->GetParentOrOuterDocument() &&
      rfh->GetLifecycleState() ==
          RenderFrameHost::LifecycleState::kInBackForwardCache) {
    // Normally, ongoing subframe navigations will be deferred by
    // `BackForwardCacheSubframeNavigationThrottle` before they reach this
    // point, if the page the subframe is on gets BFCached.
    //
    // However, it's possible for subframe navigations to end up here while its
    // page is BFCached, if at the time the navigation went through
    // `BackForwardCacheSubframeNavigationThrottle::WillStartRequest()` or
    // BackForwardCacheSubframeNavigationThrottle::WillCommitWithoutUrlLoader(),
    // the page is not BFCached yet, but then the page gets BFCached in between
    // that time and when this function is called.
    //
    // Outside of tests, this should not be possible, as
    // `BackForwardCacheSubframeNavigationThrottle` are the last throttles to be
    // registered/run. However, in tests, the last throttles to run are
    // test-only throttles, which can introduce an asynchronous step, making it
    // possible for the page to enter BFCache during that time. In that case, we
    // shouldn't continue processing the navigation in the subframe and need to
    // evict the page from BFCache.
    rfh->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating);

    // DO NOT ADD CODE after this. The previous call has destroyed the
    // NavigationRequest.
    return true;
  }
  return false;
}

net::StorageAccessApiStatus ShouldLoadWithStorageAccess(
    const blink::mojom::BeginNavigationParams& begin_params,
    const blink::mojom::CommonNavigationParams& common_params,
    const RenderFrameHostImpl* previous_document_rfh,
    bool did_encounter_cross_origin_redirect,
    const GURL response_url,
    const network::mojom::URLResponseHead* response) {
  // Experimental: Storage Access API Headers
  // (https://github.com/cfredric/storage-access-headers)
  //
  // A server can opt-in to provide storage access to a document by setting the
  // `Activate-Storage-Access: load` header, provided that the user has already
  // granted the relevant `storage-access` permission.
  //
  // Note: As of today, `about:blank`, `about:srcdoc`, and MHTML-iframe do not
  // have a response.
  if (response && response->load_with_storage_access) {
    // TODO(https://crbug.com/344608182): this ought to use a dedicated status,
    // since the JS API was *not* used to get storage access here.
    return net::StorageAccessApiStatus::kAccessViaAPI;
  }

  // Storage Access API: https://privacycg.github.io/storage-access/#navigation
  //
  // If a document has storage access, and initiates a navigation in the same
  // frame toward a document from the same origin, the `has storage access` bit
  // is inherited.
  //
  // This doesn't hold if there is a cross-origin redirect in between.
  //
  // Note: `begin_params` and `common_params` are not trusted, so we have to
  // check the frame token.
  switch (begin_params.storage_access_api_status) {
    case net::StorageAccessApiStatus::kNone:
      return net::StorageAccessApiStatus::kNone;
    case net::StorageAccessApiStatus::kAccessViaAPI:
      return common_params.initiator_origin &&
                     common_params.initiator_origin->IsSameOriginWith(
                         response_url) &&
                     begin_params.initiator_frame_token &&
                     begin_params.initiator_frame_token ==
                         previous_document_rfh->GetFrameToken() &&
                     !did_encounter_cross_origin_redirect
                 ? begin_params.storage_access_api_status
                 : net::StorageAccessApiStatus::kNone;
  }
}

// The sampling rate for UKM.
constexpr double kUkmSamplingRate = 0.001;

}  // namespace

NavigationRequest::PrerenderActivationNavigationState::
    PrerenderActivationNavigationState() = default;
NavigationRequest::PrerenderActivationNavigationState::
    ~PrerenderActivationNavigationState() = default;

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateBrowserInitiated(
    FrameTreeNode* frame_tree_node,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool was_opener_suppressed,
    const std::string& extra_headers,
    FrameNavigationEntry* frame_entry,
    NavigationEntryImpl* entry,
    bool is_form_submission,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    const std::optional<blink::Impression>& impression,
    bool is_pdf,
    bool is_embedder_initiated_fenced_frame_navigation,
    std::optional<std::u16string> embedder_shared_storage_context) {
  return Create(
      frame_tree_node, std::move(common_params), std::move(commit_params),
      /*browser_initiated=*/true, was_opener_suppressed,
      std::nullopt /* initiator_frame_token */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      extra_headers, frame_entry, entry, is_form_submission,
      std::move(navigation_ui_data), impression,
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation,
      is_pdf, is_embedder_initiated_fenced_frame_navigation,
      /*is_container_initiated=*/false, /*has_rel_opener=*/false,
      net::StorageAccessApiStatus::kNone, embedder_shared_storage_context);
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::Create(
    FrameTreeNode* frame_tree_node,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool browser_initiated,
    bool was_opener_suppressed,
    const std::optional<blink::LocalFrameToken>& initiator_frame_token,
    int initiator_process_id,
    const std::string& extra_headers,
    FrameNavigationEntry* frame_entry,
    NavigationEntryImpl* entry,
    bool is_form_submission,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    const std::optional<blink::Impression>& impression,
    blink::mojom::NavigationInitiatorActivationAndAdStatus
        initiator_activation_and_ad_status,
    bool is_pdf,
    bool is_embedder_initiated_fenced_frame_navigation,
    bool is_container_initiated,
    bool has_rel_opener,
    net::StorageAccessApiStatus storage_access_api_status,
    std::optional<std::u16string> embedder_shared_storage_context) {
  TRACE_EVENT1("navigation", "NavigationRequest::Create", "browser_initiated",
               browser_initiated);

  common_params->request_destination =
      GetDestinationFromFrameTreeNode(frame_tree_node);

  auto navigation_params = blink::mojom::BeginNavigationParams::New(
      initiator_frame_token, extra_headers, net::LOAD_NORMAL,
      false /* skip_service_worker */,
      blink::mojom::RequestContextType::LOCATION,
      blink::mojom::MixedContentContextType::kBlockable, is_form_submission,
      false /* was_initiated_by_link_click */,
      blink::mojom::ForceHistoryPush::kNo, GURL() /* searchable_form_url */,
      std::string() /* searchable_form_encoding */,
      GURL() /* client_side_redirect_url */,
      std::nullopt /* devtools_initiator_info */,
      nullptr /* trust_token_params */, impression,
      base::TimeTicks() /* renderer_before_unload_start */,
      base::TimeTicks() /* renderer_before_unload_end */,
      initiator_activation_and_ad_status, is_container_initiated,
      storage_access_api_status, has_rel_opener);

  // Shift-Reload forces bypassing caches and service workers.
  if (common_params->navigation_type ==
      blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE) {
    navigation_params->load_flags |= net::LOAD_BYPASS_CACHE;
    navigation_params->skip_service_worker = true;
  }

  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
  if (frame_entry) {
    blob_url_loader_factory = frame_entry->blob_url_loader_factory();

    if (common_params->url.SchemeIsBlob() && !blob_url_loader_factory) {
      // If this navigation entry came from session history then the blob
      // factory would have been cleared in
      // NavigationEntryImpl::ResetForCommit(). This is avoid keeping large
      // blobs alive unnecessarily and the spec is unclear. So create a new blob
      // factory which will work if the blob happens to still be alive,
      // resolving the blob URL in the site instance it was loaded in.
      blob_url_loader_factory =
          ChromeBlobStorageContext::URLLoaderFactoryForUrl(
              frame_tree_node->navigator()
                  .controller()
                  .GetBrowserContext()
                  ->GetStoragePartition(frame_entry->site_instance()),
              common_params->url);
    }
  }

  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, std::move(common_params), std::move(navigation_params),
      std::move(commit_params), browser_initiated,
      false /* from_begin_navigation */,
      false /* is_synchronous_renderer_commit */, frame_entry, entry,
      std::move(navigation_ui_data), std::move(blob_url_loader_factory),
      mojo::NullAssociatedRemote(),
      nullptr /* prefetched_signed_exchange_cache */,
      GetRenderFrameHostForBackForwardCacheRestore(frame_tree_node, entry),
      initiator_process_id, was_opener_suppressed, is_pdf,
      is_embedder_initiated_fenced_frame_navigation,
      mojo::NullReceiver() /* renderer_cancellation_listener */,
      embedder_shared_storage_context));

  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateRendererInitiated(
    FrameTreeNode* frame_tree_node,
    NavigationEntryImpl* entry,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::BeginNavigationParamsPtr begin_params,
    int current_history_list_offset,
    int current_history_list_length,
    bool override_user_agent,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
        renderer_cancellation_listener) {
  TRACE_EVENT0("navigation", "NavigationRequest::CreateRendererInitiated");
  // Only normal navigations to a different document or reloads are expected.
  // - Renderer-initiated same document navigations never start in the browser.
  // - Restore-navigations are always browser-initiated.
  // - History-navigations use the browser-initiated path, even the ones that
  //   are initiated by a javascript script.
  DCHECK(NavigationTypeUtils::IsReload(common_params->navigation_type) ||
         common_params->navigation_type ==
             blink::mojom::NavigationType::DIFFERENT_DOCUMENT);

  common_params->request_destination =
      GetDestinationFromFrameTreeNode(frame_tree_node);

  // TODO(clamy): See if the navigation start time should be measured in the
  // renderer and sent to the browser instead of being measured here.
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::mojom::CommitNavigationParams::New(
          std::nullopt,
          // The correct storage key will be computed before committing the
          // navigation.
          blink::StorageKey(), override_user_agent,
          /*redirects=*/std::vector<GURL>(),
          /*redirect_response=*/
          std::vector<network::mojom::URLResponseHeadPtr>(),
          /*redirect_infos=*/std::vector<net::RedirectInfo>(),
          /*post_content_type=*/std::string(), common_params->url,
          common_params->method,
          /*can_load_local_resources=*/false,
          /*page_state=*/std::string(),
          /*nav_entry_id=*/0,
          /*subframe_unique_names=*/base::flat_map<std::string, bool>(),
          /*intended_as_new_entry=*/false,
          // Set to -1 because history-navigations do not use this path. See
          // comments above.
          /*pending_history_list_offset=*/-1, current_history_list_offset,
          current_history_list_length,
          /*was_discarded=*/false,
          /*is_view_source=*/false,
          /*should_clear_history_list=*/false,
          /*navigation_timing=*/blink::mojom::NavigationTiming::New(),
          blink::mojom::WasActivatedOption::kUnknown,
          /*navigation_token=*/base::UnguessableToken::Create(),
          /*prefetched_signed_exchanges=*/
          std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>(),
#if BUILDFLAG(IS_ANDROID)
          /*data_url_as_string=*/std::string(),
#endif
          /*is_browser_initiated=*/false,
          /*has_ua_visual_transition*/ false,
          /*document_ukm_source_id=*/ukm::kInvalidSourceId,
          frame_tree_node->pending_frame_policy(),
          /*force_enabled_origin_trials=*/std::vector<std::string>(),
          /*origin_agent_cluster=*/false,
          /*origin_agent_cluster_left_as_default=*/true,
          /*enabled_client_hints=*/
          std::vector<network::mojom::WebClientHintsType>(),
          /*is_cross_site_cross_browsing_context_group=*/false,
          /*should_have_sticky_user_activation=*/false,
          /*old_page_info=*/nullptr, /*http_response_code=*/-1,
          blink::mojom::NavigationApiHistoryEntryArrays::New(),
          /*early_hints_preloaded_resources=*/std::vector<GURL>(),
          // This timestamp will be populated when the commit IPC is sent.
          /*commit_sent=*/base::TimeTicks(), /*srcdoc_value=*/std::string(),
          /*should_load_data_url=*/false,
          /*ancestor_or_self_has_cspee=*/
          frame_tree_node->AncestorOrSelfHasCSPEE(),
          /*reduced_accept_language=*/std::string(),
          /*navigation_delivery_type=*/
          network::mojom::NavigationDeliveryType::kDefault,
          /*view_transition_state=*/std::nullopt,
          /*soft_navigation_heuristics_task_id=*/std::nullopt,
          /*modified_runtime_features=*/
          base::flat_map<::blink::mojom::RuntimeFeature, bool>(),
          /*fenced_frame_properties=*/std::nullopt,
          /*not_restored_reasons=*/nullptr,
          /*load_with_storage_access=*/
          net::StorageAccessApiStatus::kNone,
          /*browsing_context_group_info=*/std::nullopt,
          /*lcpp_hint=*/nullptr, blink::CreateDefaultRendererContentSettings(),
          /*cookie_deprecation_label=*/std::nullopt,
          /*visited_link_salt=*/std::nullopt,
          /*local_surface_id=*/std::nullopt);

  commit_params->navigation_timing->system_entropy_at_navigation_start =
      SystemEntropyUtils::ComputeSystemEntropyForFrameTreeNode(
          frame_tree_node, blink::mojom::SystemEntropy::kNormal);

  // CreateRendererInitiated() should only be triggered when the navigation is
  // initiated by a frame in the same process.
  // TODO(crbug.com/40686861): Find a way to DCHECK that the routing ID
  // is from the current RFH.
  int initiator_process_id =
      frame_tree_node->current_frame_host()->GetProcess()->GetID();

  // `was_opener_suppressed` can be true for renderer initiated navigations, but
  // only in cases which get routed through `CreateBrowserInitiated()` instead.
  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, std::move(common_params), std::move(begin_params),
      std::move(commit_params),
      false,    // browser_initiated
      true,     // from_begin_navigation
      false,    // is_synchronous_renderer_commit
      nullptr,  // frame_entry
      entry,
      nullptr,  // navigation_ui_data
      std::move(blob_url_loader_factory), std::move(navigation_client),
      std::move(prefetched_signed_exchange_cache),
      std::nullopt,  // rfh_restored_from_back_forward_cache
      initiator_process_id,
      /*was_opener_suppressed=*/false, /*is_pdf=*/false,
      /*is_embedder_initiated_fenced_frame_navigation=*/false,
      std::move(renderer_cancellation_listener)));

  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest>
NavigationRequest::CreateForSynchronousRendererCommit(
    FrameTreeNode* frame_tree_node,
    RenderFrameHostImpl* render_frame_host,
    bool is_same_document,
    const GURL& url,
    const url::Origin& origin,
    const std::optional<GURL>& initiator_base_url,
    const net::IsolationInfo& isolation_info_for_subresources,
    blink::mojom::ReferrerPtr referrer,
    const ui::PageTransition& transition,
    bool should_replace_current_entry,
    const std::string& method,
    bool has_transient_activation,
    bool is_overriding_user_agent,
    const std::vector<GURL>& redirects,
    const GURL& original_url,
    std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter,
    int http_response_code) {
  TRACE_EVENT0("navigation", "NavigationRequest::CreateForSynchronousRendererCommit");
  // TODO(clamy): Improve the *NavigationParams and *CommitParams to avoid
  // copying so many parameters here.
  blink::mojom::CommonNavigationParamsPtr common_params =
      blink::mojom::CommonNavigationParams::New(
          url,
          // TODO(nasko): Investigate better value to pass for
          // |initiator_origin|.
          origin, initiator_base_url, std::move(referrer), transition,
          is_same_document ? blink::mojom::NavigationType::SAME_DOCUMENT
                           : blink::mojom::NavigationType::DIFFERENT_DOCUMENT,
          blink::NavigationDownloadPolicy(), should_replace_current_entry,
          GURL() /* base_url_for_data_url*/, base::TimeTicks::Now(),
          method /* method */, nullptr /* post_data */,
          network::mojom::SourceLocation::New(),
          false /* started_from_context_menu */, has_transient_activation,
          false /* has_text_fragment_token */,
          network::mojom::CSPDisposition::CHECK,
          std::vector<int>() /* initiator_origin_trial_features */,
          std::string() /* href_translate */,
          false /* is_history_navigation_in_new_child_frame */,
          base::TimeTicks::Now() /* input_start */,
          network::mojom::RequestDestination::kEmpty);
  // Note that some params are set to default values (e.g. page_state set to
  // the default blink::PageState()) even if the DidCommit message that came
  // from the renderer contained relevant info that can be used to fill the
  // params, because setting those values don't match with the pattern used
  // by navigations that went through the browser (e.g. page_state is only
  // set in CommitNavigationParams of history navigations) or these values are
  // not used by the browser after commit.
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::mojom::CommitNavigationParams::New(
          std::nullopt,
          // The correct storage key is computed right after creating the
          // NavigationRequest below.
          blink::StorageKey(), is_overriding_user_agent, redirects,
          /*redirect_response=*/
          std::vector<network::mojom::URLResponseHeadPtr>(),
          /*redirect_infos=*/std::vector<net::RedirectInfo>(),
          /*post_content_type=*/std::string(), original_url,
          /*original_method=*/method,
          /*can_load_local_resources=*/false,
          /*page_state=*/std::string(),
          /*nav_entry_id=*/0,
          /*subframe_unique_names=*/base::flat_map<std::string, bool>(),
          /*intended_as_new_entry=*/false,
          /*pending_history_list_offset=*/-1,
          /*current_history_list_offset=*/-1,
          /*current_history_list_length=*/-1,
          /*was_discarded=*/false,
          /*is_view_source=*/false,
          /*should_clear_history_list=*/false,
          /*navigation_timing=*/blink::mojom::NavigationTiming::New(),
          blink::mojom::WasActivatedOption::kUnknown,
          /*navigation_token=*/base::UnguessableToken::Create(),
          /*prefetched_signed_exchanges=*/
          std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>(),
#if BUILDFLAG(IS_ANDROID)
          /*data_url_as_string=*/std::string(),
#endif
          /*is_browser_initiated=*/false,
          /*has_ua_visual_transition*/ false,
          /*document_ukm_source_id=*/ukm::kInvalidSourceId,
          frame_tree_node->pending_frame_policy(),
          /*force_enabled_origin_trials=*/std::vector<std::string>(),
          /*origin_agent_cluster=*/false,
          /*origin_agent_cluster_left_as_default=*/true,
          /*enabled_client_hints=*/
          std::vector<network::mojom::WebClientHintsType>(),
          /*is_cross_site_cross_browsing_context_group=*/false,
          /*should_have_sticky_user_activation=*/false,
          /*old_page_info=*/nullptr, http_response_code,
          blink::mojom::NavigationApiHistoryEntryArrays::New(),
          /*early_hints_preloaded_resources=*/std::vector<GURL>(),
          // This timestamp will be populated when the commit IPC is sent.
          /*commit_sent=*/base::TimeTicks(), /*srcdoc_value=*/std::string(),
          /*should_load_data_url=*/false,
          /*ancestor_or_self_has_cspee=*/
          frame_tree_node->AncestorOrSelfHasCSPEE(),
          /*reduced_accept_language=*/std::string(),
          /*navigation_delivery_type=*/
          network::mojom::NavigationDeliveryType::kDefault,
          /*view_transition_state=*/std::nullopt,
          /*soft_navigation_heuristics_task_id=*/std::nullopt,
          /*modified_runtime_features=*/
          base::flat_map<::blink::mojom::RuntimeFeature, bool>(),
          /*fenced_frame_properties=*/std::nullopt,
          /*not_restored_reasons=*/nullptr,
          /*load_with_storage_access=*/
          net::StorageAccessApiStatus::kNone,
          /*browsing_context_group_info=*/std::nullopt,
          /*lcpp_hint=*/nullptr, blink::CreateDefaultRendererContentSettings(),
          /*cookie_deprecation_label=*/std::nullopt,
          /*visited_link_salt=*/std::nullopt,
          /*local_surface_id=*/std::nullopt);
  blink::mojom::BeginNavigationParamsPtr begin_params =
      blink::mojom::BeginNavigationParams::New();
  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, std::move(common_params), std::move(begin_params),
      std::move(commit_params), false /* browser_initiated */,
      false /* from_begin_navigation */,
      true /* is_synchronous_renderer_commit */,
      nullptr /* frame_navigation_entry */, nullptr /* navigation_entry */,
      nullptr /* navigation_ui_data */, nullptr /* blob_url_loader_factory */,
      mojo::NullAssociatedRemote(),
      nullptr /* prefetched_signed_exchange_cache */,
      std::nullopt /* rfh_restored_from_back_forward_cache */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      false /* was_opener_suppressed */, false /* is_pdf */));

  std::optional<base::UnguessableToken> nonce = render_frame_host->ComputeNonce(
      navigation_request->is_credentialless(),
      navigation_request->ComputeFencedFrameNonce());
  url::Origin top_level_origin =
      render_frame_host->ComputeTopFrameOrigin(origin);
  if (nonce) {
    // If the nonce isn't null, we can use the simpler form of the constructor.
    navigation_request->commit_params_->storage_key =
        blink::StorageKey::CreateWithNonce(origin, *nonce);
  } else {
    // Otherwise we need to derive the top_level_site and ancestor_chain_bit.
    net::SchemefulSite top_level_site(top_level_origin);

    blink::mojom::AncestorChainBit ancestor_chain_bit =
        blink::mojom::AncestorChainBit::kSameSite;
    if (render_frame_host->ComputeSiteForCookies().IsNull() ||
        net::SchemefulSite(origin) != top_level_site ||
        !top_level_site.opaque() || origin.opaque()) {
      ancestor_chain_bit = blink::mojom::AncestorChainBit::kCrossSite;
    }

    navigation_request->commit_params_->storage_key =
        blink::StorageKey::Create(origin, top_level_site, ancestor_chain_bit);
  }
  navigation_request->commit_params_->navigation_timing
      ->system_entropy_at_navigation_start =
      SystemEntropyUtils::ComputeSystemEntropyForFrameTreeNode(
          frame_tree_node, blink::mojom::SystemEntropy::kNormal);
  navigation_request->render_frame_host_ = render_frame_host->GetSafeRef();
  navigation_request->coep_reporter_ = std::move(coep_reporter);
  navigation_request->isolation_info_for_subresources_ =
      isolation_info_for_subresources;
  navigation_request->associated_rfh_type_ =
      AssociatedRenderFrameHostType::CURRENT;
  navigation_request->StartNavigation();
  DCHECK(navigation_request->IsNavigationStarted());

  return navigation_request;
}

// static class variable used to generate unique navigation ids for
// NavigationRequest.
int64_t NavigationRequest::unique_id_counter_ = 0;

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::BeginNavigationParamsPtr begin_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool browser_initiated,
    bool from_begin_navigation,
    bool is_synchronous_renderer_commit,
    const FrameNavigationEntry* frame_entry,
    NavigationEntryImpl* entry,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    std::optional<base::SafeRef<RenderFrameHostImpl>>
        rfh_restored_from_back_forward_cache,
    int initiator_process_id,
    bool was_opener_suppressed,
    bool is_pdf,
    bool is_embedder_initiated_fenced_frame_navigation,
    mojo::PendingReceiver<mojom::NavigationRendererCancellationListener>
        renderer_cancellation_listener,
    std::optional<std::u16string> embedder_shared_storage_context)
    : frame_tree_node_(frame_tree_node),
      is_synchronous_renderer_commit_(is_synchronous_renderer_commit),
      common_params_(std::move(common_params)),
      begin_params_(std::move(begin_params)),
      commit_params_(std::move(commit_params)),
      navigation_ui_data_(std::move(navigation_ui_data)),
      blob_url_loader_factory_(std::move(blob_url_loader_factory)),
      restore_type_(entry ? entry->restore_type() : RestoreType::kNotRestored),
      // Some navigations, such as renderer-initiated subframe navigations,
      // won't have a NavigationEntryImpl. Set |reload_type_| if applicable
      // for them.
      reload_type_(
          entry ? entry->reload_type()
                : NavigationTypeToReloadType(common_params_->navigation_type)),
      nav_entry_id_(entry ? entry->GetUniqueID() : 0),
      from_begin_navigation_(from_begin_navigation),
      site_info_(
          frame_tree_node_->navigator().controller().GetBrowserContext()),
      navigation_entry_offset_(
          EstimateHistoryOffset(frame_tree_node_->navigator().controller(),
                                common_params_->should_replace_current_entry)),
      prefetched_signed_exchange_cache_(
          std::move(prefetched_signed_exchange_cache)),
      rfh_restored_from_back_forward_cache_(
          rfh_restored_from_back_forward_cache),
      is_back_forward_cache_restore_(
          rfh_restored_from_back_forward_cache.has_value()),
      // Store the old RenderFrameHost id at request creation to be used later.
      current_render_frame_host_id_at_construction_(
          frame_tree_node->current_frame_host()->GetGlobalId()),
      initiator_frame_token_(begin_params_->initiator_frame_token),
      initiator_process_id_(initiator_process_id),
      sandbox_flags_initiator_(GetSandboxFlagsInitiator(
          initiator_frame_token_,
          initiator_process_id,
          static_cast<StoragePartitionImpl*>(
              GetStoragePartitionWithCurrentSiteInfo()))),
      was_opener_suppressed_(was_opener_suppressed),
      is_credentialless_(
          IsDocumentToCommitAnonymous(frame_tree_node,
                                      is_synchronous_renderer_commit)),
      previous_page_ukm_source_id_(GetPageUkmSourceId(frame_tree_node_)),
      is_pdf_(is_pdf),
      is_embedder_initiated_fenced_frame_navigation_(
          is_embedder_initiated_fenced_frame_navigation),
      fenced_frame_properties_(
          is_embedder_initiated_fenced_frame_navigation
              ? std::make_optional(FencedFrameProperties(common_params_->url))
              : std::nullopt),
      embedder_shared_storage_context_(embedder_shared_storage_context),
      has_ad_auction_headers_attribute_(frame_tree_node->ad_auction_headers()),
      request_method_(common_params_->method) {
  TRACE_EVENT_WITH_FLOW1("navigation", "NavigationRequest::NavigationRequest",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_OUT, "navigation_request", this);
  CHECK(!common_params_->initiator_base_url ||
        !common_params_->initiator_base_url->is_empty());
  DCHECK(!blink::IsRendererDebugURL(common_params_->url));
  DCHECK(common_params_->method == "POST" || !common_params_->post_data);
  DCHECK_EQ(common_params_->url, commit_params_->original_url);
  // Navigations can't be a replacement and a reload at the same time.
  DCHECK(!common_params_->should_replace_current_entry ||
         !NavigationTypeUtils::IsReload(common_params_->navigation_type));
  DCHECK(IsInOutermostMainFrame() ||
         common_params_->base_url_for_data_url.is_empty());
#if BUILDFLAG(IS_ANDROID)
  DCHECK(IsInOutermostMainFrame() ||
         commit_params_->data_url_as_string.empty());
#endif
  CheckSoftNavigationHeuristicsInvariants();

  ScopedCrashKeys crash_keys(*this);

  ComputeDownloadPolicy();

  // Ensure the blink::RuntimeFeatureStateContext is initialized.
  runtime_feature_state_context_ = blink::RuntimeFeatureStateContext();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("navigation", "NavigationRequest",
                                    navigation_id_, "navigation_request", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("navigation", "Initializing",
                                    navigation_id_);

  if (GetInitiatorFrameToken().has_value()) {
    RenderFrameHostImpl* initiator_rfh = RenderFrameHostImpl::FromFrameToken(
        GetInitiatorProcessId(), GetInitiatorFrameToken().value());
    if (initiator_rfh)
      initiator_document_token_ = initiator_rfh->GetDocumentToken();
  }

  // Spec: https://github.com/whatwg/html/issues/8846
  // We only allow the parent to access a subframe resource timing if the
  // navigation is container-initiated, e.g. iframe changed src.
  if (begin_params_->is_container_initiated) {
    // Only same-origin navigations without cross-origin redirects can
    // expose response details (status-code / mime-type).
    // https://github.com/whatwg/fetch/issues/1602
    // Note that this condition checks this navigation is not cross origin.
    // Cross-origin redirects are checked as part of OnRequestRedirected().
    commit_params_->navigation_timing->parent_resource_timing_access =
        GetParentFrame()->GetLastCommittedOrigin().IsSameOriginWith(GetURL())
            ? blink::mojom::ParentResourceTimingAccess::
                  kReportWithResponseDetails
            : blink::mojom::ParentResourceTimingAccess::
                  kReportWithoutResponseDetails;
  }

  navigation_or_document_handle_ =
      NavigationOrDocumentHandle::CreateForNavigation(*this);

  policy_container_builder_.emplace(
      GetParentFrame(),
      initiator_frame_token_.has_value() ? &*initiator_frame_token_ : nullptr,
      initiator_process_id_, GetStoragePartitionWithCurrentSiteInfo(),
      frame_entry);

  NavigationControllerImpl* controller = GetNavigationController();

  if (frame_entry) {
    frame_entry_item_sequence_number_ = frame_entry->item_sequence_number();
    frame_entry_document_sequence_number_ =
        frame_entry->document_sequence_number();
  }

  // Sanitize the referrer.
  common_params_->referrer = Referrer::SanitizeForRequest(
      common_params_->url, *common_params_->referrer);

  if (IsInPrimaryMainFrame()) {
    loading_mem_tracker_ = PeakGpuMemoryTrackerFactory::Create(
        input::PeakGpuMemoryTracker::Usage::PAGE_LOAD);
  }

  if (frame_tree_node_->IsInFencedFrameTree()) {
    commit_params_->frame_policy.sandbox_flags |=
        blink::kFencedFrameForcedSandboxFlags;
  }

  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI) &&
      base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM118)) {
    shared_storage_writable_opted_in_ =
        frame_tree_node_->shared_storage_writable_opted_in();
    shared_storage_writable_eligible_ =
        IsSharedStorageWritableEligibleForNavigationRequest(
            frame_tree_node_, common_params_->url);
  }

  if (from_begin_navigation_) {
    // This is needed to have data URLs commit in the same SiteInstance as the
    // initiating renderer.
    source_site_instance_ =
        frame_tree_node->current_frame_host()->GetSiteInstance();

    DCHECK(navigation_client.is_valid());
    SetNavigationClient(std::move(navigation_client));

    // Wait for renderer-initiated cancellation if needed. Navigation can
    // proceed as soon as the corresponding JS task in the renderer finishes
    // without calling window.stop() or other navigation cancellation triggers.
    // That means there is no need to synchronise this signal with other
    // renderer events, so this interface doesn't have to be associated and can
    // use a prioritized task runner.
    // kNavigationNetworkResponse is used as CommitNavigation typically already
    // runs in on a task from this task runner (via OnResponseReceived message
    // received from the network service).
    if (renderer_cancellation_listener.is_valid()) {
      renderer_cancellation_listener_.Bind(
          std::move(renderer_cancellation_listener),
          GetUIThreadTaskRunner({BrowserTaskType::kNavigationNetworkResponse}));
    }
  } else if (entry) {
    DCHECK(!navigation_client.is_valid());
    if (frame_entry) {
      source_site_instance_ = frame_entry->source_site_instance();
      dest_site_instance_ = frame_entry->site_instance();
      bindings_ = frame_entry->bindings();

      // Handle navigations that require a |source_site_instance| but do not
      // have one set yet. This can happen when navigation entries are restored
      // from PageState objects, because the serialized state does not contain a
      // SiteInstance so we need to use the |initiator_origin| to get an
      // appropriate source SiteInstance. This can also happen when the
      // |source_site_instance| was suppressed because of navigating in
      // "noopener" mode.
      //
      // History subframe, restore navigations and |was_opener_suppressed| are
      // the only cases where SetSourceSiteInstanceToInitiatorIfNeeded needs to
      // be called (i.e. the only cases that may have no |source_site_instance_|
      // even though RequiresInitiatorBasedSourceSiteInstance returns true).  We
      // verify that other cases which require a |source_site_instance_| indeed
      // have one with a DCHECK below.
      if (common_params_->is_history_navigation_in_new_child_frame ||
          common_params_->navigation_type ==
              blink::mojom::NavigationType::RESTORE ||
          common_params_->navigation_type ==
              blink::mojom::NavigationType::RESTORE_WITH_POST ||
          was_opener_suppressed) {
        SetSourceSiteInstanceToInitiatorIfNeeded();
      }
    }
    isolation_info_ = entry->isolation_info();

    // Ensure that we always have a |source_site_instance_| for navigations
    // that require it at this point. This is needed to ensure that data: URLs
    // commit in the SiteInstance that initiated them.
    //
    // TODO(acolwell): Move this below so it can be enforced on all paths.
    // This requires auditing same-document and other navigations that don't
    // have |from_begin_navigation_| or |entry| set.
    DCHECK(!RequiresInitiatorBasedSourceSiteInstance() ||
           source_site_instance_);
  }

  // Let the NTP override the navigation params and pretend that this is a
  // browser-initiated, bookmark-like navigation.
  // TODO(crbug.com/40702467): determine why some link navigations on chrome://
  //     pages have |browser_initiated| set to true and others set to false.
  if (source_site_instance_) {
    bool is_renderer_initiated = !browser_initiated;
    Referrer referrer(*common_params_->referrer);
    ui::PageTransition transition =
        ui::PageTransitionFromInt(common_params_->transition);
    GetContentClient()->browser()->OverrideNavigationParams(
        source_site_instance_->GetProcess()->GetProcessLock().site_url(),
        &transition, &is_renderer_initiated, &referrer,
        &common_params_->initiator_origin);
    common_params_->transition = transition;
    common_params_->referrer =
        blink::mojom::Referrer::New(referrer.url, referrer.policy);
    browser_initiated = !is_renderer_initiated;
  }
  commit_params_->is_browser_initiated = browser_initiated;

  // Update the load flags with cache information.
  UpdateLoadFlagsWithCacheFlags(&begin_params_->load_flags,
                                common_params_->navigation_type,
                                common_params_->method == "POST");

  // Add necessary headers that may not be present in the
  // blink::mojom::BeginNavigationParams.
  if (entry) {
    // TODO(altimin, crbug.com/933147): Remove this logic after we are done
    // with implementing back-forward cache.
    if (frame_tree_node->IsOutermostMainFrame() &&
        entry->back_forward_cache_metrics()) {
      entry->back_forward_cache_metrics()
          ->MainFrameDidStartNavigationToDocument();
    }

    // If this NavigationRequest is for the current pending entry, make sure
    // that we will discard the pending entry if all of associated its requests
    // go away, by creating a ref to it.
    if (entry == controller->GetPendingEntry())
      pending_entry_ref_ = controller->ReferencePendingEntry();

    // |commit_params->is_overriding_user_agent| is the single source of truth
    // in NavigationRequest. For history navigations, callers of this
    // constructor must not provide conflicting requirements. Only
    // |commit_params->is_overriding_user_agent| will be taken into account.
    DCHECK_EQ(is_overriding_user_agent(), entry->GetIsOverridingUserAgent());
  }

  net::HttpRequestHeaders headers;
  // Only add specific headers when creating a NavigationRequest before the
  // network request is made, not at commit time.
  if (!is_synchronous_renderer_commit) {
    BrowserContext* browser_context = controller->GetBrowserContext();
    ClientHintsControllerDelegate* client_hints_delegate =
        browser_context->GetClientHintsControllerDelegate();
    // Loading an about:srcdoc url on the main frame will cause a failure later
    // and GetTentativeOriginAtRequestTime() can't handle it until then.
    if ((CheckAboutSrcDoc() != AboutSrcDocCheckResult::BLOCK_REQUEST) &&
        client_hints_delegate) {
      net::HttpRequestHeaders client_hints_headers;
      AddNavigationRequestClientHintsHeaders(
          GetTentativeOriginAtRequestTime(), &client_hints_headers,
          browser_context, client_hints_delegate, is_overriding_user_agent(),
          frame_tree_node_, commit_params_->frame_policy.container_policy,
          common_params_->url);
      headers.MergeFrom(client_hints_headers);
    }

    // Add reduced accept language header.
    if (auto reduce_accept_lang_utils =
            ReduceAcceptLanguageUtils::Create(browser_context);
        reduce_accept_lang_utils && !devtools_accept_language_override_ &&
        !ReduceAcceptLanguageUtils::CheckDisableReduceAcceptLanguageOriginTrial(
            common_params_->url, frame_tree_node_,
            browser_context->GetOriginTrialsControllerDelegate())) {
      // Add the Accept-Language header with the reduce accept language value.
      // Chromium network stack won't overwrite the value if Accept-Language
      // header was already added in the request header.
      net::HttpRequestHeaders accept_language_headers;
      std::optional<std::string> reduced_accept_language =
          reduce_accept_lang_utils.value()
              .AddNavigationRequestAcceptLanguageHeaders(
                  url::Origin::Create(common_params_->url), frame_tree_node_,
                  &accept_language_headers);
      commit_params_->reduced_accept_language =
          reduced_accept_language.value_or("");
      headers.MergeFrom(accept_language_headers);
    }

    headers.AddHeadersFromString(begin_params_->headers);
    AddAdditionalRequestHeaders(
        &headers, common_params_->url, common_params_->navigation_type,
        ui::PageTransitionFromInt(common_params_->transition),
        controller->GetBrowserContext(), common_params_->method,
        GetUserAgentOverride(), common_params_->initiator_origin,
        common_params_->referrer.get(), frame_tree_node);

    if (begin_params_->is_form_submission) {
      // During form resubmit, `commit_params_->post_content_type` is populated
      // from the history. Use it.
      if (!commit_params_->post_content_type.empty()) {
        headers.SetHeaderIfMissing(net::HttpRequestHeaders::kContentType,
                                   commit_params_->post_content_type);
      }

      // Save the Content-Type in case the form is resubmitted. This will get
      // sent back to the renderer in the CommitNavigation IPC. The renderer
      // will then send it back with the post body so that we can access it
      // along with the body in FrameNavigationEntry::page_state_.
      if (std::optional<std::string> content_type =
              headers.GetHeader(net::HttpRequestHeaders::kContentType);
          content_type) {
        commit_params_->post_content_type = std::move(content_type).value();
      }
    }

    TopicsHeaderValueResult topics_header_value_result =
        GetTopicsHeaderValueForNavigationRequest(frame_tree_node,
                                                 common_params_->url);

    topics_eligible_ = topics_header_value_result.topics_eligible;

    if (topics_header_value_result.header_value) {
      headers.SetHeader(kBrowsingTopicsRequestHeaderKey,
                        *topics_header_value_result.header_value);
    }

    if (has_ad_auction_headers_attribute_ &&
        IsAdAuctionHeadersEligibleForNavigation(
            *frame_tree_node_, url::Origin::Create(common_params_->url))) {
      ad_auction_headers_eligible_ = true;
      headers.SetHeader(kAdAuctionRequestHeaderKey, "?1");
    }
  }

  begin_params_->headers = headers.ToString();

#if BUILDFLAG(IS_ANDROID)
  RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
      frame_tree_node_->current_frame_host()->GetRenderWidgetHost());
  if (NeedsUrlLoader() && IsInPrimaryMainFrame() && host &&
      !host->is_hidden() && host->GetView() &&
      host->GetView()->GetNativeView() &&
      host->GetView()->GetNativeView()->GetWindowAndroid()) {
    // If the compositor changes, we will just let the lock timeout instead of
    // trying to deal with it explicitly.
    ui::WindowAndroidCompositor* compositor =
        host->GetView()->GetNativeView()->GetWindowAndroid()->GetCompositor();
    if (compositor) {
      compositor_lock_ = compositor->GetCompositorLock(kCompositorLockTimeout);
    }
  }

  navigation_handle_proxy_ = std::make_unique<NavigationHandleProxy>(this);
#endif

  if (NeedsUrlLoader() && common_params_->url.SchemeIsHTTPOrHTTPS()) {
    if (GetContentClient()->browser()->ShouldPreconnectNavigation(
            frame_tree_node_->current_frame_host())) {
      auto* storage_partition =
          frame_tree_node_->current_frame_host()->GetStoragePartition();
      storage_partition->GetNetworkContext()->PreconnectSockets(
          1, common_params_->url, network::mojom::CredentialsMode::kInclude,
          GetIsolationInfo().network_anonymization_key());
    }
  }

  if (NeedsUrlLoader() && IsInOutermostMainFrame()) {
    MaybePrewarmHttpDiskCache(*controller->GetBrowserContext(), GetURL(),
                              GetInitiatorOrigin());
  }

  // Checking OriginCanAccessServiceWorkers() is needed before calling
  // GetTentativeOriginAtRequestTime() since loading an about:srcdoc URL
  // on the main frame will cause a failure while processing
  // GetTentativeOriginAtRequestTime().
  if (OriginCanAccessServiceWorkers(GetURL())) {
    // Preflight request for FindRegistrationForClientUrl. This
    // preflight request speeds-up the upcoming
    // FindRegistrationForClientUrl requests because the upcoming
    // requests will be merged into this preflight request in
    // `ServiceWorkerRegistry::FindRegistrationForClientUrl()` and
    // `ServiceWorkerRegistry::RunFindRegistrationCallbacks()` later.
    if (ServiceWorkerContext* context =
            frame_tree_node_->navigator()
                .controller()
                .GetBrowserContext()
                ->GetStoragePartition(site_info_.storage_partition_config())
                ->GetServiceWorkerContext()) {
      const blink::StorageKey key = blink::StorageKey::CreateFirstParty(
          GetTentativeOriginAtRequestTime());
      if (context->MaybeHasRegistrationForStorageKey(key)) {
        // `CheckHasServiceWorker` calls `FindRegistrationForClientUrl`
        // internally.
        context->CheckHasServiceWorker(GetURL(), key, base::DoNothing());
      }
    }

    // Ask the service worker context to speculatively start a service worker
    // for the request URL if necessary for optimization purposes. Don't ask to
    // do that if this request is for ReloadType::BYPASSING_CACHE that is
    // supposed to skip a service worker. There are cases where we have already
    // started the service worker (e.g, Prerendering or the previous navigation
    // already started the service worker), but this call does nothing if the
    // service worker already started for the URL.
    if (reload_type_ != ReloadType::BYPASSING_CACHE &&
        base::FeatureList::IsEnabled(
            features::kSpeculativeServiceWorkerStartup)) {
      if (ServiceWorkerContext* context =
              frame_tree_node_->navigator()
                  .controller()
                  .GetBrowserContext()
                  ->GetStoragePartition(site_info_.storage_partition_config())
                  ->GetServiceWorkerContext()) {
        const blink::StorageKey key = blink::StorageKey::CreateFirstParty(
            GetTentativeOriginAtRequestTime());
        if (context->MaybeHasRegistrationForStorageKey(key)) {
          context->StartServiceWorkerForNavigationHint(GetURL(), key,
                                                       base::DoNothing());
        }
      }
    }
  }

  // Only update the BackForwardCacheMetrics if this is for a navigation that
  // could've been served from the bfcache.
  if (IsBackForwardCacheEnabled() && !IsServedFromBackForwardCache() && entry &&
      BackForwardCacheMetrics::IsCrossDocumentMainFrameHistoryNavigation(
          this)) {
    // Update NotRestoredReasons and create a metrics object if there's none.
    entry->UpdateBackForwardCacheNotRestoredReasons(this);
    auto* metrics = entry->back_forward_cache_metrics();
    DCHECK(metrics);
    if (base::FeatureList::IsEnabled(
            blink::features::kBackForwardCacheSendNotRestoredReasons)) {
      // Only populate the web-exposed NotRestoredReasons when needed by
      // the NotRestoredReasons API.
      commit_params_->not_restored_reasons =
          metrics->GetWebExposedNotRestoredReasons();
      // Check that the reasons are not null since |this| is not served from
      // back/forward cache.
      CHECK(!commit_params_->not_restored_reasons.is_null());
    }
  }
  // Check that the reasons are null when |this| is served from back/forward
  // cache.
  if (base::FeatureList::IsEnabled(
          blink::features::kBackForwardCacheSendNotRestoredReasons) &&
      IsBackForwardCacheEnabled() && IsServedFromBackForwardCache()) {
    CHECK(commit_params_->not_restored_reasons.is_null());
  }

  // Record `SameDocumentCrossOriginInitiator` metric. It happens in the
  // NavigationRequest constructor, to catch every kind of same-document
  // navigation: the one initiated from the navigating frame's process, and the
  // others.
  if (common_params_->navigation_type ==
          blink::mojom::NavigationType::SAME_DOCUMENT &&
      GetInitiatorOrigin() &&
      !GetInitiatorOrigin()->IsSameOriginWith(
          GetTentativeOriginAtRequestTime())) {
    // This is reported to navigating frame's current document, because this is
    // the document that behave differently if this navigation was turned into a
    // cross-document one.
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        frame_tree_node_->current_frame_host(),
        blink::mojom::WebFeature::kSameDocumentCrossOriginInitiator);
  }
}

NavigationRequest::~NavigationRequest() {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::~NavigationRequest",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN);
#if DCHECK_IS_ON()
  // If |is_safe_to_delete_| is false, it means |this| is being deleted at an
  // unexpected time, more specifically a time that is likely to lead to
  // crashing when the stack unwinds (use after free). The typical scenario for
  // this is calling to the delegate when the delegate is not expected to make
  // any sort of state change. For example, when the delegate is informed that a
  // navigation has started the delegate is not expected to call Stop().
  DCHECK(is_safe_to_delete_);
#endif

  // Close the last child event. Passing nullptr as the event name will match
  // the end event with the last unmatched begin event.
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", nullptr, navigation_id_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", "NavigationRequest",
                                  navigation_id_);

  // IMPORTANT NOTE: DO NOT return early from the destructor before this line.
  // Otherwise, a queued navigation might get stuck in a queueing state forever.
  // This navigation has finished. See if there is another NavigationRequest
  // that lives in the associated FrameTreeNode that satisfies these conditions:
  // - Is currently queued to wait for a pending commit navigation to finish
  // - Is not the NavigationRequest that is currently being destructed itself
  if (NavigationRequest* request = frame_tree_node_->navigation_request()) {
    if (request->IsQueued() && request != this) {
      // It might be possible for the pending commit RFH to still exist, e.g. if
      // the navigation being destructed is an unrelated navigation
      // (same-document navigation etc). In that case, don't continue the queued
      // navigation just yet.
      if (!request->ShouldQueueDueToExistingPendingCommitRFH()) {
        request->PostResumeCommitTask();
      }
    }
  }

  if (loading_mem_tracker_)
    loading_mem_tracker_->Cancel();
  ResetExpectedProcess();

  if (IsInPrimaryMainFrame()) {
    if (auto* cache =
            GetNavigationController()->GetNavigationEntryScreenshotCache()) {
      cache->OnNavigationFinished(*this);
    }
  }

  if (HasCommitted()) {
    CHECK(!navigation_discard_reason_.has_value());
  } else {
    CHECK(navigation_discard_reason_.has_value());

    // If we're before WILL_START_NAVIGATION, we haven't reported request start
    // to DevTools yet.
    // If we're in WILL_FAIL_REQUEST, the failure has been reported already.
    if (state_ >= WILL_START_NAVIGATION && state_ != WILL_FAIL_REQUEST) {
      devtools_instrumentation::OnNavigationRequestFailed(
          *this, network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    }

    // NavigationRequests with pending Navigation API keys must notify the
    // renderer when they fail.
    if (pending_navigation_api_key_) {
      frame_tree_node_->current_frame_host()
          ->GetAssociatedLocalFrame()
          ->TraverseCancelled(
              *pending_navigation_api_key_,
              blink::mojom::TraverseCancelledReason::kAbortedBeforeCommit);
    }

    // If subframe history navigations were deferred waiting for this request,
    // the cancelation of this request should cancel them, too.
    for (auto& throttle : subframe_history_navigation_throttles_) {
      if (throttle) {
        throttle->Cancel();
      }
    }
    subframe_history_navigation_throttles_.clear();
  }

  // If this NavigationRequest is the last one referencing the pending
  // NavigationEntry, the entry is discarded.
  //
  // Leaving a stale pending NavigationEntry with no matching navigation can
  // potentially lead to URL-spoof issues.
  //
  // Note: Discarding the pending NavigationEntry is done before notifying the
  // navigation finished to the observers. One class is relying on this:
  // org.chromium.chrome.browser.toolbar.ToolbarManager
  pending_entry_ref_.reset();

#if BUILDFLAG(IS_ANDROID)
  if (navigation_visible_to_embedder_)
    navigation_handle_proxy_->DidFinish();
#endif

  if (is_deferred_on_fenced_frame_url_mapping_) {
    CHECK(NeedFencedFrameURLMapping());
    GetFencedFrameURLMap().RemoveObserverForURN(common_params_->url, this);
  }

  RecordEarlyRenderFrameHostSwapMetrics();

  if (IsNavigationStarted()) {
    GetDelegate()->DidFinishNavigation(this);
    ProcessOriginAgentClusterEndResult();
    if (IsInMainFrame()) {
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          "navigation", "Navigation StartToCommit",
          TRACE_ID_WITH_SCOPE("StartToCommit", TRACE_ID_LOCAL(this)), "URL",
          common_params_->url.spec(), "Net Error Code", net_error_);
      MaybeRecordTraceEventsAndHistograms();
    }

    // Abandon the prerender host reserved for activation if it exists.
    if (IsPrerenderedPageActivation()) {
      GetPrerenderHostRegistry().OnActivationFinished(
          prerender_frame_tree_node_id_.value());
    }

    if (!HasCommitted()) {
      ResetViewTransitionState();
    }

    if (IsServedFromBackForwardCache()) {
      auto bfcache_entry =
          GetNavigationController()->GetBackForwardCache().GetOrEvictEntry(
              nav_entry_id());
      if (!bfcache_entry.has_value() &&
          bfcache_entry.error() ==
              BackForwardCacheImpl::kEntryIneligibleAndEvicted) {
        // DO NOT ADD CODE after this. When BFCache entry is evicted, the
        // current NavigationRequest has been destroyed.
        return;
      }
      if (bfcache_entry.has_value()) {
        RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(
            bfcache_entry.value()->render_frame_host()->GetGlobalId());
        // RFH could have been deleted. E.g. eviction timer fired
        if (rfh && rfh->IsInBackForwardCache()) {
          // rfh is still in the cache so the navigation must have failed. But
          // we have already disabled eviction so the safest thing to do here to
          // recover is to evict.
          // TODO(crbug.com/40283427): Only evict BFCache if the
          // `BackForwardCacheCommitDeferringCondition`, which unfreezes the
          // page and disables the eviction on the renderer side, is completed.
          rfh->EvictFromBackForwardCacheWithReason(
              BackForwardCacheMetrics::NotRestoredReason::
                  kNavigationCancelledWhileRestoring);
        }
      }
    }
  } else {
    GetDelegate()->DidCancelNavigationBeforeStart(this);
  }
}

void NavigationRequest::RegisterCommitDeferringConditionForTesting(
    std::unique_ptr<CommitDeferringCondition> condition) {
  commit_deferrer_->AddConditionForTesting(std::move(condition));  // IN-TEST
}

bool NavigationRequest::IsCommitDeferringConditionDeferredForTesting() {
  if (!commit_deferrer_)
    return false;
  return commit_deferrer_->GetDeferringConditionForTesting();  // IN-TEST
}

CommitDeferringCondition*
NavigationRequest::GetCommitDeferringConditionForTesting() {
  if (!commit_deferrer_)
    return nullptr;
  return commit_deferrer_->GetDeferringConditionForTesting();  // IN-TEST
}

void NavigationRequest::BeginNavigation() {
  begin_navigation_time_ = base::TimeTicks::Now();
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::BeginNavigation",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  EnterChildTraceEvent("BeginNavigation", this);
  DCHECK(!loader_);
  DCHECK(!HasRenderFrameHost());
  ScopedCrashKeys crash_keys(*this);

  if (begin_navigation_callback_for_testing_) {
    std::move(begin_navigation_callback_for_testing_).Run();
  }

  if (MaybeStartPrerenderingActivationChecks()) {
    // BeginNavigationImpl() will be called after the checks.
    return;
  }

  MaybeAssignInvalidPrerenderFrameTreeNodeId();

  // Fenced frames are not allowed to load if nested in iframes with CSPEE.
  bool is_fenced_frame = frame_tree_node_->IsFencedFrameRoot();
  if (is_fenced_frame) {
    DCHECK(!frame_tree_node_->csp_attribute());
    if (GetParentFrameOrOuterDocument()->required_csp()) {
      GURL sanitized_blocked_url =
          common_params_->url.DeprecatedGetOriginAsURL();
      AddDeferredConsoleMessage(
          blink::mojom::ConsoleMessageLevel::kError,
          base::StringPrintf(
              "Refused to frame '%s' as a fenced frame because "
              "CSP Embedded Enforcement is specified by the embedder",
              sanitized_blocked_url.spec().c_str()));

      StartNavigation();
      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_CSP),
          false /*skip_throttles*/, std::nullopt /*error_page_content*/,
          false /*collapse_frame*/);
      // DO NOT ADD CODE after this. The previous call to
      // OnRequestFailedInternal has destroyed the NavigationRequest.
      return;
    }
  }

  // If this is a fenced frame with a urn:uuid, or an iframe with a urn::uuid
  // given blink::features::kAllowURNsInIframes is enabled, then convert it to a
  // url before starting the navigation; otherwise, proceed directly with the
  // navigation.
  // In long term, navigation support for urn::uuid in iframes will be
  // deprecated. Currently we issue a console warning when navigation starts.
  // TODO(crbug.com/40060657)
  if (NeedFencedFrameURLMapping()) {
    if (!is_fenced_frame) {
      // Iframes with urn::uuid.
      DCHECK(!frame_tree_node_->IsMainFrame());
      DCHECK(blink::features::IsAllowURNsInIframeEnabled());
      if (blink::features::DisplayWarningDeprecateURNIframesUseFencedFrames()) {
        AddDeferredConsoleMessage(
            blink::mojom::ConsoleMessageLevel::kWarning,
            "Protected Audience/selectURL will deprecate supporting iframes to "
            "render the winning ad/selected URL. "
            "Please use fenced frames instead. See "
            "https://developer.chrome.com/en/docs/privacy-sandbox/fenced-frame/"
            "#examples");
      }
    }

    UMA_HISTOGRAM_BOOLEAN(
        "Navigation.BrowserMappedUrnUuidInIframeOrFencedFrame",
        !is_fenced_frame);

    FencedFrameURLMapping& fenced_frame_urls_map = GetFencedFrameURLMap();

    // If the mapping finishes synchronously, OnFencedFrameURLMappingComplete
    // will be synchronously called and will reset
    // `is_deferred_on_fenced_frame_url_mapping_` to false.
    is_deferred_on_fenced_frame_url_mapping_ = true;

    fenced_frame_url_mapping_start_time_ = base::TimeTicks::Now();

    // OnFencedFrameURLMappingComplete() and BeginNavigationImpl() will be
    // invoked after this.
    fenced_frame_urls_map.ConvertFencedFrameURNToURL(common_params_->url,
                                                     /*observer=*/this);
    // DO NOT ADD CODE after this. The previous call to
    // ConvertFencedFrameURNToURL may cause the destruction of the
    // NavigationRequest.
    return;
  }

  // Send any potential navigation start automatic beacons for this frame.
  frame_tree_node_->current_frame_host()
      ->MaybeSendFencedFrameAutomaticReportingBeacon(
          *this, blink::mojom::AutomaticBeaconType::kTopNavigationStart);

  // Log a histogram for a top-level navigation that initiates from a fenced
  // frame or URN iframe.
  if (GetInitiatorDocumentRenderFrameHost() &&
      GetInitiatorDocumentRenderFrameHost()
          ->frame_tree_node()
          ->GetFencedFrameProperties()
          .has_value() &&
      IsInOutermostMainFrame()) {
    base::UmaHistogramEnumeration(blink::kFencedFrameTopNavigationHistogram,
                                  blink::FencedFrameNavigationState::kBegin);
  }

  BeginNavigationImpl();
}

bool NavigationRequest::MaybeStartPrerenderingActivationChecks() {
  // Find an available prerendered page for this request. If it's found, this
  // request may activate it instead of loading a page via network.
  FrameTreeNodeId candidate_prerender_frame_tree_node_id =
      GetPrerenderHostRegistry().FindPotentialHostToActivate(*this);
  if (candidate_prerender_frame_tree_node_id.is_null()) {
    return false;
  }

  // Run CommitDeferringConditions before activating the prerendered page. See
  // the comemnt on RunCommitDeferringConditions() for details.
  //
  // The prerendered page can be destroyed while the conditions are running.
  // In that case, this request gives up activating it and instead falls back to
  // a regular navigation.
  commit_deferrer_ = CommitDeferringConditionRunner::Create(
      *this,
      CommitDeferringCondition::NavigationType::kPrerenderedPageActivation,
      candidate_prerender_frame_tree_node_id);
  is_running_potential_prerender_activation_checks_ = true;

  // Post a task to run the conditions in case BeginNavigation() is not expected
  // to run synchronously. OnPrerenderingActivationChecksComplete() will be
  // called after all the deferring conditions finish.
  base::SequencedTaskRunner::GetCurrentDefault()->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&NavigationRequest::RunCommitDeferringConditions,
                     weak_factory_.GetWeakPtr()));
  return true;
}

void NavigationRequest::OnPrerenderingActivationChecksComplete(
    CommitDeferringCondition::NavigationType navigation_type,
    std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id) {
  TRACE_EVENT_WITH_FLOW0(
      "navigation", "NavigationRequest::OnPrerenderingActivationChecksComplete",
      TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                          TRACE_ID_LOCAL(navigation_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Prerendered page activation must run CommitDeferringConditions before
  // StartRequest().
  DCHECK_LT(state_, WILL_START_NAVIGATION);

  DCHECK(candidate_prerender_frame_tree_node_id.has_value());
  DCHECK(!prerender_frame_tree_node_id_.has_value());

  // Attempt to reserve the potential PrerenderHost.
  //
  // If it has been requested to cancel prerendered page activation during
  // CommitDeferringConditions, ReserveHostToActivate() returns an invalid
  // FrameTreeNodeId, and then NavigationRequest continues as regular
  // navigation.
  prerender_frame_tree_node_id_ =
      GetPrerenderHostRegistry().ReserveHostToActivate(
          *this, candidate_prerender_frame_tree_node_id.value());
  if (prerender_frame_tree_node_id_.value().is_null()) {
    // If we ran commit deferring conditions for a potential pre-render which
    // eventually wasn't activated, abort the ViewTransition. The state was
    // cached assuming this navigation will be same-origin which might not be
    // the case now that we need to make a network request.
    ResetViewTransitionState();
  } else {
    // The reserved host should match with the potential host. Otherwise the
    // reserved host may not be ready for activation yet as we haven't run
    // PrerenderCommitDeferringCondition for the host to finish navigation in
    // the prerendering main frame.
    DCHECK_EQ(prerender_frame_tree_node_id_.value(),
              candidate_prerender_frame_tree_node_id.value());
  }
  is_running_potential_prerender_activation_checks_ = false;
  commit_deferrer_.reset();

  // We can only activate top-level pages, which can never be at a fenced frame
  // URN that needs to be mapped.
  CHECK(!NeedFencedFrameURLMapping());

  BeginNavigationImpl();
  // DO NOT ADD CODE after this. The previous call to
  // BeginNavigationImpl may cause the destruction of the NavigationRequest.
}

FencedFrameURLMapping& NavigationRequest::GetFencedFrameURLMap() {
  // The usual case here is a fenced frame root navigating to a URNs, in which
  // case we need to consult the `FencedFrameURLMapping` in the *outer*
  // FrameTree.
  bool is_fenced_frame_root =
      frame_tree_node_->current_frame_host()->IsFencedFrameRoot();
  FrameTreeNode* node_to_use = frame_tree_node_->frame_tree()
                                   .root()
                                   ->render_manager()
                                   ->GetOuterDelegateNode();

  // However the very unusual case is an *iframe* (that supports navigations to
  // URNs via `blink::features::IsAllowURNsInIframeEnabled`) navigating to a
  // URN, possibly *inside* of a fenced frame. We can remove support for this
  // case once third party cookies are removed.
  if (!is_fenced_frame_root) {
    node_to_use = frame_tree_node_;
  }
  DCHECK(node_to_use);
  return node_to_use->current_frame_host()->GetPage().fenced_frame_urls_map();
}

bool NavigationRequest::NeedFencedFrameURLMapping() {
  if (!blink::IsValidUrnUuidURL(common_params_->url)) {
    return false;
  }
  if (blink::features::IsAllowURNsInIframeEnabled() &&
      !frame_tree_node_->IsMainFrame() &&
      !frame_tree_node_->IsFencedFrameRoot()) {
    // When urn iframes are enabled, any urn:uuid navigation to an iframe is
    // resolved using the urn mapping.
    is_embedder_initiated_fenced_frame_navigation_ = true;
  }
  return is_embedder_initiated_fenced_frame_navigation_;
}

void NavigationRequest::OnFencedFrameURLMappingComplete(
    const std::optional<FencedFrameProperties>& properties) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::OnFencedFrameURLMappingComplete",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  is_deferred_on_fenced_frame_url_mapping_ = false;

  // The URL mapping might have failed (e.g. because the urn is invalid):
  if (!properties.has_value()) {
    // For iframes, try the urn as-is to maintain existing behavior which will
    // abort the navigation as the url is unresolvable.
    if (!frame_tree_node_->IsFencedFrameRoot()) {
      BeginNavigationImpl();  // DO NOT ADD CODE after this, because it might
                              // have destroyed `this`.
      return;
    }

    StartNavigation();
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_URL),
        false /* skip_throttles */, std::nullopt /* error_page_content*/,
        false /* collapse_frame */);
    return;
  }

  if (properties->on_navigate_callback()) {
    properties->on_navigate_callback().Run();
  }

  // Currently, all fenced frame use cases include mapped urls. Patch up
  // url-related fields to use the underlying mapped url, rather than the
  // original urn.
  CHECK(properties->mapped_url().has_value());
  const GURL& mapped_url_value =
      properties->mapped_url()->GetValueIgnoringVisibility();
  common_params_->url = mapped_url_value;
  commit_params_->original_url = mapped_url_value;

  // Store the browser's view of the fenced frame properties along with any
  // embedder context for shared storage in the`NavigationRequest`. Upon commit,
  // it will be stored in the fenced frame root `FrameTreeNode`.
  fenced_frame_properties_ = properties;

  // Set the shared storage context in the fenced frame properties.
  DCHECK(fenced_frame_properties_);
  fenced_frame_properties_->SetEmbedderSharedStorageContext(
      embedder_shared_storage_context_);
  embedder_shared_storage_context_ = std::nullopt;

  // For urns loaded into iframes, we disable certain aspects of fenced frames:
  // * a storage/network partition nonce
  // * the ability to call window.fence.disableUntrustedNetwork
  if (!frame_tree_node_->IsFencedFrameRoot()) {
    CHECK(blink::features::IsAllowURNsInIframeEnabled());
    fenced_frame_properties_->AdjustPropertiesForUrnIframe();
  }

  // This implies the URN is created from shared storage.
  if (fenced_frame_properties_->shared_storage_budget_metadata()) {
    base::TimeDelta time_spent_in_fenced_frame_url_mapping =
        base::TimeTicks::Now() - fenced_frame_url_mapping_start_time_;

    base::UmaHistogramTimes(
        "Storage.SharedStorage.Timing.UrlMappingDuringNavigation",
        time_spent_in_fenced_frame_url_mapping);
  }

  BeginNavigationImpl();  // DO NOT ADD CODE after this, because it might have
                          // destroyed `this`.
}

void NavigationRequest::BeginNavigationImpl() {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::BeginNavigationImpl",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  base::ElapsedTimer timer;
  SetState(WILL_START_NAVIGATION);
#if BUILDFLAG(IS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());
  bool should_override_url_loading = false;

  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(),
          commit_params_->is_browser_initiated, commit_params_->original_url,
          commit_params_->original_method, common_params_->has_user_gesture,
          false, frame_tree_node_->IsOutermostMainFrame(),
          frame_tree_node_->frame_tree().is_prerendering(),
          ui::PageTransitionFromInt(common_params_->transition),
          &should_override_url_loading)) {
    // A Java exception was thrown by the embedding application; we
    // need to return from this task. Specifically, it's not safe from
    // this point on to make any JNI calls.
    return;
  }

  // The content/ embedder might cause |this| to be deleted while
  // |ShouldOverrideUrlLoading| is called.
  // See https://crbug.com/770157.
  if (!this_ptr)
    return;

  if (should_override_url_loading) {
    // Don't create a NavigationHandle here to simulate what happened with the
    // old navigation code path (i.e. doesn't fire onPageFinished notification
    // for aborted loads).
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, std::nullopt /*error_page_content*/,
        false /*collapse_frame*/);
    return;
  }
#endif

  // Check Content Security Policy before the NavigationThrottles run. This
  // gives CSP a chance to modify requests that NavigationThrottles would
  // otherwise block. Similarly, the NavigationHandle is created afterwards, so
  // that it gets the request URL after potentially being modified by CSP.
  net::Error net_error = CheckContentSecurityPolicy(
      false /* has_followed redirect */,
      false /* url_upgraded_after_redirect */, false /* is_response_check */);
  if (net_error != net::OK) {
    // Create a navigation handle so that the correct error code can be set on
    // it by OnRequestFailedInternal().
    StartNavigation();
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            std::nullopt /* error_page_content */,
                            false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
      CredentialedSubresourceCheckResult::BLOCK_REQUEST) {
    // Create a navigation handle so that the correct error code can be set on
    // it by OnRequestFailedInternal().
    StartNavigation();
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles  */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  StartNavigation();

  // The previous call to `StartNavigation()` could have changed the
  // is_overriding_user_agent value in CommitNavigationParams. If we're trying
  // to restore an entry from the back-forward cache, we need to ensure that
  // the is_overriding_user_agent used in the RenderFrameHost to restore matches
  // the value set in CommitNavigationParams.
  if (IsServedFromBackForwardCache() &&
      GetRenderFrameHostRestoredFromBackForwardCache()
              ->GetPage()
              .is_overriding_user_agent() !=
          commit_params_->is_overriding_user_agent) {
    // Trigger an eviction, which will cancel this navigation and trigger a new
    // one to the same entry (but won't try to restore the entry from the
    // back-forward cache) asynchronously.
    GetRenderFrameHostRestoredFromBackForwardCache()
        ->EvictFromBackForwardCacheWithReason(
            BackForwardCacheMetrics::NotRestoredReason::
                kUserAgentOverrideDiffers);
    // DO NOT ADD CODE after this. The previous call to
    // `EvictFromBackForwardCacheWithReason()`
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckAboutSrcDoc() == AboutSrcDocCheckResult::BLOCK_REQUEST) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_URL),
        true /* skip_throttles */, std::nullopt /* error_page_content*/,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (!post_commit_error_page_html_.empty()) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net_error_),
        true /* skip_throttles  */,
        post_commit_error_page_html_ /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (IsForMhtmlSubframe())
    is_mhtml_or_subframe_ = true;

  // TODO(antoniosartori): This takes a snapshot of the 'csp' attribute. This
  // should be done at the beginning of the navigation instead. Otherwise, the
  // attribute might have change while waiting for the beforeunload handlers to
  // complete.
  SetupCSPEmbeddedEnforcement();

  if (!NeedsUrlLoader()) {
    // The types of pages that don't need a URL Loader should never get served
    // from the BackForwardCache or activated from a prerender.
    DCHECK(!IsServedFromBackForwardCache());
    DCHECK(!IsPrerenderedPageActivation());

    // There is no need to make a network request for this navigation, so commit
    // it immediately.
    EnterChildTraceEvent("ResponseStarted", this);

    // |CheckCSPEmbeddedEnforcement()| below populates the |required_csp_|. No
    // URLs will be blocked, because they are either:
    // - allowing blanket enforcement of CSP (about:blank, about:srcdoc, ...).
    // - MHTML document, not supported by CSPEE (https://crbug.com/1164353).
    if (CheckCSPEmbeddedEnforcement() ==
        CSPEmbeddedEnforcementResult::BLOCK_RESPONSE) {
      NOTREACHED_IN_MIGRATION();
      base::debug::DumpWithoutCrashing();
    }

    ComputePoliciesToCommit();

    // Same-document navigations occur in the currently loaded document. See
    // also RenderFrameHostManager::DidCreateNavigationRequest() which will
    // expect us to use the current RenderFrameHost for this NavigationRequest,
    // and https://crbug.com/1125106.
    if (IsSameDocument()) {
      render_frame_host_ = frame_tree_node_->current_frame_host()->GetSafeRef();

      // The SiteInstance should have a site already from the navigation that
      // committed the document, unless the scheme does not require a site. Same
      // document navigations cannot change scheme or origin, so it should be
      // equivalent to check the current vs destination UrlInfo.
      DCHECK(render_frame_host_.value()->GetSiteInstance()->HasSite() ||
             !SiteInstanceImpl::ShouldAssignSiteForUrlInfo(GetUrlInfo()));

      WillCommitWithoutUrlLoader();
      return;
    } else {
      // [spec]: https://html.spec.whatwg.org/C/#process-a-navigate-response
      // 4. if [...] the result of checking a navigation response's adherence to
      // its embedder policy [...], then set failure to true.
      if (!CheckResponseAdherenceToCoep(common_params_->url)) {
        OnRequestFailedInternal(network::URLLoaderCompletionStatus(
                                    network::mojom::BlockedByResponseReason::
                                        kCoepFrameResourceNeedsCoepHeader),
                                false /* skip_throttles */,
                                std::nullopt /* error_page_content */,
                                false /* collapse_frame */);
        return;
        // DO NOT ADD CODE after this. The previous call to
        // OnRequestFailedInternal has destroyed the NavigationRequest.
      }

      // Enforce cross-origin-opener-policy for about:blank, about:srcdoc and
      // MHTML iframe, before selecting the RenderFrameHost.
      const url::Origin origin = GetOriginForURLLoaderFactoryUnchecked();
      const net::SchemefulSite site = net::SchemefulSite(origin);

      // Set the COOP origin in the policy container builder via the mutable
      // reference before FinalPolicies() is called.
      std::optional<url::Origin>& coop_origin =
          policy_container_builder_->GetPolicyContainerHost()
              ->cross_origin_opener_policy()
              .origin;
      if (!coop_origin.has_value()) {
        coop_origin = origin;
      }
      coop_status_.EnforceCOOP(
          policy_container_builder_->FinalPolicies().cross_origin_opener_policy,
          origin, net::NetworkAnonymizationKey::CreateSameSite(site));

      SelectFrameHostForCrossDocumentNavigationWithNoUrlLoader();
      return;
    }
  }

  base::UmaHistogramTimes(
      base::StrCat({"Navigation.BeginNavigationImpl.",
                    IsInMainFrame() ? "MainFrame" : "Subframe"}),
      timer.Elapsed());
  WillStartRequest();
  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

void NavigationRequest::
    SelectFrameHostForCrossDocumentNavigationWithNoUrlLoader() {
  DCHECK(!NeedsUrlLoader());
  CHECK(!HasRenderFrameHost())
      << "`render_frame_host_` should not be set before the "
         "`NavigationRequest` starts to select the RFH.";

  if (auto result =
          frame_tree_node_->render_manager()->GetFrameHostForNavigation(
              this, &browsing_context_group_swap_);
      result.has_value()) {
    render_frame_host_ = result.value()->GetSafeRef();
  } else {
    switch (result.error()) {
      case GetFrameHostForNavigationFailed::kCouldNotReinitializeMainFrame:
        // TODO(crbug.com/40250311): This was unhandled before and
        // remains explicitly unhandled. This branch may be removed in the
        // future.
        break;
      case GetFrameHostForNavigationFailed::kBlockedByPendingCommit:
        resume_commit_closure_ = base::BindOnce(
            &NavigationRequest::
                SelectFrameHostForCrossDocumentNavigationWithNoUrlLoader,
            weak_factory_.GetWeakPtr());
        frame_tree_node_->render_manager()
            ->speculative_frame_host()
            ->RecordMetricsForBlockedGetFrameHostAttempt(
                /* commit_attempt=*/true);
        return;
      case GetFrameHostForNavigationFailed::kIntentionalDefer:
        // We will not defer RFH creation for requests without a URL loader
        NOTREACHED();
    }
  }

  CHECK(Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
      &*render_frame_host_.value(), GetUrlInfo(),
      /*is_renderer_initiated_check=*/false));

  auto* site_instance = render_frame_host_.value()->GetSiteInstance();
  if (!site_instance->HasSite() &&
      SiteInstanceImpl::ShouldAssignSiteForUrlInfo(GetUrlInfo())) {
    site_instance->ConvertToDefaultOrSetSite(GetUrlInfo());
  }

  WillCommitWithoutUrlLoader();
}

void NavigationRequest::SetWaitingForRendererResponse() {
  EnterChildTraceEvent("WaitingForRendererResponse", this);
  SetState(WAITING_FOR_RENDERER_RESPONSE);
}

bool NavigationRequest::ShouldAddCookieChangeListener() {
  // The `CookieChangeListener` will only be set up if all of these are true:
  // (1) the navigation's protocol is HTTP(s).
  // (2) we allow a document with `Cache-control: no-store` header to
  // enter BFCache.
  // (3) the navigation is neither a same-document navigation nor a page
  // activation, since in these cases, an existing `RenderFrameHost` will be
  // used, and it would already have an existing listener, so we should skip the
  // initialization.
  // (4) the navigation is a primary main frame navigation, as the cookie
  // change information will only be used in the inactive document control
  // logic.
  return frame_tree_node_->navigator()
             .controller()
             .GetBackForwardCache()
             .should_allow_storing_pages_with_cache_control_no_store() &&
         !IsPageActivation() && !IsSameDocument() && IsInPrimaryMainFrame() &&
         common_params_->url.SchemeIsHTTPOrHTTPS();
}

void NavigationRequest::StartNavigation() {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::StartNavigation",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(frame_tree_node_->navigation_request() == this ||
         is_synchronous_renderer_commit_);
  FrameTreeNode* frame_tree_node = frame_tree_node_;

  MaybeAssignInvalidPrerenderFrameTreeNodeId();

  // This is needed to get site URLs and assign the expected RenderProcessHost.
  // This is not always the same as |source_site_instance_|, as it only depends
  // on the current frame host, and does not depend on |entry|.
  // The |starting_site_instance_| needs to be set here instead of the
  // constructor since a navigation can be started after the constructor and
  // before here, which can set a different RenderFrameHost and a different
  // starting SiteInstance.
  starting_site_instance_ =
      frame_tree_node->current_frame_host()->GetSiteInstance();
  site_info_ = GetSiteInfoForCommonParamsURL();

  // It's important to start listening to the cookie changes before the network
  // request of the navigation begins in order to ensure the listener won't miss
  // any cookie changes that happen after the network request is sent that
  // potentially modify some cookie values that are used in this request.
  // The information of cookie modification will be used to determine if the
  // document that this navigation will load should be eligible for BFCache.
  // The listener eventually will be transferred over to the committed
  // `RenderFrameHost`.
  if (ShouldAddCookieChangeListener()) {
    // The listener should receive the change events of the cookies from the
    // the domain of the main-frame navigation url.
    // If the navigation gets redirected, it will be reset with the new URL when
    // `NavigationRequest::OnRequestRedirected()` is called.
    cookie_change_listener_ =
        std::make_unique<RenderFrameHostImpl::CookieChangeListener>(
            GetStoragePartitionWithCurrentSiteInfo(), common_params_->url);
  }

  // Compute the redirect chain.
  // TODO(clamy): Try to simplify this and have the redirects be part of
  // CommonNavigationParams.
  redirect_chain_.clear();
  if (!begin_params_->client_side_redirect_url.is_empty()) {
    // |begin_params_->client_side_redirect_url| will be set when the navigation
    // was triggered by a client-side redirect.
    redirect_chain_.push_back(begin_params_->client_side_redirect_url);
  } else if (!commit_params_->redirects.empty()) {
    // Redirects that were specified at NavigationRequest creation time should
    // be added to the list of redirects. In particular, if the
    // NavigationRequest was created at commit time, redirects that happened
    // during the navigation have been added to |commit_params_->redirects| and
    // should be passed to the NavigationHandle.
    for (const auto& url : commit_params_->redirects)
      redirect_chain_.push_back(url);
  }

  // Finally, add the current URL to the vector of redirects.
  // Note: for NavigationRequests created at commit time, the current URL has
  // been added to |commit_params_->redirects|, so don't add it a second time.
  if (!is_synchronous_renderer_commit_) {
    if (!common_params_->base_url_for_data_url.is_empty()) {
      // If this is a loadDataWithBaseURL/loadDataAsStringWithBaseUrl
      // navigation, use the base URL instead of the data: URL used for commit.
      redirect_chain_.push_back(common_params_->base_url_for_data_url);
    } else {
      redirect_chain_.push_back(common_params_->url);
    }
  }

  // Mirrors the logic in RenderFrameImpl::SendDidCommitProvisionalLoad.
  if (common_params_->transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
    // If the page contained a client redirect (meta refresh,
    // document.location), set the referrer appropriately.
    // Note that this value will stay the same, even after cross-origin
    // redirects. This means the referrer URL and policy in
    // `sanitized_referrer_` might not be the same as the actual referrer sent
    // for the final navigation request (which will be updated/re-sanitized on
    // each redirect).
    // TODO(crbug.com/40771822): Remove this special case, and also
    // `sanitized_referrer_` entirely, in favor of CommonNavigationParams'
    // `referrer`, which will be properly sanitized after each redirect.
    sanitized_referrer_ = blink::mojom::Referrer::New(
        redirect_chain_[0], Referrer::SanitizeForRequest(
                                common_params_->url, *common_params_->referrer)
                                ->policy);
  } else {
    sanitized_referrer_ = Referrer::SanitizeForRequest(
        common_params_->url, *common_params_->referrer);
  }

  // If the navigation explicitly requested for history list clearing (e.g. when
  // running layout tests), don't do a replacement (since there won't be any
  // entry to replace after the navigation).
  if (commit_params_->should_clear_history_list) {
    common_params_->should_replace_current_entry = false;
  } else if (
      ShouldReplaceCurrentEntryForSameUrlNavigation() ||
      ShouldReplaceCurrentEntryForNavigationFromInitialEmptyDocumentOrEntry()) {
    common_params_->should_replace_current_entry = true;
  }

  // Set the expected process for this navigation, if we can. The navigation
  // might not have an associated RenderFrameHost yet, which is possible if it
  // can't create a speculative RenderFrameHost when there's a pending commit
  // navigation (when navigation queueing is enabled), or it had an associated
  // RenderFrameHost when the NavigationRequest was created but another
  // navigation had committed in between that time and StartNavigation, which
  // invalidates the `associated_rfh_type_`, or it's intentionally deferred with
  // feature flag DeferSpeculativeRFHCreation. It's fine to skip setting the
  // expected process in this case, as we'll set the expected process again from
  // ReadyToCommitNavigation(), when we know the final RenderFrameHost for the
  // navigation.
  SetExpectedProcessIfAssociated();

  DCHECK(!IsNavigationStarted());
  SetState(WILL_START_REQUEST);
  is_navigation_started_ = true;

  modified_request_headers_.Clear();
  removed_request_headers_.clear();

  throttle_runner_ = std::make_unique<NavigationThrottleRunner>(
      this, navigation_id_, IsInPrimaryMainFrame());

  // For prerendered page activation, CommitDeferringConditions have already run
  // at the beginning of the navigation, so we won't run them again.
  if (!IsPrerenderedPageActivation()) {
    commit_deferrer_ = CommitDeferringConditionRunner::Create(
        *this, CommitDeferringCondition::NavigationType::kOther,
        /*candidate_prerender_frame_tree_node_id=*/std::nullopt);
  }

  navigation_visible_to_embedder_ = true;
#if BUILDFLAG(IS_ANDROID)
  // Once the navigation has started, fill in the details in the Java side
  // navigation handle.
  navigation_handle_proxy_->DidStart();
#endif

  if (IsInMainFrame()) {
    DCHECK(!common_params_->navigation_start.is_null());
    DCHECK(!blink::IsRendererDebugURL(common_params_->url));
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "navigation", "Navigation StartToCommit",
        TRACE_ID_WITH_SCOPE("StartToCommit", TRACE_ID_LOCAL(this)),
        common_params_->navigation_start, "Initial URL",
        common_params_->url.spec());
  }

  if (IsSameDocument()) {
    EnterChildTraceEvent("Same document", this);
  }

  {
#if DCHECK_IS_ON()
    DCHECK(is_safe_to_delete_);
    base::AutoReset<bool> resetter(&is_safe_to_delete_, false);
#endif
    base::AutoReset<bool> resetter2(&ua_change_requires_reload_, false);
    GetDelegate()->DidStartNavigation(this);
  }
}

void NavigationRequest::ResetForCrossDocumentRestart() {
  DCHECK(IsSameDocument());

  // TODO(crbug.com/40055210): A same document history navigation was performed
  // but the renderer thinks there's a different document loaded. Where did
  // this navigation come from?
  if (common_params_->navigation_type ==
      blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT) {
    CaptureTraceForNavigationDebugScenario(
        DebugScenario::kDebugSameDocNavigationDocIdMismatch);
  }

  // Reset the NavigationHandle, which is now incorrectly marked as
  // same-document. Ensure |loader_| does not exist as it can hold raw pointers
  // to objects owned by the handle (see the comment in the header).
  DCHECK(!loader_);

#if BUILDFLAG(IS_ANDROID)
  if (navigation_visible_to_embedder_)
    navigation_handle_proxy_->DidFinish();
#endif

  // It is necessary to call DidFinishNavigation before resetting
  // |navigation_handle_proxy_|. See https://crbug.com/958396.
  if (IsNavigationStarted()) {
    GetDelegate()->DidFinishNavigation(this);
    if (IsInMainFrame()) {
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          "navigation", "Navigation StartToCommit",
          TRACE_ID_WITH_SCOPE("StartToCommit", TRACE_ID_LOCAL(this)), "URL",
          common_params_->url.spec(), "Net Error Code", net_error_);
    }
  }

  // Reset the state of the NavigationRequest, and the navigation_handle_id.
  StopCommitTimeout();
  SetState(NOT_STARTED);
  is_navigation_started_ = false;
  processing_navigation_throttle_ = false;

  navigation_visible_to_embedder_ = false;
#if BUILDFLAG(IS_ANDROID)
  if (navigation_visible_to_embedder_) {
    navigation_handle_proxy_.reset();
    navigation_handle_proxy_ = std::make_unique<NavigationHandleProxy>(this);
  }
#endif

  // Reset the previously selected RenderFrameHost. This is expected to be null
  // at the beginning of a new navigation. See https://crbug.com/936962.
  DCHECK(HasRenderFrameHost());
  render_frame_host_ = std::nullopt;

  // Convert the navigation type to the appropriate cross-document one.
  common_params_->navigation_type =
      ConvertToCrossDocumentType(common_params_->navigation_type);

  // Reset navigation handle timings.
  navigation_handle_timing_ = NavigationHandleTiming();

  policy_container_builder_->ResetForCrossDocumentRestart();
  commit_params_->soft_navigation_heuristics_task_id = std::nullopt;

  CheckSoftNavigationHeuristicsInvariants();
}

void NavigationRequest::ResetStateForSiteInstanceChange() {
  // This method should only be called when there is a dest_site_instance.
  DCHECK(dest_site_instance_);

  // When a request has a destination SiteInstance (e.g., reload or session
  // history navigation) but it changes during the navigation (e.g., due to
  // redirect or error page), it's important not to remember privileges or
  // attacker-controlled state from the original entry.

  // Reset bindings (e.g., since error pages for WebUI URLs don't get them).
  bindings_.reset();

  // Reset any existing PageState with a non-empty, clean PageState, so that old
  // attacker-controlled state is not pulled into the new process.
  blink::PageState page_state =
      blink::PageState::CreateFromEncodedData(commit_params_->page_state);
  if (page_state.IsValid())
    commit_params_->page_state =
        blink::PageState::CreateFromURL(GetURL()).ToEncodedData();

  // ISNs and DSNs are process-specific.
  frame_entry_item_sequence_number_ = -1;
  frame_entry_document_sequence_number_ = -1;
}

void NavigationRequest::RegisterSubresourceOverride(
    blink::mojom::TransferrableURLLoaderPtr transferrable_loader) {
  if (!transferrable_loader)
    return;
  if (!subresource_overrides_)
    subresource_overrides_.emplace();

  subresource_overrides_->push_back(std::move(transferrable_loader));
}

mojom::NavigationClient* NavigationRequest::GetCommitNavigationClient() {
  if (commit_navigation_client_ && commit_navigation_client_.is_bound())
    return commit_navigation_client_.get();

  // Instantiate a new NavigationClient interface.
  commit_navigation_client_ =
      GetRenderFrameHost()->GetNavigationClientFromInterfaceProvider();
  HandleInterfaceDisconnection(commit_navigation_client_);
  return commit_navigation_client_.get();
}

void NavigationRequest::SetRequiredCSP(
    network::mojom::ContentSecurityPolicyPtr csp) {
  DCHECK(!required_csp_);
  required_csp_ = std::move(csp);
  if (required_csp_)
    SetRequestHeader("Sec-Required-CSP", required_csp_->header->header_value);
}

network::mojom::ContentSecurityPolicyPtr NavigationRequest::TakeRequiredCSP() {
  return std::move(required_csp_);
}

const PolicyContainerPolicies*
NavigationRequest::GetInitiatorPolicyContainerPolicies() const {
  return policy_container_builder_->InitiatorPolicies();
}

const blink::DocumentToken& NavigationRequest::GetDocumentToken() const {
  DCHECK(!IsSameDocument());
  DCHECK_GE(state_, READY_TO_COMMIT);

  return *document_token_;
}

const PolicyContainerPolicies& NavigationRequest::GetPolicyContainerPolicies()
    const {
  DCHECK_GE(state_, READY_TO_COMMIT);

  return policy_container_builder_->FinalPolicies();
}

blink::mojom::PolicyContainerPtr
NavigationRequest::CreatePolicyContainerForBlink() {
  DCHECK_GE(state_, READY_TO_COMMIT);

  return policy_container_builder_->CreatePolicyContainerForBlink();
}
scoped_refptr<PolicyContainerHost> NavigationRequest::GetPolicyContainerHost() {
  DCHECK_GE(state_, READY_TO_COMMIT);
  // It is invalid calling this method after `TakePolicyContainerHost()`.
  CHECK(policy_container_builder_);
  return policy_container_builder_->GetPolicyContainerHost();
}

scoped_refptr<PolicyContainerHost>
NavigationRequest::TakePolicyContainerHost() {
  DCHECK_GE(state_, READY_TO_COMMIT);

  // Move the host out of the data member, then reset the member. This ensures
  // we do not use the helper after we moved its contents.
  scoped_refptr<PolicyContainerHost> host =
      std::move(*policy_container_builder_).TakePolicyContainerHost();
  policy_container_builder_ = std::nullopt;

  return host;
}

void NavigationRequest::CreateCoepReporter(
    StoragePartition* storage_partition) {
  DCHECK(!isolation_info_for_subresources_.IsEmpty());

  const PolicyContainerPolicies& policies =
      policy_container_builder_->FinalPolicies();
  coep_reporter_ = std::make_unique<CrossOriginEmbedderPolicyReporter>(
      static_cast<StoragePartitionImpl*>(storage_partition)->GetWeakPtr(),
      common_params_->url,
      policies.cross_origin_embedder_policy.reporting_endpoint,
      policies.cross_origin_embedder_policy.report_only_reporting_endpoint,
      GetRenderFrameHost()->GetFrameToken().value(),
      isolation_info_for_subresources_.network_anonymization_key());
}

std::unique_ptr<CrossOriginEmbedderPolicyReporter>
NavigationRequest::TakeCoepReporter() {
  return std::move(coep_reporter_);
}

ukm::SourceId NavigationRequest::GetPreviousPageUkmSourceId() {
  return previous_page_ukm_source_id_;
}

void NavigationRequest::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    network::mojom::URLResponseHeadPtr response_head) {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::OnRequestRedirected",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  ScopedCrashKeys crash_keys(*this);

  // Sanity check - this can only be set at commit time.
  DCHECK(!auth_challenge_info_);

  DCHECK(response_head);
  DCHECK(response_head->parsed_headers);
  response_head_ = std::move(response_head);
  ssl_info_ = response_head_->ssl_info;

  // Reset the page state as it can no longer be used at commit time since the
  // navigation was redirected.
  commit_params_->page_state = std::string();

  // Reset NotRestoredReasons as the reasons are for the original page and not
  // for the redirected one.
  commit_params_->not_restored_reasons = nullptr;

  // Reset the tentative origin_to_commit, as the redirected one is different.
  tentative_data_origin_to_commit_ = std::nullopt;

  // A request was made. Record it before we decide to block this response for
  // a reason or another.
  RecordAddressSpaceFeature();

#if BUILDFLAG(IS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());

  bool should_override_url_loading = false;
  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(),
          commit_params_->is_browser_initiated, redirect_info.new_url,
          redirect_info.new_method,
          // Redirects are always not counted as from user gesture.
          false, true, frame_tree_node_->IsOutermostMainFrame(),
          frame_tree_node_->frame_tree().is_prerendering(),
          ui::PageTransitionFromInt(common_params_->transition),
          &should_override_url_loading)) {
    // A Java exception was thrown by the embedding application; we
    // need to return from this task. Specifically, it's not safe from
    // this point on to make any JNI calls.
    return;
  }

  // The content/ embedder might cause |this| to be deleted while
  // |ShouldOverrideUrlLoading| is called.
  // See https://crbug.com/770157.
  if (!this_ptr)
    return;

  if (should_override_url_loading) {
    net_error_ = net::ERR_ABORTED;
    common_params_->url = redirect_info.new_url;
    common_params_->method = redirect_info.new_method;
    // Update the navigation handle to point to the new url to ensure
    // AwWebContents sees the new URL and thus passes that URL to onPageFinished
    // (rather than passing the old URL).
    UpdateStateFollowingRedirect(GURL(redirect_info.new_referrer));
    frame_tree_node_->ResetNavigationRequest(
        NavigationDiscardReason::kInternalCancellation);
    return;
  }
#endif
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanRedirectToURL(
          redirect_info.new_url)) {
    DVLOG(1) << "Denied redirect for "
             << redirect_info.new_url.possibly_invalid_spec();
    // TODO(arthursonzogni): Redirect to a javascript URL should display an
    // error page with the net::ERR_UNSAFE_REDIRECT error code. Instead, the
    // browser simply ignores the navigation, because some extensions use this
    // edge case to silently cancel navigations. See https://crbug.com/941653.
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // For renderer-initiated navigations we need to check if the source has
  // access to the URL. Browser-initiated navigations only rely on the
  // |CanRedirectToURL| test above.
  if (!commit_params_->is_browser_initiated && GetSourceSiteInstance() &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
          GetSourceSiteInstance()->GetProcess()->GetID(),
          redirect_info.new_url)) {
    DVLOG(1) << "Denied unauthorized redirect for "
             << redirect_info.new_url.possibly_invalid_spec();
    // TODO(arthursonzogni): This case uses ERR_ABORTED to be consistent with
    // the javascript URL redirect case above, though ideally it would use
    // net::ERR_UNSAFE_REDIRECT and an error page. See https://crbug.com/941653.
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  const std::optional<network::mojom::BlockedByResponseReason>
      coop_requires_blocking =
          coop_status_.SanitizeResponse(response_head_.get());
  if (coop_requires_blocking) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(*coop_requires_blocking),
        false /* skip_throttles */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }
  const url::Origin origin = GetOriginForURLLoaderFactoryUnchecked();
  // Set the COOP origin in the policy container builder via the mutable
  // reference before coop is sent to EnforceCOOP.
  network::CrossOriginOpenerPolicy& coop =
      response()->parsed_headers->cross_origin_opener_policy;
  coop.origin = origin;
  coop_status_.EnforceCOOP(coop, origin, network_anonymization_key);

  const std::optional<network::mojom::BlockedByResponseReason>
      coep_requires_blocking = EnforceCOEP();
  if (coep_requires_blocking) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(*coep_requires_blocking),
        false /* skip_throttles */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  // For now, DevTools needs the POST data sent to the renderer process even if
  // it is no longer a POST after the redirect.
  if (redirect_info.new_method != "POST")
    common_params_->post_data.reset();

  const bool is_first_response = commit_params_->redirects.empty();
  UpdateNavigationHandleTimingsOnResponseReceived(/*is_redirect=*/true,
                                                  is_first_response);

  // Mark time for the Navigation Timing API.
  if (commit_params_->navigation_timing->redirect_start.is_null()) {
    commit_params_->navigation_timing->redirect_start =
        commit_params_->navigation_timing->fetch_start;
  }
  commit_params_->navigation_timing->redirect_end = base::TimeTicks::Now();
  commit_params_->navigation_timing->fetch_start = base::TimeTicks::Now();

  commit_params_->redirect_response.push_back(response_head_.Clone());
  commit_params_->redirect_infos.push_back(redirect_info);

  // TODO(rakina): This should use `GetTentativeOriginAtRequestTime()` instead
  // of the url from `common_params_`.
  const bool is_same_origin_redirect =
      url::Origin::Create(common_params_->url)
          .IsSameOriginWith(redirect_info.new_url);

  did_encounter_cross_origin_redirect_ |= !is_same_origin_redirect;

  // Only same-origin navigations without cross-origin redirects can
  // expose response details (status-code / mime-type).
  // https://github.com/whatwg/fetch/issues/1602
  if (!is_same_origin_redirect &&
      commit_params_->navigation_timing->parent_resource_timing_access ==
          blink::mojom::ParentResourceTimingAccess::
              kReportWithResponseDetails) {
    commit_params_->navigation_timing->parent_resource_timing_access =
        blink::mojom::ParentResourceTimingAccess::kReportWithoutResponseDetails;
  }

  did_receive_early_hints_before_cross_origin_redirect_ |=
      did_create_early_hints_manager_params_ && !is_same_origin_redirect;

  commit_params_->redirects.push_back(common_params_->url);
  common_params_->url = redirect_info.new_url;
  common_params_->method = redirect_info.new_method;
  common_params_->referrer->url = GURL(redirect_info.new_referrer);
  common_params_->referrer = Referrer::SanitizeForRequest(
      common_params_->url, *common_params_->referrer);

  // On redirects, the initial referrer is no longer correct, so it must
  // be updated.  (A parallel process updates the outgoing referrer in the
  // network stack.)
  commit_params_->redirect_infos.back().new_referrer =
      common_params_->referrer->url.spec();

  // When the redirection happens, the cookie_change_listener_ should be
  // re-initialized if needed.
  if (ShouldAddCookieChangeListener()) {
    cookie_change_listener_ =
        std::make_unique<RenderFrameHostImpl::CookieChangeListener>(
            GetStoragePartitionWithCurrentSiteInfo(), common_params_->url);
  } else {
    cookie_change_listener_.reset();
  }

  // Check Content Security Policy before the NavigationThrottles run. This
  // gives CSP a chance to modify requests that NavigationThrottles would
  // otherwise block.
  net::Error net_error =
      CheckContentSecurityPolicy(true /* has_followed_redirect */,
                                 redirect_info.insecure_scheme_was_upgraded,
                                 false /* is_response_check */);
  if (net_error != net::OK) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net_error), false /*skip_throttles*/,
        std::nullopt /*error_page_content*/, false /*collapse_frame*/);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
      CredentialedSubresourceCheckResult::BLOCK_REQUEST) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, std::nullopt /*error_page_content*/,
        false /*collapse_frame*/);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Compute the SiteInstance to use for the redirect and pass its
  // RenderProcessHost if it has a process. Keep a reference if it has a
  // process, so that the SiteInstance and its associated process aren't deleted
  // before the navigation is ready to commit.
  scoped_refptr<SiteInstance> site_instance =
      frame_tree_node_->render_manager()->GetSiteInstanceForNavigationRequest(
          this, &browsing_context_group_swap_);
  speculative_site_instance_ =
      site_instance->HasProcess() ? site_instance : nullptr;

  // If the new site instance doesn't yet have a process, then tell the
  // SpareRenderProcessHostManager so it can decide whether to start warming up
  // the spare at this time (note that the actual behavior depends on
  // RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes).
  if (!site_instance->HasProcess()) {
    RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedSiteInstance(
        site_instance.get());
  }

  // Check what the process of the SiteInstance is. It will be passed to the
  // NavigationHandle, and informed to expect a navigation to the redirected
  // URL.
  // Note: calling GetProcess on the SiteInstance can lead to the creation of a
  // new process if it doesn't have one. In this case, it should only be called
  // on a SiteInstance that already has a process.
  RenderProcessHost* expected_process =
      site_instance->HasProcess() ? site_instance->GetProcess() : nullptr;

  WillRedirectRequest(common_params_->referrer->url, expected_process);
}

base::WeakPtr<NavigationRequest> NavigationRequest::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::SafeRef<NavigationHandle> NavigationRequest::GetSafeRef() {
  return weak_factory_.GetSafeRef();
}

bool NavigationRequest::ExistingDocumentWasDiscarded() const {
  return commit_params_->was_discarded;
}

void NavigationRequest::SetContentSettings(
    blink::mojom::RendererContentSettingsPtr content_settings) {
  commit_params_->content_settings = std::move(content_settings);
}

blink::mojom::RendererContentSettingsPtr
NavigationRequest::GetContentSettingsForTesting() {
  return commit_params_->content_settings->Clone();
}

void NavigationRequest::SetIsAdTagged() {
  is_ad_tagged_ = true;
}

void NavigationRequest::CheckForIsolationOptIn(const GURL& url) {
  // Check whether an origin-keyed agent cluster is explicitly requested, either
  // opting in or out, before attempting to isolate it. If an explicit request
  // was made, then we must check if the origin has been previously
  // encountered in order to remain consistent within the isolation context
  // (BrowserContext). Note: we only do the global walk for explicit opt-outs
  // when OriginAgentCluster-by-default is enabled, but that check is made in
  // IsOriginAgentClusterOptOutRequested().
  if (!IsOriginAgentClusterOptInRequested() &&
      !IsOriginAgentClusterOptOutRequested())
    return;

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin origin = url::Origin::Create(url);
  auto* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  if (policy->UpdateOriginIsolationOptInListIfNecessary(browser_context,
                                                        origin)) {
    // This is a new request for isolating |origin|, either by explicitly opting
    // it in or out. Do a global walk of session history to find any existing
    // instances of |origin|, so that those existing BrowsingInstances can give
    // it default isolation. Only new BrowsingInstances and ones that have not
    // seen |origin| before will honor the request. We don't always have a value
    // for render_frame_host_ at this point, so we map the global-walk call onto
    // NavigatorDelegate to get it into WebContents. We definitely need to do
    // the global walk prior to deciding on the render_frame_host_ to commit to.
    // We must exclude ourselves from the global walk otherwise we may mark our
    // origin as having default isolation before it gets the change to register
    // itself as opted-in/out.
    frame_tree_node_->navigator()
        .GetDelegate()
        ->RegisterExistingOriginAsHavingDefaultIsolation(
            origin, this /* navigation_request_to_exclude */);
  }
}

void NavigationRequest::AddOriginAgentClusterStateIfNecessary(
    const IsolationContext& isolation_context) {
  // Normally for explicit opt-ins the origin is tracked when we create the
  // SiteInstance, but there are two cases where that fails. (1) If process-
  // isolation for OAC is not enabled we need to track opt-in here (used for
  // origin-agent-cluster-by-default), and (2) if origin-keyed processes by
  // default is enabled, then it's possible we got here due to using a
  // speculative RenderFrameHost. In this latter case, the opt-in header had not
  // arrived when the SiteInstance was created, so the origin was not tracked
  // earlier.
  bool is_opt_in_requested = IsOriginAgentClusterOptInRequested();
  bool explicitly_requests_origin_keyed_process =
      is_opt_in_requested &&
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled();

  // Since opt-outs are asking not to have OAC or requires_origin_keyed_process,
  // they don't get their own SiteInstance, and so we must register their
  // opt-out here.
  bool is_opt_out_requested = IsOriginAgentClusterOptOutRequested();

  // We never register isolation state here unless it's explicitly requested.
  if (!is_opt_in_requested && !is_opt_out_requested)
    return;

  bool should_isolate_origin = is_opt_in_requested;

  // Note: we don't handle IsIsolationImplied() cases here, since those only
  // occur when OAC-by-default is enabled, and in that case we only pro-actively
  // record explicit opt-ins and opt-outs. Implicitly isolated origins only end
  // up recorded if a future request from the same origin attempts to opt-in or
  // opt-out, which would trigger a normal global walk and record that the
  // origin has already been implicitly isolated in some BrowsingInstances.

  // TODO(crbug.com/40910871): investigate using one of NavigationRequest's
  // Get*Origin*() functions to compute this, instead of assuming we can just
  // convert directly from GetURL().
  url::Origin origin = url::Origin::Create(GetURL());
  // Since this origin is using a site-keyed process (either because
  // origin-keyed processes are disabled, not used for this origin, or the
  // origin has opted out), we can't rely on a newly created SiteInstance to add
  // the origin as OAC/not-OAC, so we do it manually here.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  // If there is already a state registered for `origin` in `isolation_context`,
  // then the following call does nothing.
  policy->AddOriginIsolationStateForBrowsingInstance(
      isolation_context, origin,
      should_isolate_origin /* is_origin_agent_cluster */,
      explicitly_requests_origin_keyed_process);
}

bool NavigationRequest::IsOriginAgentClusterOptInRequested() {
  // We explicitly do not honor Origin-Agent-Cluster headers in redirects and
  // may only consider them in final responses, according to spec.
  // https://crbug.com/1329061
  if (state_ < WILL_PROCESS_RESPONSE || state_ == WILL_FAIL_REQUEST) {
    return false;
  }

  if (!response()) {
    return false;
  }

  // Do not attempt isolation if the feature is not enabled.
  if (!SiteIsolationPolicy::IsOriginAgentClusterEnabled()) {
    return false;
  }

  return response_head_->parsed_headers->origin_agent_cluster ==
         network::mojom::OriginAgentClusterValue::kTrue;
}

bool NavigationRequest::IsOriginAgentClusterOptOutRequested() {
  // We explicitly do not honor Origin-Agent-Cluster headers in redirects and
  // may only consider them in final responses, according to spec.
  // https://crbug.com/1329061
  if (state_ < WILL_PROCESS_RESPONSE || state_ == WILL_FAIL_REQUEST) {
    return false;
  }

  if (!response()) {
    return false;
  }

  // We only allow explicit opt-outs when OAC-by-default is enabled. The
  // following check will be false if IsOriginAgentClusterEnabled() is false.
  if (!SiteIsolationPolicy::AreOriginAgentClustersEnabledByDefault(
          frame_tree_node_->navigator().controller().GetBrowserContext())) {
    return false;
  }

  return response_head_->parsed_headers->origin_agent_cluster ==
         network::mojom::OriginAgentClusterValue::kFalse;
}

bool NavigationRequest::IsIsolationImplied() {
  if (!SiteIsolationPolicy::AreOriginAgentClustersEnabledByDefault(
          frame_tree_node_->navigator().controller().GetBrowserContext())) {
    return false;
  }

  return !response() || response_head_->parsed_headers->origin_agent_cluster ==
                            network::mojom::OriginAgentClusterValue::kAbsent;
}

void NavigationRequest::DetermineOriginAgentClusterEndResult() {
  DCHECK(state_ == WILL_PROCESS_RESPONSE ||
         state_ == WILL_COMMIT_WITHOUT_URL_LOADER ||
         state_ == WILL_FAIL_REQUEST || state_ == CANCELING);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin origin = GetOriginToCommit().value();
  const IsolationContext& isolation_context =
      GetRenderFrameHost()->GetSiteInstance()->GetIsolationContext();

  bool is_requested = IsOriginAgentClusterOptInRequested();
  bool expects_origin_agent_cluster = is_requested || IsIsolationImplied();
  bool is_origin_keyed_process_implied =
      IsIsolationImplied() &&
      SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault();
  bool requires_origin_keyed_process =
      (is_requested || is_origin_keyed_process_implied) &&
      SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled();

  OriginAgentClusterIsolationState requested_isolation_state =
      expects_origin_agent_cluster
          ? OriginAgentClusterIsolationState::CreateForOriginAgentCluster(
                requires_origin_keyed_process)
          : OriginAgentClusterIsolationState::CreateNonIsolated();

  const bool got_origin_agent_cluster =
      policy
          ->DetermineOriginAgentClusterIsolation(isolation_context, origin,
                                                 requested_isolation_state)
          .is_origin_agent_cluster();

  if (SiteIsolationPolicy::AreOriginAgentClustersEnabledByDefault(
          frame_tree_node_->navigator().controller().GetBrowserContext())) {
    // When OAC is enabled by default, report enum values that distinguish
    // between explicitly requesting OAC (on or off) and having no related
    // header.
    bool was_explicitly_requested =
        response_head_ &&
        response_head_->parsed_headers->origin_agent_cluster ==
            network::mojom::OriginAgentClusterValue::kTrue;
    bool was_explicitly_not_requested =
        response_head_ &&
        response_head_->parsed_headers->origin_agent_cluster ==
            network::mojom::OriginAgentClusterValue::kFalse;

    if (got_origin_agent_cluster) {
      if (was_explicitly_requested) {
        origin_agent_cluster_end_result_ =
            OriginAgentClusterEndResult::kExplicitlyRequestedAndOriginKeyed;
      } else if (was_explicitly_not_requested) {
        origin_agent_cluster_end_result_ =
            OriginAgentClusterEndResult::kExplicitlyNotRequestedButOriginKeyed;
      } else {
        origin_agent_cluster_end_result_ =
            OriginAgentClusterEndResult::kNotExplicitlyRequestedAndOriginKeyed;
      }
    } else {
      if (was_explicitly_requested) {
        origin_agent_cluster_end_result_ =
            OriginAgentClusterEndResult::kExplicitlyRequestedButNotOriginKeyed;
      } else if (was_explicitly_not_requested) {
        origin_agent_cluster_end_result_ = OriginAgentClusterEndResult::
            kExplicitlyNotRequestedAndNotOriginKeyed;
      } else {
        origin_agent_cluster_end_result_ = OriginAgentClusterEndResult::
            kNotExplicitlyRequestedButNotOriginKeyed;
      }
    }
  } else {
    // When OAC is not enabled by default, report enum values that only indicate
    // if OAC was requested or not vs whether it took effect.
    if (is_requested) {
      origin_agent_cluster_end_result_ =
          got_origin_agent_cluster
              ? OriginAgentClusterEndResult::kRequestedAndOriginKeyed
              : OriginAgentClusterEndResult::kRequestedButNotOriginKeyed;
    } else {
      origin_agent_cluster_end_result_ =
          got_origin_agent_cluster
              ? OriginAgentClusterEndResult::kNotRequestedButOriginKeyed
              : OriginAgentClusterEndResult::kNotRequestedAndNotOriginKeyed;
    }
  }

  // This needs to be computed separately from origin.opaque() because, per
  // https://crbug.com/1041376, we don't have a notion of the true origin yet.
  const bool is_opaque_origin_because_sandbox =
      (policy_container_builder_->FinalPolicies().sandbox_flags &
       network::mojom::WebSandboxFlags::kOrigin) ==
      network::mojom::WebSandboxFlags::kOrigin;

  // The origin_agent_cluster navigation commit parameter communicates to the
  // renderer about origin-keying, so it should be true for opaque origin
  // cases (e.g., for data: URLs). origin_agent_cluster_end_result_ shouldn't be
  // modified since it's used for warnings and use counters, i.e. things that
  // don't apply to this sort of "automatic" origin-keying.
  commit_params_->origin_agent_cluster = is_opaque_origin_because_sandbox ||
                                         origin.opaque() ||
                                         got_origin_agent_cluster;

  // The origin_agent_cluster_left_as_default navigation commit parameter
  // communicates to the renderer whether the origin_agent_cluster decision
  // (recorded just above) has been made based on an absent Origin-Agent-Cluster
  // http header.
  commit_params_->origin_agent_cluster_left_as_default =
      !response_head_ || response_head_->parsed_headers->origin_agent_cluster ==
                             network::mojom::OriginAgentClusterValue::kAbsent;
}

void NavigationRequest::ProcessOriginAgentClusterEndResult() {
  if (!HasCommitted() || IsErrorPage() || IsSameDocument())
    return;

  if (origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kRequestedAndOriginKeyed ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kRequestedButNotOriginKeyed ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kExplicitlyRequestedAndOriginKeyed ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kExplicitlyRequestedButNotOriginKeyed) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        GetRenderFrameHost(),
        blink::mojom::WebFeature::kOriginAgentClusterHeader);
  }

  const url::Origin origin = url::Origin::Create(GetURL());

  if (origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kRequestedButNotOriginKeyed ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kExplicitlyRequestedButNotOriginKeyed) {
    GetRenderFrameHost()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "The page requested an origin-keyed agent cluster using the "
            "Origin-Agent-Cluster header, but could not be origin-keyed since "
            "the origin '%s' had previously been placed in a site-keyed agent "
            "cluster. Update your headers to uniformly request origin-keying "
            "for all pages on the origin.",
            origin.Serialize().c_str()));
  }

  if (origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kNotRequestedButOriginKeyed ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kExplicitlyNotRequestedButOriginKeyed) {
    GetRenderFrameHost()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "The page did not request an origin-keyed agent cluster, but was "
            "put in one anyway because the origin '%s' had previously been "
            "placed in an origin-keyed agent cluster. Update your headers to "
            "uniformly request origin-keying for all pages on the origin.",
            origin.Serialize().c_str()));
  }
}

void NavigationRequest::PopulateDocumentTokenForCrossDocumentNavigation() {
  DCHECK(!IsSameDocument());
  DCHECK_GE(state_, READY_TO_COMMIT);
  const auto* token_to_reuse =
      GetRenderFrameHost()->GetDocumentTokenForCrossDocumentNavigationReuse(
          /* passkey */ {});
  document_token_.emplace(token_to_reuse ? *token_to_reuse
                                         : blink::DocumentToken());
}

bool NavigationRequest::HasCommittingOrigin(const url::Origin& origin) {
  // We are only interested in checking requests that have been assigned a
  // SiteInstance.
  if (state() < WILL_PROCESS_RESPONSE)
    return false;

  // This origin conversion won't be correct for about:blank, but origin
  // isolation shouldn't need to care about that case because a previous
  // instance of the origin would already have determined its isolation status
  // in that BrowsingInstance.
  // TODO(crbug.com/40092527): Use the computed origin here just to be
  // safe.
  return origin == url::Origin::Create(GetURL());
}

bool NavigationRequest::ShouldRequestSiteIsolationForCOOP() {
  if (!SiteIsolationPolicy::IsSiteIsolationForCOOPEnabled())
    return false;

  // COOP headers are only served once a response is available.
  if (state_ < WILL_PROCESS_RESPONSE)
    return false;

  // COOP isolation can only be triggered from main frames.  COOP headers
  // aren't honored in subframes.
  if (!IsInMainFrame())
    return false;

  // Filter out URLs with origins that are considered invalid for being
  // isolated. Note that the origin we'll eventually attempt to isolate should
  // be based on process_lock_url(), so that we apply isolation to the actual
  // site rather than the effective URL in the case of hosted apps.
  url::Origin origin(url::Origin::Create(site_info_.process_lock_url()));
  if (!IsolatedOriginUtil::IsValidIsolatedOrigin(origin))
    return false;

  // Check the COOP header value. All same-origin values are considered to be
  // an implicit hint for site isolation.
  bool should_header_value_trigger_isolation = false;
  switch (coop_status_.current_coop().value) {
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
    case network::mojom::CrossOriginOpenerPolicyValue::kRestrictProperties:
    case network::mojom::CrossOriginOpenerPolicyValue::
        kRestrictPropertiesPlusCoep:
    case network::mojom::CrossOriginOpenerPolicyValue::kNoopenerAllowPopups:
      should_header_value_trigger_isolation = true;
      break;
    case network::mojom::CrossOriginOpenerPolicyValue::kUnsafeNone:
      should_header_value_trigger_isolation = false;
      break;
      // Don't handle the default case on purpose to force a compiler error if
      // new COOP values are added, so that they are explicitly handled here.
  }
  if (!should_header_value_trigger_isolation)
    return false;

  // There's no need for additional isolation if the site already requires a
  // dedicated process via other isolation mechanisms.  However, we still
  // return true if the site has been isolated due to COOP previously, so that
  // we can go through the COOP isolation flow to update the timestamp of when
  // the COOP isolation for this site was last used.
  //
  // Note: we can use `site_info_` here, since that has been assigned at
  // request start time and updated by redirects, but it is not (currently)
  // recomputed when response is received, so it does not include the COOP
  // isolation request (which would cause RequiresDedicatedProcess to return
  // true regardless of prior isolation). If we ever decide to update
  // `site_info_` at response time, we should revisit this and ensure that we
  // call RequiresDedicatedProcess on a SiteInfo that does not already have an
  // isolation request (enforced by DCHECK below).
  DCHECK(!site_info_.does_site_request_dedicated_process_for_coop());
  if (site_info_.RequiresDedicatedProcess(
          GetStartingSiteInstance()->GetIsolationContext())) {
    bool is_already_isolated_due_to_coop =
        ChildProcessSecurityPolicyImpl::GetInstance()->IsIsolatedSiteFromSource(
            origin,
            ChildProcessSecurityPolicy::IsolatedOriginSource::WEB_TRIGGERED);
    return is_already_isolated_due_to_coop;
  }
  return true;
}

UrlInfo NavigationRequest::GetUrlInfo() {
  // Compute the isolation request flags.  Note that multiple requests could be
  // active simultaneously for the same navigation.
  // We start by assuming that the default isolation will be used, and only
  // change it if an explicit opt-in or opt-out request is seen. Depending on
  // the value of OriginAgentClusterIsolationState::CreateForDefaultIsolation,
  // default isolation could potentially be non-isolated, origin-agent-cluster,
  // or origin-agent-cluster in an origin-keyed process. Note: the
  // IsOriginIsolationImplied() case is handled via kDefault. It is the only
  // case where the `Origin-Agent-Cluster` header is absent.
  uint32_t isolation_flags = UrlInfo::OriginIsolationRequest::kDefault;

  if (IsOriginAgentClusterOptOutRequested()) {
    isolation_flags = UrlInfo::OriginIsolationRequest::kNone;
  } else if (IsOriginAgentClusterOptInRequested()) {
    // An origin-keyed agent cluster is used if explicitly requested by header.
    isolation_flags =
        UrlInfo::OriginIsolationRequest::kOriginAgentClusterByHeader;
    if (SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled()) {
      // An origin-keyed process is used if requested by header.
      isolation_flags |=
          UrlInfo::OriginIsolationRequest::kRequiresOriginKeyedProcessByHeader;
    }
  }

  // Compute the CrossOriginIsolationKey for the navigation.
  std::optional<AgentClusterKey::CrossOriginIsolationKey>
      cross_origin_isolation_key = ComputeCrossOriginIsolationKey();

  auto isolation_request =
      static_cast<UrlInfo::OriginIsolationRequest>(isolation_flags);

  // Compute the WebExposedIsolationInfo that will be bundled into UrlInfo.
  auto web_exposed_isolation_info = ComputeWebExposedIsolationInfo();

  UrlInfoInit url_info_init(GetURL());
  url_info_init.WithOriginIsolationRequest(isolation_request)
      .WithCOOPSiteIsolation(ShouldRequestSiteIsolationForCOOP())
      .WithWebExposedIsolationInfo(web_exposed_isolation_info)
      .WithCrossOriginIsolationKey(cross_origin_isolation_key)
      .WithIsPdf(is_pdf_);

  // Records in the UrlInfo if COOP: same-origin or COOP: restrict-properties
  // was set, and from which origin.
  auto common_coop_origin = ComputeCommonCoopOrigin();
  if (common_coop_origin.has_value()) {
    url_info_init.WithCommonCoopOrigin(common_coop_origin.value());
  }

  // Navigations with SiteInstances which have fixed storage partition (e.g.
  // <webview> tags) should always stay in the current StoragePartition.
  SiteInstanceImpl* current_instance =
      frame_tree_node_->current_frame_host()->GetSiteInstance();
  if (current_instance->IsFixedStoragePartition()) {
    url_info_init.WithStoragePartitionConfig(
        current_instance->GetStoragePartitionConfig());
  }

  // Child frames (including fenced frames) should always use the
  // same StoragePartition as their parent.
  RenderFrameHostImpl* parent = GetParentFrameOrOuterDocument();
  if (parent) {
    url_info_init.WithStoragePartitionConfig(
        parent->GetSiteInstance()->GetStoragePartitionConfig());
  }

  if (IsLoadDataWithBaseURL()) {
    // LoadDataWithBaseURL() navigations also need to explicitly set the origin
    // to the origin of the base URL.  This ensures that the process for this
    // navigation will eventually be locked to the right origin (i.e., origin of
    // the base URL rather than the data: URL).
    //
    // Note that while LoadDataWithBaseURL() is supported in <webview> tags on
    // desktop platforms and on Android Webview, only <webview> tags currently
    // utilize this special case when running in site-isolated mode. Android
    // Webview doesn't currently lock processes for LoadDataWithBaseURL()
    // navigations.
    url_info_init.WithOrigin(
        url::Origin::Create(common_params().base_url_for_data_url));
  } else if (GetURL().IsAboutBlank() && GetInitiatorOrigin().has_value()) {
    // about:blank inherits its origin from the initiator, so ensure that this
    // is reflected in the UrlInfo.  In the common case, this isn't needed for
    // process model decisions, since we already leave about:blank in the
    // source SiteInstance, which corresponds to the initiator (see
    // `RenderFrameHostManager::CanUseSourceSiteInstance()`). However, in
    // certain corner cases, the source SiteInstance can't be used, but we
    // will still need to assign a proper process for about:blank. In that
    // case, we should honor the initiator origin, so that about:blank ends up
    // in a process that's locked to that origin, rather than an unlocked
    // process with an unassigned SiteInstance.  The latter would be violating
    // site isolation guarantees and would be problematic for Citadel
    // enforcements in
    // ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin().  See
    // https://crbug.com/1426928.
    //
    // TODO(alexmos): Consider also specifying UrlInfo::origin for about:srcdoc
    // navigations. This is not currently needed in the SiteInstance
    // and process assignment paths for srcdoc frames, but doing this might
    // simplify some of that code and would be good for consistency, since both
    // about:blank and about:srcdoc inherit the origin per spec
    // (https://html.spec.whatwg.org/multipage/document-sequences.html#determining-the-origin).
    url_info_init.WithOrigin(*GetInitiatorOrigin());
  } else {
    // Overriding the origin for a URL is dangerous and only allowed in very
    // narrow cases which are handled explicitly above.  Please think very
    // carefully about any new cases that need to do this.
    DCHECK(!url_info_init.origin().has_value());
  }

  // Propagate the tentative origin to commit value (for data: URLs that will be
  // rendered) to the UrlInfo, to make sure the nonce remains the same
  // throughout the navigation.
  if (GetURL().SchemeIs(url::kDataScheme)) {
    // The function for computing the request's origin depends on the stage of
    // the request, but the same opaque nonce value is preserved across both
    // functions for data: URLs.
    if (state_ < WILL_PROCESS_RESPONSE) {
      url_info_init.WithOrigin(GetTentativeOriginAtRequestTime());
    } else if (response_should_be_rendered_) {
      // The origin to commit is nullopt for cases that are not rendered (e.g.,
      // downloads), but the UrlInfo does not need the origin for data: URLs in
      // such cases.
      url_info_init.WithOrigin(GetOriginToCommit().value());
    }
  }

  // Determine if the request is for a sandboxed frame or not, and if so whether
  // the sandboxed frame should get a dedicated process. Setting
  // `has_origin_restricted_sandbox_flag` to true indicates it should get
  // process isolation, but only if the site/origin would have qualified for a
  // dedicated process even without the sandbox flags.
  //
  // If PolicyContainer::ComputePoliciesToCommit() has run
  // `policy_container_builder_` will be valid, but even if it hasn't, we can
  // speculatively take `commit_params_->frame_policy.sandbox_flags` if we
  // haven't received the response yet and don't have the final
  // `policy_container_builder_`, and if the state of the kOrigin flag changes,
  // we'll detect the change and recompute the target SiteInstance elsewhere.
  //
  // In general, about:blank documents should stay in their initiator's process.
  // If neither the initiator or about:blank is sandboxed, or if both are, then
  // the about:blank should stay in its parent's process, if only to avoid
  // needing an extra process that only shows the empty frame (if the parent and
  // about:blank are sandboxed, they parent cannot script the about:blank
  // frame). If the initiator is not sandboxed but the about:blank document is
  // (e.g., due to iframe attributes), then nothing else will be able to script
  // the empty about:blank document and it is safe to leave it in the same
  // process. (It is not possible for the initiator to be sandboxed and the
  // about:blank to not be sandboxed, because about:blank inherits the
  // sandboxing of its initiator.)
  bool is_eligible_for_sandboxing =
      !GetURL().IsAboutBlank() ||
      (source_site_instance_ &&
       source_site_instance_->GetSiteInfo().is_sandboxed());
  if (SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled() &&
      is_eligible_for_sandboxing) {
    // Determine if the frame has the sandbox flag or not.
    bool has_origin_restricted_sandbox_flag = false;
    if (policy_container_builder_->HasComputedPolicies()) {
      has_origin_restricted_sandbox_flag =
          (policy_container_builder_->FinalPolicies().sandbox_flags &
           network::mojom::WebSandboxFlags::kOrigin) ==
          network::mojom::WebSandboxFlags::kOrigin;
    } else {
      // Note: We'll end up here if this function is called before
      // ComputePoliciesToCommit(), such as when computing a speculative
      // RenderFrameHost's SiteInstance before receiving a response. In that
      // event we use the sandbox flags in commit_params_ as a current "best
      // estimate".
      has_origin_restricted_sandbox_flag =
          (commit_params_->frame_policy.sandbox_flags &
           network::mojom::WebSandboxFlags::kOrigin) ==
          network::mojom::WebSandboxFlags::kOrigin;
    }

    // It's possible that a sandbox attribute can disappear from a frame that
    // still contains a sandboxed initiator, meaning we won't have sandbox
    // flags here, but should still respect the sandbox of the initiator.
    bool should_inherit_initiators_sandbox =
        GetURL().IsAboutBlank() && source_site_instance_ &&
        source_site_instance_->GetSiteInfo().is_sandboxed();

    // Consider isolating sandboxed frames that won't end up as downloads or
    // 204s.
    if ((has_origin_restricted_sandbox_flag ||
         should_inherit_initiators_sandbox) &&
        response_should_be_rendered_) {
      // If the URL under consideration wouldn't qualify for a dedicated process
      // without the sandbox flags, then it shouldn't qualify even with the
      // sandbox flag. This is most likely to occur when site isolation is only
      // partial, as on Android.
      //
      // Ideally the IsolationContext would be the one used for the committed
      // RenderFrameHost at the end of the navigation, since a different set of
      // origins may require isolation if a BrowsingInstance swap occurs. This
      // isn't known at the start of the navigation, though, so we use the
      // current IsolationContext instead.
      const IsolationContext& isolation_context =
          current_instance->GetIsolationContext();
      if (SiteInfo::Create(isolation_context, UrlInfo(url_info_init))
              .RequiresDedicatedProcess(isolation_context)) {
        // Embedders can identify, via ContentBrowserClient, cases that should
        // not use isolated sandboxed frames.
        ContentBrowserClient* client = GetContentClient()->browser();
        BrowserContext* context =
            frame_tree_node_->navigator().controller().GetBrowserContext();
        url::SchemeHostPort precursor;
        if (state_ < WILL_PROCESS_RESPONSE) {
          precursor = GetTentativeOriginAtRequestTime()
                          .GetTupleOrPrecursorTupleIfOpaque();
        } else if (GetOriginToCommit()) {
          precursor = GetOriginToCommit()->GetTupleOrPrecursorTupleIfOpaque();
        } else {
          CHECK(false) << "No origin-to-commit for sandboxed url = "
                       << GetURL();
        }

        bool client_allows_cross_process_sandboxed_frames =
            client->ShouldAllowCrossProcessSandboxedFrameForPrecursor(
                context, precursor.GetURL(), GetURL());
        if (client_allows_cross_process_sandboxed_frames) {
          url_info_init.WithSandbox(true);
          // If an isolated sandbox is required, and the "per-document" grouping
          // mode has been specified with kIsolateSandboxedIframes, then we use
          // a unique document identifier, provided by `navigation_id_`, to
          // guarantee that each sandboxed iframe gets its own SiteInstance,
          // even if two or more such documents share a site/origin. Using
          // navigation_id_ means that each new NavigationRequest (and thus each
          // document) will get a different value.
          if (blink::features::kIsolateSandboxedIframesGroupingParam.Get() ==
              blink::features::IsolateSandboxedIframesGrouping::kPerDocument) {
            url_info_init.WithUniqueSandboxId(navigation_id_);
          }
        }
      }
    }
  }

  // If a prefetch might have been affected by cross-site state, the
  // relationship with other windows should be severed to make this more
  // difficult to use to leak cross-site state.
  // https://crbug.com/1439246
  if (base::FeatureList::IsEnabled(
          features::kPrefetchStateContaminationMitigation) &&
      response_head_ &&
      response_head_->is_prefetch_with_cross_site_contamination) {
    url_info_init.WithCrossSitePrefetchContamination(true);
  }

  return UrlInfo(url_info_init);
}

const GURL& NavigationRequest::GetOriginalRequestURL() {
  // If this is a loadData navigation we should return the URL used to commit,
  // even if the navigation went through redirects. This is to preserve the
  // previous behavior where we use the redirect chain from the renderer to get
  // the original request URL. When we commit a loadDataWithBaseURL, or a
  // loadDataAsStringWithBaseUrl navigation, the redirect chain in the renderer
  // used to only contain the commit URL.
  if (IsLoadDataWithBaseURL())
    return GetURL();

  // Otherwise, return the first URL in the redirect chain. If the navigation
  // is started by a client redirect, this will be the URL of the document that
  // started the redirect. Otherwise, this will be the first destination URL
  // of the navigation, before any server redirects.
  // TODO(crbug.com/40168423): Reconsider the behavior with client
  // redirects, as all script-initiated navigations are considered client
  // redirects, which means the client redirect might not always trigger
  // immediately (or at all, if the navigation depends on user interaction)
  // if we decide to do a reload with the original URL.
  DCHECK(!redirect_chain_.empty());
  return redirect_chain_[0];
}

void NavigationRequest::OnResponseStarted(
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    GlobalRequestID request_id,
    bool is_download,
    net::NetworkAnonymizationKey network_anonymization_key,
    SubresourceLoaderParams subresource_loader_params,
    EarlyHints early_hints) {
  receive_response_time_ = base::TimeTicks::Now();
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::OnResponseStarted",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  ScopedCrashKeys crash_keys(*this);

  // The |loader_|'s job is finished. It must not call the NavigationRequest
  // anymore from now.
  loader_.reset();
  if (is_download)
    RecordDownloadUseCountersPrePolicyCheck();
  is_download_ = is_download && download_policy().IsDownloadAllowed();
  if (is_download_)
    RecordDownloadUseCountersPostPolicyCheck();
  request_id_ = request_id;

  DCHECK(IsNavigationStarted());
  DCHECK(response_head);
  DCHECK(response_head->parsed_headers);
  EnterChildTraceEvent("OnResponseStarted", this);
  SetState(WILL_PROCESS_RESPONSE);
  response_head_ = std::move(response_head);
  response_body_ = std::move(response_body);
  ssl_info_ = response_head_->ssl_info;
  auth_challenge_info_ = response_head_->auth_challenge_info;

  // TODO(crbug.com/40218207): Store the whole EarlyHints struct instead
  // of duplicating all of its fields.
  was_resource_hints_received_ = early_hints.was_resource_hints_received;
  early_hints_manager_ = std::move(early_hints.manager);
  if (early_hints_manager_ &&
      early_hints_manager_->first_early_hints_receive_time()) {
    base::UmaHistogramTimes(
        "Navigation.EarlyHints.WillStartRequestToEarlyHintsTime",
        *early_hints_manager_->first_early_hints_receive_time() -
            will_start_request_time_);
    base::UmaHistogramTimes(
        "Navigation.EarlyHints.EarlyHintsToResponseStartTime",
        base::TimeTicks::Now() -
            *early_hints_manager_->first_early_hints_receive_time());
  }

  // A request was made. Record it before we decide to block this response for
  // a reason or another.
  RecordAddressSpaceFeature();

  const bool is_mhtml_archive = IsMhtmlMimeType(response_head_->mime_type);
  if (is_mhtml_archive)
    is_mhtml_or_subframe_ = true;

  if (CheckCSPEmbeddedEnforcement() ==
      CSPEmbeddedEnforcementResult::BLOCK_RESPONSE) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_CSP),
        true /* skip_throttles */, std::nullopt /* error_page_content*/,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // See https://github.com/whatwg/fetch/pull/1579
  if (!response_head_->timing_allow_passed) {
    commit_params_->navigation_timing->parent_resource_timing_access =
        blink::mojom::ParentResourceTimingAccess::kDoNotReport;
  }

  {
    const std::optional<network::mojom::BlockedByResponseReason>
        coop_requires_blocking =
            coop_status_.SanitizeResponse(response_head_.get());
    if (coop_requires_blocking) {
      // TODO(crbug.com/40166503): Investigate what must be done in case
      // of a download.
      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(*coop_requires_blocking),
          false /* skip_throttles */, std::nullopt /* error_page_content */,
          false /* collapse_frame */);
      // DO NOT ADD CODE after this. The previous call to
      // OnRequestFailedInternal has destroyed the NavigationRequest.
      return;
    }
    policy_container_builder_->SetCrossOriginOpenerPolicy(
        response_head_->parsed_headers->cross_origin_opener_policy);
  }

  ComputePoliciesToCommit();
  // After this line. The sandbox flags to commit have been computed. The origin
  // can be determined. This is needed for enforcing COOP below.

  {
    // Set the COOP origin in the policy container builder before
    // FinalPolicies() is called.
    const url::Origin origin = GetOriginForURLLoaderFactoryBeforeResponse(
        policy_container_builder_->FinalPolicies().sandbox_flags);
    policy_container_builder_->GetPolicyContainerHost()
        ->cross_origin_opener_policy()
        .origin = origin;
    coop_status_.EnforceCOOP(
        policy_container_builder_->FinalPolicies().cross_origin_opener_policy,
        origin, network_anonymization_key);
  }

  // The navigation may have encountered a header that requests isolation for
  // the url's origin. Before we pick the renderer, make sure we update the
  // origin-isolation opt-ins appropriately.
  CheckForIsolationOptIn(GetURL());

  // Check if the response should be sent to a renderer.
  // Regular downloads should not be rendered, but downloads with an
  // unsuccessful response code will cause an error page to be rendered.
  response_should_be_rendered_ =
      (!is_download ||
       IsFailedDownload(is_download, response_head_->headers.get())) &&
      (!response_head_->headers.get() ||
       (response_head_->headers->response_code() != net::HTTP_NO_CONTENT &&
        response_head_->headers->response_code() != net::HTTP_RESET_CONTENT &&
        !ShouldRenderFallbackContentForResponse(*response_head_->headers)));

  // Response that will not commit should be marked as aborted in the
  // NavigationHandle.
  if (!response_should_be_rendered_)
    net_error_ = net::ERR_ABORTED;

  const bool is_first_response = commit_params_->redirects.empty();
  UpdateNavigationHandleTimingsOnResponseReceived(/*is_redirect=*/false,
                                                  is_first_response);

  commit_params_->http_response_code =
      response_head_->headers ? response_head_->headers->response_code()
                              : -1 /* no http_response_code */;

  // Update fetch start timing. While NavigationRequest updates fetch start
  // timing for redirects, it's not aware of service worker interception so
  // fetch start timing could happen earlier than worker start timing. Use
  // worker ready time if it is greater than the current value to make sure
  // fetch start timing always comes after worker start timing (if a service
  // worker intercepted the navigation).
  commit_params_->navigation_timing->fetch_start =
      std::max(commit_params_->navigation_timing->fetch_start,
               response_head_->load_timing.service_worker_ready_time);

  // A navigation is user activated if it contains a user gesture or the frame
  // received a gesture and the navigation is renderer initiated. If the
  // navigation is browser initiated, it has to come from the context menu.
  // In all cases, the previous and new URLs have to match the
  // `ShouldPropagateUserActivation` requirements (same eTLD+1).
  // There are two different checks:
  // 1. if the `frame_tree_node_` has an origin and is following the rules above
  //    with the target URL, it is used and the bit is set if the navigation is
  //    renderer initiated and the `frame_tree_node_` had a gesture. This should
  //    apply to same page navigations and is preferred over using the referrer
  //    as it can be changed.
  // 2. if referrer and the target url are following the rules above, two
  //    conditions will set the bit: navigation comes from a gesture and is
  //    renderer initiated (middle click/ctrl+click) or it is coming from a
  //    context menu. This should apply to pages that open in a new tab and we
  //    have to follow the referrer. It means that the activation might not be
  //    transmitted if it should have.
  if (commit_params_->was_activated ==
      blink::mojom::WasActivatedOption::kUnknown) {
    commit_params_->was_activated = blink::mojom::WasActivatedOption::kNo;

    if (!commit_params_->is_browser_initiated &&
        (frame_tree_node_->HasStickyUserActivation() ||
         frame_tree_node_->has_received_user_gesture_before_nav()) &&
        ShouldPropagateUserActivation(
            frame_tree_node_->current_origin(),
            url::Origin::Create(common_params_->url))) {
      commit_params_->was_activated = blink::mojom::WasActivatedOption::kYes;
      // TODO(crbug.com/41367031): the next check is relying on
      // sanitized_referrer_ but should ideally use a more reliable source for
      // the originating URL when the navigation is renderer initiated.
    } else if (((common_params_->has_user_gesture &&
                 !commit_params_->is_browser_initiated) ||
                common_params_->started_from_context_menu) &&
               ShouldPropagateUserActivation(
                   url::Origin::Create(sanitized_referrer_->url),
                   url::Origin::Create(common_params_->url))) {
      commit_params_->was_activated = blink::mojom::WasActivatedOption::kYes;
    }
  }

  // MHTML document can't be framed into non-MHTML document (and vice versa).
  // The full page must load from the MHTML archive or none of it.
  if (is_mhtml_archive && !IsInMainFrame() && response_should_be_rendered_) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_RESPONSE),
        false /* skip_throttles */, std::nullopt /* error_page_contnet */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  const std::optional<network::mojom::BlockedByResponseReason>
      coep_requires_blocking = EnforceCOEP();
  if (coep_requires_blocking) {
    // TODO(crbug.com/40166503): Investigate what must be done in case of
    // a download.
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(*coep_requires_blocking),
        false /* skip_throttles */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  const auto& url = common_params_->url;

  if (IsDisabledEmbedderInitiatedFencedFrameNavigation()) {
    frame_tree_node_->current_frame_host()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        "Embedder-initiated navigations of fenced frames are not allowed after "
        "both the embedder and embedded fenced frame network access has been "
        "disabled.");
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        /*skip_throttles=*/false, /*error_page_content=*/std::nullopt,
        /*collapse_frame=*/false);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  // The fenced frame root and the nested iframes are required to have the
  // Supports-Loading-Mode HTTP response header "fenced-frame" to be able to
  // load. Otherwise a console error is emitted.
  const bool should_enforce_fenced_frame_opt_in =
      response_should_be_rendered_ && response_head_->headers &&
      frame_tree_node_->IsInFencedFrameTree() &&
      !(url.IsAboutBlank() || url.SchemeIsBlob() ||
        url.SchemeIs(url::kDataScheme));
  if (should_enforce_fenced_frame_opt_in &&
      !IsOptedInFencedFrame(*response_head_->headers)) {
    blink::RecordFencedFrameCreationOutcome(
        blink::FencedFrameCreationOutcome::kResponseHeaderNotOptIn);
    AddDeferredConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kError,
        "Supports-Loading-Mode HTTP response header 'fenced-frame' is required "
        "to load the fenced frame root and its nested iframes.");
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_RESPONSE),
        false /* skip_throttles */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  // [spec]: https://html.spec.whatwg.org/C/#process-a-navigate-response
  // 4. if [...] the result of checking a navigation response's adherence to its
  // embedder policy [...], then set failure to true.
  if (!CheckResponseAdherenceToCoep(url)) {
    // TODO(crbug.com/40166503): Investigate what must be done in
    // case of a download.
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(
                                network::mojom::BlockedByResponseReason::
                                    kCoepFrameResourceNeedsCoepHeader),
                            false /* skip_throttles */,
                            std::nullopt /* error_page_content */,
                            false /* collapse_frame */);
    return;
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
  }

  SelectFrameHostForOnResponseStarted(std::move(url_loader_client_endpoints),
                                      is_download,
                                      std::move(subresource_loader_params));
}

void NavigationRequest::SelectFrameHostForOnResponseStarted(
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    bool is_download,
    SubresourceLoaderParams subresource_loader_params) {
  TRACE_EVENT_WITH_FLOW0(
      "navigation", "NavigationRequest::SelectFrameHostForOnResponseStarted",
      TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                          TRACE_ID_LOCAL(navigation_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK(!HasRenderFrameHost())
      << "`render_frame_host_` should not be set before the "
         "`NavigationRequest` starts to select the RFH.";
  ScopedCrashKeys crash_keys(*this);

  std::string rfh_selected_reason;

  // Select an appropriate renderer to commit the navigation.
  if (IsServedFromBackForwardCache()) {
    NavigationControllerImpl* controller = GetNavigationController();
    auto entry =
        controller->GetBackForwardCache().GetOrEvictEntry(nav_entry_id_);
    if (!entry.has_value() &&
        entry.error() == BackForwardCacheImpl::kEntryIneligibleAndEvicted) {
      // If the RenderFrameHost to restore has been evicted and deleted, or the
      // current navigation is being restarted due to the `GetOrEvictEntry`
      // call, we should stop processing this back/forward cache restore
      // navigation, as the navigation will soon be restarted as a normal
      // history navigation and the current NavigationRequest will be reset.
      // DO NOT ADD CODE after this. The previous call to
      // `GetOrEvictEntry()` has destroyed the NavigationRequest.
      return;
    }
    CHECK(entry.has_value() && entry.value());
    CHECK(entry.value()->render_frame_host());
    render_frame_host_ = entry.value()->render_frame_host()->GetSafeRef();
  } else if (IsPrerenderedPageActivation()) {
    // Prerendering requires changing pages starting at the root node.
    DCHECK(IsInMainFrame());

    render_frame_host_ = GetPrerenderHostRegistry()
                             .GetRenderFrameHostForReservedHost(
                                 prerender_frame_tree_node_id_.value())
                             ->GetSafeRef();
  } else if (response_should_be_rendered_) {
    if (auto result =
            frame_tree_node_->render_manager()->GetFrameHostForNavigation(
                this, &browsing_context_group_swap_, &rfh_selected_reason);
        result.has_value()) {
      render_frame_host_ = result.value()->GetSafeRef();
    } else {
      switch (result.error()) {
        case GetFrameHostForNavigationFailed::kCouldNotReinitializeMainFrame:
          // TODO(crbug.com/40250311): This was unhandled before and
          // remains explicitly unhandled. This branch may be removed in the
          // future.
          break;
        case GetFrameHostForNavigationFailed::kBlockedByPendingCommit:
          DCHECK(ShouldQueueDueToExistingPendingCommitRFH());
          // This closure is posted to the event loop, so it must use WeakPtr.
          resume_commit_closure_ = base::BindOnce(
              &NavigationRequest::SelectFrameHostForOnResponseStarted,
              weak_factory_.GetWeakPtr(),
              std::move(url_loader_client_endpoints), is_download,
              std::move(subresource_loader_params));
          frame_tree_node_->render_manager()
              ->speculative_frame_host()
              ->RecordMetricsForBlockedGetFrameHostAttempt(
                  /* commit_attempt=*/true);
          return;
        case GetFrameHostForNavigationFailed::kIntentionalDefer:
          // We only defer RFH creation when the navigation is not started yet.
          NOTREACHED();
      }
    }

    // GetFrameHostForNavigation() should update associated_rfh_type_, so it
    // should never be NONE here.
    DCHECK_NE(AssociatedRenderFrameHostType::NONE, associated_rfh_type_);

    if (!Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
            GetRenderFrameHost(), GetUrlInfo(),
            /* is_renderer_initiated_check */ false)) {
      CHECK(false);
    }
  } else {
    render_frame_host_ = std::nullopt;
  }
  if (!HasRenderFrameHost()) {
    DCHECK(!response_should_be_rendered_);
  }

  if (!commit_params_->is_browser_initiated && HasRenderFrameHost() &&
      GetRenderFrameHost()->GetProcess() !=
          frame_tree_node_->current_frame_host()->GetProcess()) {
    // Allow the embedder to cancel the cross-process commit if needed.
    if (!frame_tree_node_->navigator()
             .GetDelegate()
             ->ShouldAllowRendererInitiatedCrossProcessNavigation(
                 frame_tree_node_->IsOutermostMainFrame())) {
      net_error_ = net::ERR_ABORTED;
      frame_tree_node_->ResetNavigationRequest(
          NavigationDiscardReason::kInternalCancellation);
      return;
    }
  }

  // Store the URLLoaderClient endpoints until checks have been processed.
  url_loader_client_endpoints_ = std::move(url_loader_client_endpoints);

  subresource_loader_params_ = std::move(subresource_loader_params);

  // Most cases where ShouldAssignSiteForUrlInfo() is false should never load
  // actual content and reach this.  Since only empty document schemes are
  // allowed to leave a SiteInstance's site unassigned, they should follow the
  // !NeedsUrlLoader() path for committing the navigation early without ever
  // making a network request, and hence they should never reach the response
  // processing code here.
  //
  // The sole exception to this is about:blank URLs, since extensions are
  // allowed to redirect to them after a regular network request/response has
  // started.  Hence, about:blank is the only possible URL which both uses
  // unassigned SiteInstances and can reach this point (via an extension
  // redirect).
  if (common_params_->url.IsAboutBlank()) {
    // TODO(alexmos): Convert to a CHECK after verifying that this doesn't
    // happen in practice.
    if (!WasServerRedirect()) {
      DVLOG(1) << "about:blank should only go through the network stack "
               << "when an extension redirects to it.";
      base::debug::DumpWithoutCrashing();
    }
  } else {
    // TODO(alexmos): Convert to a CHECK after verifying that this doesn't
    // happen in practice.
    if (!SiteInstanceImpl::ShouldAssignSiteForUrlInfo(GetUrlInfo())) {
      DVLOG(1) << "This URL was unexpectedly loaded through the network stack: "
               << common_params_->url;
      base::debug::DumpWithoutCrashing();
    }
  }

  if (HasRenderFrameHost()) {
    // Set the site URL now if it hasn't been set already. If the site requires
    // a dedicated process, this will lock the process to that site, which will
    // prevent other sites from incorrectly reusing this process. See
    // https://crbug.com/738634.
    SiteInstanceImpl* instance = GetRenderFrameHost()->GetSiteInstance();
    if (!instance->HasSite() &&
        SiteInstanceImpl::ShouldAssignSiteForUrlInfo(GetUrlInfo())) {
      instance->ConvertToDefaultOrSetSite(GetUrlInfo());
    }

    // Since we've made the final pick for the RenderFrameHost above, the picked
    // RenderFrameHost's process should be considered "tainted" for future
    // process reuse decisions. That is, a site requiring a dedicated process
    // should not reuse this process, unless it's same-site with the URL we're
    // committing.
    //
    // The process must be marked used after calling ConvertToDefaultOrSetSite,
    // because that call verifies that a SiteInstance with an unassigned site
    // (e.g., about:blank) can only be locked to a site if it is still unused.
    //
    // Note that although NavigationThrottles could still cancel the navigation
    // as part of WillProcessResponse below, we must update the process here,
    // since otherwise there could be a race if a NavigationThrottle defers the
    // navigation, and in the meantime another navigation reads the incorrect
    // IsUnused() value from the same process when making a process reuse
    // decision.
    GetRenderFrameHost()->GetProcess()->SetIsUsed();

    // Now that we know the IsolationContext for the assigned SiteInstance, we
    // opt the origin into OAC here if needed. Note that this doesn't need to
    // account for loading data URLs with a base URL, because such a base URL
    // can never opt into OAC.
    // TODO(wjmaclean): Remove this call/function when same-process
    // OriginAgentCluster moves to SiteInstanceGroup, as then all OAC origins
    // will get a SiteInstance (regardless of process isolation) and tracking
    // will be handled by the existing pathway in
    // SiteInstanceImpl::SetSiteInfoInternal().
    const IsolationContext& isolation_context = instance->GetIsolationContext();
    AddOriginAgentClusterStateIfNecessary(isolation_context);

    // TODO(wjmaclean): Once this is all working, consider combining the
    // following code into the function above.
    // If this navigation request didn't opt-in to origin isolation, we need
    // to check here in case the origin has previously requested isolation and
    // should be marked as opted-out in this SiteInstance. At this point we know
    // that |render_frame_host_|'s SiteInstance has been finalized, so it's safe
    // to use it here to get the correct |IsolationContext|.
    //
    // When loading a data URL with a base URL, use the base URL to calculate
    // the origin; otherwise, `AddDefaultIsolatedOriginIfNeeded()` will simply
    // do nothing as a data: URL has an opaque origin.
    //
    // TODO(wjmaclean): this won't handle cases like about:blank (where it
    // inherits an origin we care about).  We plan to compute the origin
    // before commit time (https://crbug.com/888079), which may make it
    // possible to compute the right origin here.
    const url::Origin origin =
        IsLoadDataWithBaseURL()
            ? url::Origin::Create(common_params_->base_url_for_data_url)
            : url::Origin::Create(common_params_->url);
    ChildProcessSecurityPolicyImpl::GetInstance()
        ->AddDefaultIsolatedOriginIfNeeded(
            isolation_context, origin,
            false /* is_global_walk_or_frame_removal */);

    // Replace the SiteInstance of the previously committed entry if it's for a
    // url that doesn't require a site assignment, if this new commit will be
    // assigning an incompatible site to the previous SiteInstance. This ensures
    // the new SiteInstance can be used with the old entry if we return to it.
    // See http://crbug.com/992198 for further context.
    NavigationEntryImpl* nav_entry =
        frame_tree_node_->navigator().controller().GetLastCommittedEntry();
    if (nav_entry && !nav_entry->GetURL().IsAboutBlank() &&
        !SiteInstance::ShouldAssignSiteForURL(nav_entry->GetURL()) &&
        SiteInstanceImpl::ShouldAssignSiteForUrlInfo(GetUrlInfo())) {
      scoped_refptr<FrameNavigationEntry> frame_entry =
          nav_entry->root_node()->frame_entry;
      scoped_refptr<SiteInstanceImpl> new_site_instance =
          base::WrapRefCounted<SiteInstanceImpl>(static_cast<SiteInstanceImpl*>(
              instance->GetRelatedSiteInstance(frame_entry->url()).get()));
      nav_entry->AddOrUpdateFrameEntry(
          frame_tree_node_, NavigationEntryImpl::UpdatePolicy::kReplace,
          frame_entry->item_sequence_number(),
          frame_entry->document_sequence_number(),
          frame_entry->navigation_api_key(), new_site_instance.get(),
          frame_entry->source_site_instance(), frame_entry->url(),
          frame_entry->committed_origin(), frame_entry->referrer(),
          frame_entry->initiator_origin(), frame_entry->initiator_base_url(),
          frame_entry->redirect_chain(), frame_entry->page_state(),
          frame_entry->method(), frame_entry->post_id(),
          frame_entry->blob_url_loader_factory(),
          frame_entry->policy_container_policies()
              ? frame_entry->policy_container_policies()->ClonePtr()
              : nullptr);
    }
  }

  devtools_instrumentation::OnNavigationResponseReceived(*this,
                                                         *response_head_);

  // The response code indicates that this is an error page, but we don't
  // know how to display the content.  We follow Firefox here and show our
  // own error page instead of intercepting the request as a stream or a
  // download.
  if (IsFailedDownload(is_download, response_head_->headers.get())) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_RESPONSE),
        false /* skip_throttles */, std::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  net::Error net_error = CheckContentSecurityPolicy(
      was_redirected_ /* has_followed_redirect */,
      false /* url_upgraded_after_redirect */, true /* is_response_check */);
  DCHECK_NE(net_error, net::ERR_BLOCKED_BY_CLIENT);
  if (net_error != net::OK) {
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            std::nullopt /* error_page_content */,
                            false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // TODO(crbug.com/40065692): Remove.
  SCOPED_CRASH_KEY_STRING256(
      "Bug1454273", "base_host_for_data_url",
      common_params_->base_url_for_data_url.host_piece());
  SCOPED_CRASH_KEY_STRING1024("Bug1454273", "rfh_selected_reason",
                              rfh_selected_reason);

  if (HasRenderFrameHost() &&
      !CheckPermissionsPoliciesForFencedFrames(GetOriginToCommit().value())) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, std::nullopt /*error_page_content*/,
        false /*collapse_frame*/);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  // Check if the navigation should be allowed to proceed.
  WillProcessResponse();
}

void NavigationRequest::OnRequestFailed(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_NE(status.error_code, net::OK);

  OnRequestFailedInternal(
      status, false /* skip_throttles */, std::nullopt /* error_page_content */,
      status.should_collapse_initiator /* collapse_frame */);
}

std::optional<NavigationEarlyHintsManagerParams>
NavigationRequest::CreateNavigationEarlyHintsManagerParams(
    const network::mojom::EarlyHints& early_hints) {
  TRACE_EVENT_WITH_FLOW0(
      "navigation",
      "NavigationRequest::CreateNavigationEarlyHintsManagerParams",
      TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                          TRACE_ID_LOCAL(navigation_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // Early Hints preloads should happen only before the final response is
  // received, and limited only in the main frame for now.
  CHECK(!HasRenderFrameHost());
  CHECK(loader_);
  CHECK_LT(state_, WILL_PROCESS_RESPONSE);
  CHECK(!IsSameDocument());
  CHECK(IsInMainFrame());
  DCHECK(!IsPageActivation());

  // Getting a RenderProcessHost from a tentative RenderFrameHost during
  // navigation is generally discouraged because it has potential performance
  // impact (the RenderProcessHost could be discarded without actually being
  // used after a cross origin redirect). However, Early Hints preloads require
  // the RenderProcessHost for the tentative RenderFrameHost to set up
  // URLLoaderFactoryParams accordingly. The performance implication should be
  // negligible for Early Hints because these are rarely followed by cross
  // origin redirects. Early Hints preloads before a cross origin redirect don't
  // make sense since such preloads are not available for the redirected page.
  // The CrossOriginRedirectAfterEarlyHints variant of
  // Navigation.MainFrame.TimeToReadyToCommit2 histogram tracks the performance
  // impacts.
  auto result = frame_tree_node_->render_manager()->GetFrameHostForNavigation(
      this, &browsing_context_group_swap_);

  // Early hints is an optimization; if it is not possible to get a suitable
  // RenderFrameHost for any reason, just bail out.
  if (!result.has_value()) {
    return std::nullopt;
  }

  RenderProcessHost* process = result.value()->GetProcess();

  // The process is shutting down.
  if (!process->GetBrowserContext())
    return std::nullopt;

  // Compute sandbox flags. Currently just inherit from the frame.
  // TODO(crbug.com/40188470): Think about the right way the specification
  // should handle sandbox flags with Early Hints.
  network::mojom::WebSandboxFlags sandbox_flags =
      commit_params_->frame_policy.sandbox_flags;

  const url::Origin tentative_origin =
      GetOriginForURLLoaderFactoryBeforeResponse(sandbox_flags);

  mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer;
  Clone(cookie_observer.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
      trust_token_observer;
  Clone(trust_token_observer.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
      shared_dictionary_observer;
  Clone(shared_dictionary_observer.InitWithNewPipeAndPassReceiver());

  network::mojom::URLLoaderFactoryParamsPtr url_loader_factory_params =
      URLLoaderFactoryParamsHelper::CreateForEarlyHintsPreload(
          process, tentative_origin, *this, early_hints,
          std::move(cookie_observer), std::move(trust_token_observer),
          std::move(shared_dictionary_observer));

  net::IsolationInfo isolation_info = url_loader_factory_params->isolation_info;

  // TODO(crbug.com/40188470): Support DevTools instrumentation and extension's
  // WebRequest API.
  auto loader_factory = url_loader_factory::CreatePendingRemote(
      ContentBrowserClient::URLLoaderFactoryType::kEarlyHints,
      url_loader_factory::TerminalParams::ForNetworkContext(
          process->GetStoragePartition()->GetNetworkContext(),
          std::move(url_loader_factory_params)));

  did_create_early_hints_manager_params_ = true;
  return NavigationEarlyHintsManagerParams(
      tentative_origin, std::move(isolation_info),
      mojo::Remote<network::mojom::URLLoaderFactory>(
          std::move(loader_factory)));
}

void NavigationRequest::OnRequestFailedInternal(
    const network::URLLoaderCompletionStatus& status,
    bool skip_throttles,
    const std::optional<std::string>& error_page_content,
    bool collapse_frame) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::OnRequestFailedInternal",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CheckStateTransition(WILL_FAIL_REQUEST);
  DCHECK(!(status.error_code == net::ERR_ABORTED &&
           error_page_content.has_value()));
  ScopedCrashKeys crash_keys(*this);

  // The request failed, the |loader_| must not call the NavigationRequest
  // anymore from now while the error page is being loaded.
  loader_.reset();

  // Reset the RenderFrameHost R1 that had been computed for committing the
  // failed navigation. This breaks the binding between the current
  // NavigationRequest and R1, so that if we create another speculative
  // RenderFrameHost R2 to commit an error page after this, deleting R1 won't
  // try to delete this NavigationRequest along with it.
  render_frame_host_ = std::nullopt;

  ssl_info_ = status.ssl_info;

  devtools_instrumentation::OnNavigationRequestFailed(*this, status);

  // TODO(crbug.com/41340435): Check that ssl_info.has_value() if
  // net_error is a certificate error.
  EnterChildTraceEvent("OnRequestFailed", this, "error", status.error_code);
  SetState(WILL_FAIL_REQUEST);
  processing_navigation_throttle_ = false;

  // Ensure the pending entry also gets discarded if it has no other active
  // requests.
  pending_entry_ref_.reset();

  net_error_ = static_cast<net::Error>(status.error_code);
  extended_error_code_ = status.extended_error_code;
  resolve_error_info_ = status.resolve_error_info;
  navigation_handle_timing_.request_failed_time = base::TimeTicks::Now();

  if (MaybeCancelFailedNavigation())
    return;

  // Only notify the NavigationHandleTiming update if the navigation will commit
  // an error page (instead of getting ignored and deleted above).
  GetDelegate()->DidUpdateNavigationHandleTiming(this);

  if (collapse_frame) {
    DCHECK_EQ(net::ERR_BLOCKED_BY_CLIENT, status.error_code);
    frame_tree_node_->SetCollapsed(true);
  }

  is_mhtml_or_subframe_ = false;
  // TODO(crbug.com/40736932): Apparently, error pages inherit sandbox
  // flags from their parent/opener. Document loaded from the network
  // shouldn't have any influence over Chrome's internal error page. We should
  // define our own flags, preferably the strictest ones instead.
  ComputePoliciesToCommitForError();

  const auto origin = url::Origin();
  // Set the COOP origin in the policy container builder before FinalPolicies()
  // is called.
  policy_container_builder_->GetPolicyContainerHost()
      ->cross_origin_opener_policy()
      .origin = origin;
  coop_status_.EnforceCOOP(
      policy_container_builder_->FinalPolicies().cross_origin_opener_policy,
      origin, net::NetworkAnonymizationKey::CreateTransient());

  SelectFrameHostForOnRequestFailedInternal(status.exists_in_cache,
                                            skip_throttles, error_page_content);
}

void NavigationRequest::SelectFrameHostForOnRequestFailedInternal(
    bool exists_in_cache,
    bool skip_throttles,
    const std::optional<std::string>& error_page_content) {
  CHECK(!HasRenderFrameHost())
      << "`render_frame_host_` should not be set before the "
         "`NavigationRequest` starts to select the RFH.";

  switch (ComputeErrorPageProcess()) {
    case ErrorPageProcess::kCurrentProcess:
      // There's no way to get here with a same-document navigation, it would
      // need to be on a document that was not blocked but became blocked, but
      // same document navigations don't go to the network so it wouldn't know
      // about the change.
      CHECK(!IsSameDocument());
      break;
    case ErrorPageProcess::kIsolatedProcess:
      // In this case we are isolating the error page from the source and
      // destination process, and want it to go to a new process.
      //
      // TODO(nasko): Investigate whether GetFrameHostForNavigation can properly
      // account for clearing the expected process if it clears the speculative
      // RenderFrameHost. See https://crbug.com/793127.
      ResetExpectedProcess();
      [[fallthrough]];
    case ErrorPageProcess::kDestinationProcess:
      // A same-document navigation would normally attempt to navigate the
      // current document, but since we will be presenting an error instead and
      // there will not be a document to navigate. We always make an error here
      // into a cross-document navigation. See https://crbug.com/1018385 and
      // https://crbug.com/1125106.
      common_params_->navigation_type =
          ConvertToCrossDocumentType(common_params_->navigation_type);
      break;
    case ErrorPageProcess::kNotErrorPage:
    case ErrorPageProcess::kPostCommitErrorPage:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  RenderFrameHostImpl* render_frame_host = nullptr;
  if (auto result =
          frame_tree_node_->render_manager()->GetFrameHostForNavigation(
              this, &browsing_context_group_swap_);
      result.has_value()) {
    render_frame_host = result.value();
  } else {
    switch (result.error()) {
      case GetFrameHostForNavigationFailed::kCouldNotReinitializeMainFrame:
        // TODO(crbug.com/40250311): This was unhandled
        // before and remains explicitly unhandled. This branch may be
        // removed in the future.
        break;
      case GetFrameHostForNavigationFailed::kBlockedByPendingCommit:
        resume_commit_closure_ = base::BindOnce(
            &NavigationRequest::SelectFrameHostForOnRequestFailedInternal,
            weak_factory_.GetWeakPtr(), exists_in_cache, skip_throttles,
            error_page_content);
        frame_tree_node_->render_manager()
            ->speculative_frame_host()
            ->RecordMetricsForBlockedGetFrameHostAttempt(
                /* commit_attempt=*/true);
        return;
      case GetFrameHostForNavigationFailed::kIntentionalDefer:
        // We only defer RFH creation when the navigation is not started yet.
        NOTREACHED();
    }
  }

  render_frame_host_ = render_frame_host->GetSafeRef();

  // Update the associated RenderFrameHost type.
  SetAssociatedRFHType(
      GetRenderFrameHost() ==
              frame_tree_node_->render_manager()->current_frame_host()
          ? AssociatedRenderFrameHostType::CURRENT
          : AssociatedRenderFrameHostType::SPECULATIVE);

  // Set the site URL now if it hasn't been set already.  It's possible to get
  // here if we navigate to an error out of an initial "blank" SiteInstance.
  // Also mark the process as used, since it will be hosting an error page.
  SiteInstanceImpl* instance = GetRenderFrameHost()->GetSiteInstance();
  if (!instance->HasSite())
    instance->ConvertToDefaultOrSetSite(GetUrlInfo());
  GetRenderFrameHost()->GetProcess()->SetIsUsed();

  // The check for WebUI should be performed only if error page isolation is
  // enabled for this failed navigation. It is possible for subframe error page
  // to be committed in a WebUI process as shown in https://crbug.com/944086.
  if (frame_tree_node_->IsErrorPageIsolationEnabled()) {
    if (!Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
            GetRenderFrameHost(), GetUrlInfo(),
            /* is_renderer_initiated_check */ false)) {
      CHECK(false);
    }
  }

  has_stale_copy_in_cache_ = exists_in_cache;

  if (skip_throttles) {
    // The NavigationHandle shouldn't be notified about renderer-debug URLs.
    // They will be handled by the renderer process.
    CommitErrorPage(error_page_content);
  } else {
    // Check if the navigation should be allowed to proceed.
    WillFailRequest();
  }
}

NavigationRequest::ErrorPageProcess
NavigationRequest::ComputeErrorPageProcess() {
  if (net_error_ == net::OK) {
    return ErrorPageProcess::kNotErrorPage;
  }

  if (state_ < NavigationRequest::CANCELING) {
    CHECK(!post_commit_error_page_html_.empty());
    // Post-commit error page normally goes through the "non-error page"
    // navigation path, so treat them specially here too.
    return ErrorPageProcess::kPostCommitErrorPage;
  }

  // By policy we can isolate all error pages from both the current and
  // destination processes.
  if (frame_tree_node_->IsErrorPageIsolationEnabled())
    return ErrorPageProcess::kIsolatedProcess;

  // Decide whether to leave the error page in the original process.
  // * If this was a renderer-initiated navigation, and the request is blocked
  //   because the initiating document wasn't allowed to make the request,
  //   commit the error in the existing process. This is a strategy to to
  //   avoid creating a process for the destination, which may belong to an
  //   origin with a higher privilege level.
  // * Error pages resulting from errors like network outage, no network, or
  //   DNS error can reasonably expect that a reload at a later point in time
  //   would work. These should be allowed to transfer away from the current
  //   process: they do belong to whichever process that will host the
  //   destination URL, as a reload will end up committing in that process
  //   anyway.
  // * Error pages that arise during browser-initiated navigations to blocked
  //   URLs should be allowed to transfer away from the current process, which
  //   didn't request the navigation and may have a higher privilege level
  //   than the blocked destination.
  if (net::IsRequestBlockedError(net_error_) && !browser_initiated()) {
    return ErrorPageProcess::kCurrentProcess;
  }
  return ErrorPageProcess::kDestinationProcess;
}

void NavigationRequest::OnStartChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::OnStartChecksComplete",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(result.action() != NavigationThrottle::DEFER);
  DCHECK(result.action() != NavigationThrottle::BLOCK_RESPONSE);

  if (on_start_checks_complete_closure_)
    std::move(on_start_checks_complete_closure_).Run();
  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL ||
      result.action() == NavigationThrottle::BLOCK_REQUEST ||
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
#if DCHECK_IS_ON()
    if (result.action() == NavigationThrottle::BLOCK_REQUEST) {
      DCHECK(net::IsRequestBlockedError(result.net_error_code()));
    }
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    else if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE) {
      DCHECK_EQ(result.net_error_code(), net::ERR_ABORTED);
    }
#endif

    bool collapse_frame =
        result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE;

    // If the start checks completed synchronously, which could happen if there
    // is no onbeforeunload handler or if a NavigationThrottle cancelled it,
    // then this could cause reentrancy into NavigationController. So use a
    // PostTask to avoid that.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&NavigationRequest::OnRequestFailedInternal,
                                  weak_factory_.GetWeakPtr(),
                                  network::URLLoaderCompletionStatus(
                                      result.net_error_code()),
                                  true /* skip_throttles */,
                                  result.error_page_content(), collapse_frame));

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  StoragePartition* partition = GetStoragePartitionWithCurrentSiteInfo();
  DCHECK(partition);

  // |loader_| should not exist if the service worker handle
  // will be destroyed, since it holds raw pointers to it. See the
  // comment in the header for |loader_|.
  DCHECK(!loader_);

  // Only initialize the ServiceWorkerMainResourceHandle if it can be created
  // for this frame.
  bool can_create_service_worker =
      (frame_tree_node_->pending_frame_policy().sandbox_flags &
       network::mojom::WebSandboxFlags::kOrigin) !=
      network::mojom::WebSandboxFlags::kOrigin;
  if (can_create_service_worker) {
    ServiceWorkerContextWrapper* service_worker_context =
        static_cast<ServiceWorkerContextWrapper*>(
            partition->GetServiceWorkerContext());
    service_worker_handle_ = std::make_unique<ServiceWorkerMainResourceHandle>(
        service_worker_context,
        base::BindRepeating(&NavigationRequest::OnServiceWorkerAccessed,
                            weak_factory_.GetWeakPtr()));
  }

  // Mark the fetch_start (Navigation Timing API).
  commit_params_->navigation_timing->fetch_start = base::TimeTicks::Now();

  // Ensure that normal history navigations can dispatch the Navigation API's
  // navigate event as the navigation is starting. Cases without a UrlLoader
  // are handled in OnWillCommitWithoutUrlLoaderChecksComplete.
  MaybeDispatchNavigateEventForCrossDocumentTraversal();

  std::unique_ptr<NavigationUIData> navigation_ui_data;
  if (navigation_ui_data_)
    navigation_ui_data = navigation_ui_data_->Clone();

  // Give DevTools a chance to override begin params (headers, skip SW)
  // before actually loading resource.
  bool report_raw_headers = false;
  std::optional<std::vector<net::SourceStream::SourceType>>
      devtools_accepted_stream_types;
  devtools_instrumentation::ApplyNetworkRequestOverrides(
      frame_tree_node_, begin_params_.get(), &report_raw_headers,
      &devtools_accepted_stream_types, &devtools_user_agent_override_,
      &devtools_accept_language_override_);
  devtools_instrumentation::OnNavigationRequestWillBeSent(*this);

  // Merge headers with embedder's headers.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);
  headers.MergeFrom(TakeModifiedRequestHeaders());
  begin_params_->headers = headers.ToString();

  // TODO(clamy): Avoid cloning the navigation params and create the
  // ResourceRequest directly here.
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptor;
  net::HttpRequestHeaders cors_exempt_headers;
  std::swap(cors_exempt_headers, cors_exempt_request_headers_);

  auto loader_type = NavigationURLLoader::LoaderType::kRegular;
  network::mojom::URLResponseHeadPtr cached_response_head = nullptr;
  if (IsServedFromBackForwardCache()) {
    loader_type = NavigationURLLoader::LoaderType::kNoopForBackForwardCache;
    cached_response_head = GetRenderFrameHostRestoredFromBackForwardCache()
                               ->last_response_head()
                               ->Clone();
  } else if (IsPrerenderedPageActivation()) {
    loader_type = NavigationURLLoader::LoaderType::kNoopForPrerender;
    DCHECK(prerender_frame_tree_node_id_.has_value());
    const network::mojom::URLResponseHeadPtr& last_response_head =
        GetPrerenderHostRegistry()
            .GetRenderFrameHostForReservedHost(*prerender_frame_tree_node_id_)
            ->last_response_head();
    // As PrerenderCommitDeferringCondition makes sure to finish the prerender
    // initial navigation before activation, a valid last_response_head should
    // be always stored before reaching here.
    DCHECK(last_response_head);
    cached_response_head = last_response_head->Clone();
  }

  // Sandbox flags inherited from the frame. In particular, this does not
  // include:
  // - Sandbox flags inherited from the creator via the PolicyContainer.
  // - Sandbox flags forced for MHTML documents.
  // - Sandbox flags from the future response via CSP.
  // It is used by the ExternalProtocolHandler to ensure sandboxed iframe won't
  // navigate the user toward a different application, which can be seen as a
  // main frame navigation somehow.
  network::mojom::WebSandboxFlags sandbox_flags =
      commit_params_->frame_policy.sandbox_flags;

  // Reset the compositor lock before starting the loader.
  compositor_lock_.reset();

  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();

  // Create `PrefetchServingPageMetricsContainer` only if the initiator
  // document has its `PrefetchDocumentManager`.
  base::WeakPtr<PrefetchServingPageMetricsContainer>
      serving_page_metrics_container;
  if (!IsSameDocument() && initiator_document_token_ &&
      PrefetchDocumentManager::FromDocumentToken(initiator_process_id_,
                                                 *initiator_document_token_)) {
    serving_page_metrics_container =
        PrefetchServingPageMetricsContainer::GetOrCreateForNavigationHandle(
            *this)
            ->GetWeakPtr();
  }

  loader_ = NavigationURLLoader::Create(
      browser_context, partition,
      std::make_unique<NavigationRequestInfo>(
          common_params_->Clone(), begin_params_.Clone(), sandbox_flags,
          GetIsolationInfo(),
          frame_tree_node_->current_frame_host()->IsInPrimaryMainFrame(),
          frame_tree_node_->IsOutermostMainFrame(),
          frame_tree_node_->IsMainFrame(),
          IsSecureFrame(frame_tree_node_->parent()),
          frame_tree_node_->frame_tree_node_id(), report_raw_headers,
          upgrade_if_insecure_,
          blob_url_loader_factory_ ? blob_url_loader_factory_->Clone()
                                   : nullptr,
          devtools_navigation_token(),
          frame_tree_node_->current_frame_host()->devtools_frame_token(),
          std::move(cors_exempt_headers),
          BuildClientSecurityStateForNavigationFetch(),
          devtools_accepted_stream_types, is_pdf_, GetInitiatorProcessId(),
          initiator_document_token_, GetPreviousRenderFrameHostId(),
          std::move(serving_page_metrics_container),
          allow_cookies_from_browser_, navigation_id_,
          shared_storage_writable_eligible_, is_ad_tagged_,
          force_no_https_upgrade_),
      std::move(navigation_ui_data), service_worker_handle_.get(),
      std::move(prefetched_signed_exchange_cache_), this, loader_type,
      CreateCookieAccessObserver(), CreateTrustTokenAccessObserver(),
      CreateSharedDictionaryAccessObserver(),
      static_cast<StoragePartitionImpl*>(partition)
          ->CreateURLLoaderNetworkObserverForNavigationRequest(*this),
      NetworkServiceDevToolsObserver::MakeSelfOwned(frame_tree_node_),
      std::move(cached_response_head), std::move(interceptor));
  DCHECK(!HasRenderFrameHost());

  // If needed, perform an early RenderFrameHost swap after notifying observers
  // with DidStartNavigation, after processing WillStartRequest throttle events,
  // and after creating the NavigationURLLoader above (which needs the old
  // current_frame_host()). This (1) avoids performing the early swap in case
  // the navigation gets canceled prior to getting here, and (2) ensures that
  // DidStartNavigation and WillStartRequest implementations are not disrupted
  // by the early swap and don't see a RenderFrameHostChanged event prior to a
  // navigation actually starting.
  //
  // TODO(crbug.com/40276607): Remove the `is_called_after_did_start_navigation`
  // param once all early swaps are done from here.
  frame_tree_node_->render_manager()->PerformEarlyRenderFrameHostSwapIfNeeded(
      this, /*is_called_after_did_start_navigation=*/true);

  base::UmaHistogramTimes(
      base::StrCat({"Navigation.WillStartRequestToLoaderStart.",
                    IsInMainFrame() ? "MainFrame" : "Subframe"}),
      base::TimeTicks::Now() - will_start_request_time_);
  absl::Cleanup scoped_set_now = [navigation_request =
                                      weak_factory_.GetWeakPtr()] {
    if (navigation_request) {
      navigation_request->navigation_handle_timing_.loader_start_time =
          base::TimeTicks::Now();
      navigation_request->GetDelegate()->DidUpdateNavigationHandleTiming(
          navigation_request.get());
    }
  };

  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());
  loader_->Start();

  if (!this_ptr) {
    // `this` have been deleted by NavigationURLLoader::Start
    // DO NOT ADD CODE HERE.
    return;
  }

  // Try to create the speculative RFH after sending the network request
  // if DeferSpeculativeRFHCreation is enabled.
  // Only create the speculative RFH if it is a normal loading rather than
  // a BFCache restore or prerender activation. Otherwise `OnResponseStarted`
  // will be called instantly and the creation of the speculative RFH is
  // redundant.
  if (base::FeatureList::IsEnabled(features::kDeferSpeculativeRFHCreation) &&
      GetAssociatedRFHType() == AssociatedRenderFrameHostType::NONE) {
    if (features::kCreateSpeculativeRFHFilterRestore.Get() &&
        loader_type != NavigationURLLoader::LoaderType::kRegular) {
      return;
    }
    auto create_speculative_rfh_task = base::BindOnce(
        [](base::WeakPtr<NavigationRequest> request) {
          if (!request || request->state_ >= WILL_PROCESS_RESPONSE ||
              request->HasRenderFrameHost()) {
            return;
          }
          auto rfh_creation_result =
              request->frame_tree_node_->render_manager()
                  ->GetFrameHostForNavigation(
                      request.get(), &request->browsing_context_group_swap_);
          if (rfh_creation_result.has_value()) {
            request->SetExpectedProcessIfAssociated();
          }
        },
        weak_factory_.GetWeakPtr());
    int delay_ms = features::kCreateSpeculativeRFHDelayMs.Get();
    if (delay_ms > 0) {
      GetUIThreadTaskRunner()->PostDelayedTask(
          FROM_HERE, std::move(create_speculative_rfh_task),
          base::Milliseconds(delay_ms));
    } else {
      std::move(create_speculative_rfh_task).Run();
    }
  }
}

void NavigationRequest::OnServiceWorkerAccessed(
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  GetDelegate()->OnServiceWorkerAccessed(this, scope, allowed);
}

network::mojom::WebSandboxFlags NavigationRequest::SandboxFlagsInitiator() {
  return sandbox_flags_initiator_;
}

network::mojom::WebSandboxFlags NavigationRequest::SandboxFlagsInherited() {
  return commit_params_->frame_policy.sandbox_flags;
}

network::mojom::WebSandboxFlags NavigationRequest::SandboxFlagsToCommit() {
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);
  DCHECK(!IsSameDocument());
  DCHECK(!IsPageActivation());
  return policy_container_builder_->FinalPolicies().sandbox_flags;
}

void NavigationRequest::MaybeAddResourceTimingEntryForCancelledNavigation() {
  if (!base::FeatureList::IsEnabled(
          features::kResourceTimingForCancelledNavigationInFrame)) {
    return;
  }

  // Some navigation are cancelled even before requesting and receiving a
  // response. Those cases are not supported and the ResourceTiming is not
  // reported to the parent.
  if (!response()) {
    return;
  }

  network::URLLoaderCompletionStatus status;
  status.encoded_data_length = response()->encoded_data_length;
  status.completion_time = base::TimeTicks::Now();
  AddResourceTimingEntryForFailedSubframeNavigation(status);
}

void NavigationRequest::AddResourceTimingEntryForFailedSubframeNavigation(
    const network::URLLoaderCompletionStatus& status) {
  // For TAO-fail navigations, we would resort to fallback timing.
  // See HTMLFrameOwnerElement::ReportFallbackResourceTimingIfNeeded().
  DCHECK(response());
  if (commit_params().navigation_timing->parent_resource_timing_access ==
      blink::mojom::ParentResourceTimingAccess::kDoNotReport) {
    return;
  }

  network::mojom::URLResponseHeadPtr response_head = response()->Clone();

  bool allow_response_details =
      commit_params().navigation_timing->parent_resource_timing_access ==
      blink::mojom::ParentResourceTimingAccess::kReportWithResponseDetails;

  GetParentFrame()->AddResourceTimingEntryForFailedSubframeNavigation(
      frame_tree_node(), common_params().navigation_start,
      commit_params().navigation_timing->redirect_end,
      commit_params().original_url, common_params().url,
      std::move(response_head), allow_response_details, status);
}

void NavigationRequest::OnRedirectChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::OnRedirectChecksComplete",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(result.action() != NavigationThrottle::DEFER);
  DCHECK(result.action() != NavigationThrottle::BLOCK_RESPONSE);
  DCHECK(!IsPageActivation());

  bool collapse_frame =
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE;

  // Abort the request if needed. This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL) {
    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE if needed.
    DCHECK(result.action() == NavigationThrottle::CANCEL ||
           result.net_error_code() == net::ERR_ABORTED);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(), collapse_frame);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (result.action() == NavigationThrottle::BLOCK_REQUEST ||
      result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE) {
    DCHECK(net::IsRequestBlockedError(result.net_error_code()));
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(), collapse_frame);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  devtools_instrumentation::OnNavigationRequestWillBeSent(*this);

  net::HttpRequestHeaders modified_headers = TakeModifiedRequestHeaders();
  std::vector<std::string> removed_headers = TakeRemovedRequestHeaders();

  // The topics a request is allowed to see can change within its redirect
  // chain thus we need to recalculate them. For example, different caller
  // origins (i.e. navigation URL's origin) may receive different topics, as the
  // callers can only get the topics about the sites they were on. Besides,
  // regardless of cross-origin-ness, the timestamp can also affect the
  // candidate epochs where the topics are derived from, thus resulting in
  // different topics across redirects.
  if (topics_eligible_) {
    topics_eligible_ = false;

    // At this point we may not have a valid `GetRenderFrameHost()` if the
    // navigation is during a cross-site redirect. Thus, pass in the current/old
    // RenderFrameHost here. This is fine, because it should still give us the
    // desired IsPrimary() status and the desired top-level frame that
    // `HandleTopicsEligibleResponse()` is interested in knowing.
    HandleTopicsEligibleResponse(
        commit_params_->redirect_response.back().get()->parsed_headers,
        url::Origin::Create(commit_params_->redirects.back()),
        *frame_tree_node_->current_frame_host(),
        browsing_topics::ApiCallerSource::kIframeAttribute);
  }

  // Removes the topics header. This will effectively be a no-op if the topics
  // header wasn't sent for the previous request.
  removed_headers.push_back(kBrowsingTopicsRequestHeaderKey);

  TopicsHeaderValueResult topics_header_value_result =
      GetTopicsHeaderValueForNavigationRequest(frame_tree_node_,
                                               common_params_->url);

  topics_eligible_ = topics_header_value_result.topics_eligible;

  if (topics_header_value_result.header_value) {
    modified_headers.SetHeader(kBrowsingTopicsRequestHeaderKey,
                               *topics_header_value_result.header_value);
  }

  if (ad_auction_headers_eligible_) {
    // Redirects are ineligible for ad auction headers.
    ad_auction_headers_eligible_ = false;
    removed_headers.push_back(kAdAuctionRequestHeaderKey);
  }

  if (shared_storage_writable_opted_in_) {
    // On a redirect, the PermissionsPolicy may change the status of this
    // request's Shared Storage eligibility, so we need to re-compute it.
    bool previous_shared_storage_writable_eligible =
        shared_storage_writable_eligible_;
    shared_storage_writable_eligible_ =
        IsSharedStorageWritableEligibleForNavigationRequest(
            frame_tree_node_, common_params_->url);

    if (shared_storage_writable_eligible_ !=
        previous_shared_storage_writable_eligible) {
      if (shared_storage_writable_eligible_) {
        modified_headers.SetHeader(kSecSharedStorageWritableRequestHeaderKey,
                                   "?1");
      } else {
        removed_headers.push_back(kSecSharedStorageWritableRequestHeaderKey);
      }
    }
  }

  // Removes all Client Hints from the request, that were passed on from the
  // previous one.
  for (const auto& elem : network::GetClientHintToNameMap()) {
    const auto& header = elem.second;
    removed_headers.push_back(header);
  }

  // Add any required Client Hints to the current request.
  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    net::HttpRequestHeaders client_hints_extra_headers;
    const url::Origin& source_origin =
        url::Origin::Create(commit_params_->redirects.back());
    const network::mojom::URLResponseHead* response_head =
        commit_params_->redirect_response.back().get();
    ParseAndPersistAcceptCHForNavigation(
        source_origin, response_head->parsed_headers,
        response_head->headers.get(), browser_context, client_hints_delegate,
        frame_tree_node_);

    AddNavigationRequestClientHintsHeaders(
        GetTentativeOriginAtRequestTime(), &client_hints_extra_headers,
        browser_context, client_hints_delegate, is_overriding_user_agent(),
        frame_tree_node_, commit_params_->frame_policy.container_policy,
        common_params_->url);
    modified_headers.MergeFrom(client_hints_extra_headers);
    // On a redirect, unless devtools has overridden the User-Agent header, we
    // should send the User-Agent string based on policy.
    if (!devtools_user_agent_override_) {
      modified_headers.SetHeader(
          net::HttpRequestHeaders::kUserAgent,
          ComputeUserAgentValue(modified_headers, GetUserAgentOverride(),
                                browser_context));
    }
  }

  // Add reduced accept language header to the current request.
  // If devtools has overridden the Accept-Language header, skip reduce
  // Accept-Language header.
  if (auto reduce_accept_lang_utils =
          ReduceAcceptLanguageUtils::Create(browser_context);
      reduce_accept_lang_utils && !devtools_accept_language_override_) {
    if (!ReduceAcceptLanguageUtils::CheckDisableReduceAcceptLanguageOriginTrial(
            common_params_->url, frame_tree_node_,
            browser_context->GetOriginTrialsControllerDelegate())) {
      net::HttpRequestHeaders accept_language_headers;
      std::optional<std::string> reduced_accept_language =
          reduce_accept_lang_utils.value()
              .AddNavigationRequestAcceptLanguageHeaders(
                  url::Origin::Create(common_params_->url), frame_tree_node_,
                  &accept_language_headers);
      commit_params_->reduced_accept_language =
          reduced_accept_language.value_or("");
      modified_headers.MergeFrom(accept_language_headers);
    } else {
      // Remove the Accept-Language header passed from previous request, if any.
      removed_headers.push_back(net::HttpRequestHeaders::kAcceptLanguage);
      commit_params_->reduced_accept_language = "";
    }
  }

  net::HttpRequestHeaders cors_exempt_headers;
  std::swap(cors_exempt_headers, cors_exempt_request_headers_);
  loader_->FollowRedirect(std::move(removed_headers),
                          std::move(modified_headers),
                          std::move(cors_exempt_headers));
}

void NavigationRequest::OnFailureChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::OnFailureChecksComplete",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // This method is called as a result of getting to the end of
  // OnRequestFailedInternal(), which calls WillFailRequest(), which
  // runs the throttles, which eventually call back to this method.
  DCHECK(result.action() != NavigationThrottle::DEFER);

  // The throttle may have changed the net_error_code, so we set the
  // `net_error_` again, overriding what OnRequestFailedInternal() set.
  net::Error old_net_error = net_error_;
  ErrorPageProcess old_error_page_process = ComputeErrorPageProcess();
  net_error_ = result.net_error_code();

  // FIXME: Should we clear out |extended_error_code_| here?

  // Ensure that WillFailRequest() isn't changing the error code in a way that
  // switches the destination process for the error page - see
  // https://crbug.com/817881.
  CHECK_EQ(old_error_page_process, ComputeErrorPageProcess())
      << " Unsupported error code change in WillFailRequest(): from "
      << old_net_error << " to " << net_error_;

  // The new `net_error_` value may mean we want to cancel the navigation.
  if (MaybeCancelFailedNavigation())
    return;

  // The OnRequestFailedInternal() did not commit the error page as it
  // deferred to WillFailRequest(), which has called through to here, and
  // now we are finally ready to commit the error page. This will be committed
  // to the RenderFrameHost previously chosen in OnRequestFailedInternal().
  CommitErrorPage(result.error_page_content());
  // DO NOT ADD CODE after this. The previous call to CommitErrorPage()
  // caused the destruction of the NavigationRequest.
}

void NavigationRequest::OnWillProcessResponseChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() != NavigationThrottle::DEFER);

  // If the NavigationThrottles allowed the navigation to continue, have the
  // processing of the response resume in the network stack.
  if (result.action() == NavigationThrottle::PROCEED) {
    // If this is a download and NavigationThrottles allowed it, intercept the
    // navigation response, pass it to DownloadManager, and cancel the
    // navigation.
    if (is_download_) {
      // TODO(arthursonzogni): Pass the real ResourceRequest. For the moment
      // only these parameters will be used, but it may evolve quickly.
      auto resource_request = std::make_unique<network::ResourceRequest>();
      resource_request->url = common_params_->url;
      resource_request->method = common_params_->method;
      resource_request->request_initiator = common_params_->initiator_origin;
      resource_request->referrer = common_params_->referrer->url;
      resource_request->has_user_gesture = common_params_->has_user_gesture;
      resource_request->mode = network::mojom::RequestMode::kNavigate;
      resource_request->transition_type = common_params_->transition;
      resource_request->trusted_params =
          network::ResourceRequest::TrustedParams();
      resource_request->trusted_params->isolation_info = GetIsolationInfo();

      BrowserContext* browser_context =
          frame_tree_node_->navigator().controller().GetBrowserContext();
      DownloadManagerImpl* download_manager = static_cast<DownloadManagerImpl*>(
          browser_context->GetDownloadManager());
      download_manager->InterceptNavigation(
          std::move(resource_request), redirect_chain_, response_head_.Clone(),
          std::move(response_body_), std::move(url_loader_client_endpoints_),
          ssl_info_.has_value() ? ssl_info_->cert_status : 0,
          frame_tree_node_->frame_tree_node_id(),
          from_download_cross_origin_redirect_);

      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED),
          false /*skip_throttles*/, std::nullopt /*error_page_content*/,
          false /*collapse_frame*/);
      // DO NOT ADD CODE after this. The previous call to OnRequestFailed has
      // destroyed the NavigationRequest.
      return;
    }

    // Per https://whatwg.org/C/iframe-embed-object.html#the-object-element,
    // this implements step 4.7 from "determine what the object element
    // represents": "If the load failed (e.g. there was an HTTP 404 error, there
    // was a DNS error), fire an event named error at the element, then jump to
    // the step below labeled fallback."
    //
    // This case handles HTTP errors, which are otherwise considered a
    // "successful" navigation.
    //
    // TODO(dcheng): According to the standard, an <object> element shouldn't
    // even have a browsing context associated with it unless a successful
    // navigation would commit in it.
    if (response()->headers &&
        ShouldRenderFallbackContentForResponse(*response()->headers)) {
      // Spin up a helper to drain the response body for this navigation. The
      // helper will request fallback content (triggering completion) and report
      // the resource timing info once the entire response body is drained.
      //
      // The response body fetcher takes advantage of base::SupportsUserData to
      // ensure that the fetcher does not outlive `this`. This ensures that the
      // fallback / resource timing are only reported if the navigation request
      // is logically still pending.
      ObjectNavigationFallbackBodyLoader::CreateAndStart(
          *this, std::move(response_body_),
          std::move(url_loader_client_endpoints_),
          base::BindOnce(&NavigationRequest::OnRequestFailedInternal,
                         weak_factory_.GetWeakPtr(),
                         network::URLLoaderCompletionStatus(net::ERR_ABORTED),
                         false /* skip_throttles */,
                         std::nullopt /* error_page_content */,
                         false /* collapse_frame */));
      // Unlike the other early returns, intentionally skip calling
      // `OnRequestFailedInternal()`. This allows the response body drainer to
      // track the lifetime of `this` and skip the remaining work if `this` is
      // deleted before the response body is completely loaded.
      return;
    }
  }

  // Abort the request if needed. This includes requests that were blocked by
  // NavigationThrottles and requests that should not commit (e.g. downloads,
  // 204/205s). This will destroy the NavigationRequest.
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
      result.action() == NavigationThrottle::CANCEL ||
      !response_should_be_rendered_) {
    MaybeAddResourceTimingEntryForCancelledNavigation();

    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    if (!response_should_be_rendered_) {
      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED),
          true /* skip_throttles */, std::nullopt /* error_page_content */,
          false /* collapse_frame */);

      // DO NOT ADD CODE after this. The previous call to
      // OnRequestFailedInternal has destroyed the NavigationRequest.
      return;
    }

    DCHECK(result.action() == NavigationThrottle::CANCEL ||
           result.net_error_code() == net::ERR_ABORTED);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(),
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (result.action() == NavigationThrottle::BLOCK_RESPONSE) {
    DCHECK_EQ(net::ERR_BLOCKED_BY_RESPONSE, result.net_error_code());
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(),
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  DCHECK_EQ(result.action(), NavigationThrottle::PROCEED);

  // When this request is for prerender activation, `commit_deferrer_` has
  // already been processed.
  if (IsPrerenderedPageActivation()) {
    DCHECK(!commit_deferrer_);
    CommitNavigation();
    // DO NOT ADD CODE after this. The previous call to CommitNavigation
    // destroyed the NavigationRequest.
    return;
  };

  RunCommitDeferringConditions();
  // DO NOT ADD CODE after this. The previous call to
  // RunCommitDeferringConditions may have caused the destruction of the
  // NavigationRequest.
}

void NavigationRequest::OnWillCommitWithoutUrlLoaderChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
         result.action() == NavigationThrottle::PROCEED);
  if (result.action() == NavigationThrottle::CANCEL_AND_IGNORE) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        /*skip_throttles=*/true, result.error_page_content(),
        /*collapse_frame=*/false);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Ensure that bfcache and other non-UrlLoader history navigations can
  // dispatch the Navigation API's navigate event as the navigation is starting.
  // Cases with a UrlLoader are handled in OnStartChecksComplete.
  MaybeDispatchNavigateEventForCrossDocumentTraversal();

  CommitNavigation();
}

void NavigationRequest::RunCommitDeferringConditions() {
  commit_deferrer_->RegisterDeferringConditions(*this);
  commit_deferrer_->ProcessChecks();
  // DO NOT ADD CODE after this. The previous call to ProcessChecks may have
  // caused the destruction of the NavigationRequest.
}

void NavigationRequest::OnCommitDeferringConditionChecksComplete(
    CommitDeferringCondition::NavigationType navigation_type,
    std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id) {
  switch (navigation_type) {
    case CommitDeferringCondition::NavigationType::kPrerenderedPageActivation:
      OnPrerenderingActivationChecksComplete(
          navigation_type, candidate_prerender_frame_tree_node_id);
      // DO NOT ADD CODE after this. The previous call to
      // OnPrerenderingActivationChecksComplete caused the destruction of the
      // NavigationRequest.
      return;
    case CommitDeferringCondition::NavigationType::kOther:
      DCHECK_LT(state_, READY_TO_COMMIT);
      CommitNavigation();
      // DO NOT ADD CODE after this. The previous call to CommitNavigation
      // caused the destruction of the NavigationRequest.
      return;
  }
}

void NavigationRequest::CommitErrorPage(
    const std::optional<std::string>& error_page_content) {
  DCHECK(!IsSameDocument());

  DetermineOriginAgentClusterEndResult();

  UpdateHistoryParamsInCommitNavigationParams();

  common_params_->should_replace_current_entry =
      ShouldReplaceCurrentEntryForFailedNavigation();

  // Don't pass the base url in a failed navigation.
  common_params_->initiator_base_url = std::nullopt;

  if (request_navigation_client_.is_bound()) {
    if (GetRenderFrameHost() ==
        RenderFrameHostImpl::FromID(
            current_render_frame_host_id_at_construction_)) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    } else {
      // This navigation is cross-RenderFrameHost: the original document should
      // no longer be able to cancel it.
      IgnoreInterfaceDisconnection();
    }
  }

  topics_eligible_ = false;

  ad_auction_headers_eligible_ = false;

  base::WeakPtr<NavigationRequest> weak_self(weak_factory_.GetWeakPtr());
  ReadyToCommitNavigation(true /* is_error */);
  // The caller above might result in the deletion of `this`. Return immediately
  // if so.
  if (!weak_self) {
    return;
  }

  PopulateDocumentTokenForCrossDocumentNavigation();
  // Use a separate cache shard, and no cookies, for error pages.
  isolation_info_for_subresources_ = net::IsolationInfo::CreateTransient();
  GetRenderFrameHost()->FailedNavigation(
      this, *common_params_, *commit_params_, has_stale_copy_in_cache_,
      net_error_, extended_error_code_, error_page_content, *document_token_);

  SendDeferredConsoleMessages();
}

void NavigationRequest::AddOldPageInfoToCommitParamsIfNeeded() {
  // Add the routing ID and the updated lifecycle state of the old page if we
  // need to run pagehide and visibilitychange handlers of the old page
  // when we commit the new page.
  auto* old_frame_host =
      frame_tree_node_->render_manager()->current_frame_host();
  if (!GetRenderFrameHost()
           ->ShouldDispatchPagehideAndVisibilitychangeDuringCommit(
               old_frame_host, GetUrlInfo())) {
    return;
  }
  DCHECK(!IsSameDocument());
  // The pagehide event's "persisted" property depends on whether the old page
  // will be put into the back-forward cache or not. As we won't freeze the
  // page until after the commit finished, there is no way to know for sure
  // whether the page will actually be persisted or not after commit, but we
  // will send our best guess by checking if the page can be persisted at this
  // point.
  bool can_store_old_page_in_bfcache =
      frame_tree_node_->frame_tree()
          .controller()
          .GetBackForwardCache()
          .GetFutureBackForwardCacheEligibilityPotential(old_frame_host)
          .CanStore();
  commit_params_->old_page_info = blink::mojom::OldPageInfo::New();
  commit_params_->old_page_info->frame_token_for_old_main_frame =
      old_frame_host->GetFrameToken();
  auto* page_lifecycle_state_manager =
      old_frame_host->render_view_host()->GetPageLifecycleStateManager();
  commit_params_->old_page_info->new_lifecycle_state_for_old_page =
      page_lifecycle_state_manager->SetPagehideDispatchDuringNewPageCommit(
          can_store_old_page_in_bfcache /* persisted */);
}

bool NavigationRequest::ShouldDispatchPageSwapEvent() const {
  const bool feature_enabled =
      base::FeatureList::IsEnabled(blink::features::kPageSwapEvent) ||
      base::FeatureList::IsEnabled(
          blink::features::kViewTransitionOnNavigation);
  if (!feature_enabled) {
    return false;
  }

  if (early_render_frame_host_swap_type_ !=
      EarlyRenderFrameHostSwapType::kNone) {
    return false;
  }

  if (IsSameDocument()) {
    return false;
  }

  return !did_fire_page_swap_;
}

void NavigationRequest::CommitNavigation() {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::CommitNavigation",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // A navigation request should only commit once the response has been
  // processed.
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);
  // If a WebUI was created for this navigation, it must have been moved to the
  // RenderFrameHost we're about to commit in already.
  CHECK(!HasWebUI());
  CheckSoftNavigationHeuristicsInvariants();

  if (!CoopCoepSanityCheck())
    return;

  DetermineOriginAgentClusterEndResult();

  UpdateHistoryParamsInCommitNavigationParams();
  DCHECK(NeedsUrlLoader() == !!response_head_ ||
         (was_redirected_ && common_params_->url.IsAboutBlank()));
  DCHECK(!common_params_->url.SchemeIs(url::kJavaScriptScheme));
  DCHECK(!blink::IsRendererDebugURL(common_params_->url));

  AddOldPageInfoToCommitParamsIfNeeded();
  if (ShouldDispatchPageSwapEvent()) {
    frame_tree_node_->current_frame_host()
        ->GetAssociatedLocalFrame()
        ->DispatchPageSwap(WillDispatchPageSwap());
  }

  url::Origin origin_to_commit = GetOriginToCommit().value();
  isolation_info_for_subresources_ =
      GetRenderFrameHost()->ComputeIsolationInfoForSubresourcesForPendingCommit(
          origin_to_commit, is_credentialless(), ComputeFencedFrameNonce());
  DCHECK(!isolation_info_for_subresources_.IsEmpty());

  // If this is a srcdoc document, the content comes from the parent frame, so
  // the origin must be the parent and not the initiator. In this case, do not
  // inherit the base URI from the initiator if the origins do not agree
  // (accounting for the case that the chosen origin might be opaque with a
  // precursor of the parent's origin, in a sandboxed case). There should also
  // not be an initiator base URL if there is no initiator origin, such as in a
  // browser-initiated navigation.
  if (GetURL().IsAboutSrcdoc() &&
      (!common_params().initiator_origin ||
       origin_to_commit.GetTupleOrPrecursorTupleIfOpaque() !=
           common_params()
               .initiator_origin->GetTupleOrPrecursorTupleIfOpaque())) {
    // TODO(crbug.com/40165505): Make this unreachable by blocking
    // cross-origin about:srcdoc navigations. Then enforce that the chosen
    // origin for srcdoc cases agrees with the parent frame's origin.
    common_params_->initiator_base_url = std::nullopt;
  }

  // TODO(crbug.com/40092527): The storage key's origin is ignored at the
  // moment. We will be able to use it once the browser can compute the origin
  // to commit.
  std::optional<base::UnguessableToken> nonce =
      GetRenderFrameHost()->ComputeNonce(is_credentialless(),
                                         ComputeFencedFrameNonce());

  commit_params_->storage_key = GetRenderFrameHost()->CalculateStorageKey(
      origin_to_commit, base::OptionalToPtr(nonce));

  if (topics_eligible_) {
    topics_eligible_ = false;

    if (response()) {
      HandleTopicsEligibleResponse(
          response_head_->parsed_headers, url::Origin::Create(GetURL()),
          *GetRenderFrameHost(),
          browsing_topics::ApiCallerSource::kIframeAttribute);
    }
  }

  if (ad_auction_headers_eligible_) {
    ProcessAdAuctionResponseHeaders(origin_to_commit,
                                    GetRenderFrameHost()->GetPage(),
                                    response() ? response()->headers : nullptr);
  } else if (has_ad_auction_headers_attribute_) {
    RemoveAdAuctionResponseHeaders(response() ? response()->headers : nullptr);
  }

  RenderFrameHostImpl* old_frame_host =
      frame_tree_node_->render_manager()->current_frame_host();
  if (!NavigationTypeUtils::IsSameDocument(common_params_->navigation_type)) {
    // We want to record this for the frame that we are navigating away from.
    old_frame_host->RecordNavigationSuddenTerminationHandlers();
  }
  if (IsServedFromBackForwardCache() || IsPrerenderedPageActivation()) {
    CommitPageActivation();
    return;
  }

  // For consistency, prerendering activation *should* go through this as well.
  // However, the prerender implementation based on swapping WebContents
  // introduces a number of edge cases that navigation code wouldn't normally
  // have to handle, so it's easier to simply skip this in the hopes that the
  // non-mparch implementation will be removed soon.
  base::WeakPtr<NavigationRequest> weak_self(weak_factory_.GetWeakPtr());
  ReadyToCommitNavigation(false /* is_error */);
  // The call above might block on showing a user dialog. The interaction of
  // the user with this dialog might result in the WebContents owning this
  // NavigationRequest to be destroyed. Return if this is the case.
  if (!weak_self)
    return;

  DCHECK(GetRenderFrameHost() == old_frame_host ||
         GetRenderFrameHost() ==
             frame_tree_node_->render_manager()->speculative_frame_host());

  if (request_navigation_client_.is_bound()) {
    if (GetRenderFrameHost() ==
        RenderFrameHostImpl::FromID(
            current_render_frame_host_id_at_construction_)) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    } else {
      // This navigation is cross-RenderFrameHost: the original document should
      // no longer be able to cancel it.
      IgnoreInterfaceDisconnection();
    }
  }

  CreateCoepReporter(GetRenderFrameHost()->GetProcess()->GetStoragePartition());
  coop_status_.UpdateReporterStoragePartition(
      GetRenderFrameHost()->GetProcess()->GetStoragePartition());

  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    std::optional<std::vector<network::mojom::WebClientHintsType>>
        opt_in_hints_from_response;
    if (response()) {
      opt_in_hints_from_response = ParseAndPersistAcceptCHForNavigation(
          url::Origin::Create(common_params_->url), response()->parsed_headers,
          response()->headers.get(), browser_context, client_hints_delegate,
          frame_tree_node_);
    }
    commit_params_->enabled_client_hints =
        LookupAcceptCHForCommit(origin_to_commit, client_hints_delegate,
                                frame_tree_node_, common_params_->url);
  }
  // Navigation requests should use the new origin as the partition origin
  // except if embedded in an outer frame.
  url::Origin partition_origin = origin_to_commit;
  bool is_top_level = frame_tree_node()->GetParentOrOuterDocument() == nullptr;
  if (!is_top_level) {
    partition_origin = frame_tree_node()
                           ->GetParentOrOuterDocument()
                           ->GetOutermostMainFrame()
                           ->GetLastCommittedOrigin();
  }

  PersistOriginTrialsFromHeaders(origin_to_commit, partition_origin, response(),
                                 browser_context, GetNextPageUkmSourceId());

  // Clean the reduced accept-language to commit if the final response have a
  // valid deprecation origin trial token.
  if (auto reduce_accept_lang_utils =
          ReduceAcceptLanguageUtils::Create(browser_context);
      reduce_accept_lang_utils && !devtools_accept_language_override_ &&
      ReduceAcceptLanguageUtils::CheckDisableReduceAcceptLanguageOriginTrial(
          common_params_->url, frame_tree_node_,
          browser_context->GetOriginTrialsControllerDelegate()) &&
      !commit_params_->reduced_accept_language.empty()) {
    reduce_accept_lang_utils.value().RemoveReducedAcceptLanguage(
        origin_to_commit, frame_tree_node_);
    commit_params_->reduced_accept_language = "";
  }

  // Sticky user activation should only be preserved for same-site subframe
  // navigations. This is done to prevent newly navigated documents from
  // re-using the sticky user activation state from the previously navigated
  // document in the frame. We persist user activation across same-site
  // navigations for compatibility reasons, and this does not need to match the
  // same-site checks used in the process model. See: crbug.com/736415.
  // TODO(crbug.com/40228985): Remove this once we find a way to reset
  // activation unconditionally without breaking sites in practice.
  commit_params_->should_have_sticky_user_activation =
      !frame_tree_node_->IsMainFrame() &&
      old_frame_host->HasStickyUserActivation() &&
      net::SchemefulSite(old_frame_host->GetLastCommittedOrigin()) ==
          net::SchemefulSite(origin_to_commit);

  // Generate a UKM source and track it on NavigationRequest. This will be
  // passed down to the blink::Document to be created, if any, and used for UKM
  // source creation when navigation has successfully committed.
  commit_params_->document_ukm_source_id = ukm::UkmRecorder::GetNewSourceID();

  blink::mojom::ServiceWorkerContainerInfoForClientPtr
      service_worker_container_info;
  blink::mojom::ControllerServiceWorkerInfoPtr controller;

  // Notify the service worker navigation handle that navigation commit is
  // about to go.
  if (service_worker_handle_ &&
      service_worker_handle_->service_worker_client()) {
    DCHECK(coep_reporter());
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        reporter_remote;
    coep_reporter()->Clone(reporter_remote.InitWithNewPipeAndPassReceiver());

    std::tie(service_worker_container_info, controller) =
        service_worker_handle_->scoped_service_worker_client()
            ->CommitResponseAndRelease(
                GetRenderFrameHost()->GetGlobalId(),
                policy_container_builder_->FinalPolicies(),
                std::move(reporter_remote),
                commit_params_->document_ukm_source_id);
  }

  // Determine if top-level navigation is allowed without sticky user
  // activation. This is used to fix the exploit in https://crbug.com/1251790.
  // If a child document is cross-origin with its parent, it loses its ability
  // to navigate top without user gesture. One notable exception is made if its
  // parent embeds it using sandbox="allow-top-navigation". Please note this is
  // quite unusual, because it means using sandbox brings new capabilities, as
  // opposed to new restrictions.
  using WebSandboxFlags = network::mojom::WebSandboxFlags;
  const bool embedder_allows_top_navigation_explicitly =
      ((commit_params_->frame_policy.sandbox_flags != WebSandboxFlags::kNone) &&
       (commit_params_->frame_policy.sandbox_flags &
        WebSandboxFlags::kTopNavigation) == WebSandboxFlags::kNone);
  const bool is_same_origin_to_top =
      origin_to_commit ==
      GetRenderFrameHost()->GetMainFrame()->GetLastCommittedOrigin();
  if (is_same_origin_to_top) {
    policy_container_builder_->SetAllowTopNavigationWithoutUserGesture(true);
  } else if (!IsInMainFrame() && !embedder_allows_top_navigation_explicitly) {
    policy_container_builder_->SetAllowTopNavigationWithoutUserGesture(false);
  }

  if (!IsSameDocument()) {
    commit_params_->navigation_api_history_entry_arrays =
        GetNavigationController()->GetNavigationApiHistoryEntryVectors(
            frame_tree_node_, this);
    PopulateDocumentTokenForCrossDocumentNavigation();
  }

  if (early_hints_manager_) {
    commit_params_->early_hints_preloaded_resources =
        early_hints_manager_->TakePreloadedResourceURLs();
  }

  if (response_head_) {
    commit_params_->navigation_delivery_type =
        response_head_->navigation_delivery_type;
  }

  // Add our map of modified blink runtime-enabled features to
  // the commit params so they can be communicated to the renderer process.
  commit_params_->modified_runtime_features =
      runtime_feature_state_context_.GetFeatureOverrides();

  // Documents loaded from fenced frame configs can opt into allowing
  // cross-origin subframes to use their reporting metadata to send
  // `reportEvent()` beacons. The cross-origin subframes still require a
  // separate per-report opt-in.
  if (fenced_frame_properties_.has_value() && response_head_ &&
      response_head_->parsed_headers->allow_cross_origin_event_reporting) {
    fenced_frame_properties_->SetAllowCrossOriginEventReporting();
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kFencedFramesSrcPermissionsPolicy)) {
    std::optional<url::Origin> mapped_origin;
    if (fenced_frame_properties_.has_value()) {
      mapped_origin = url::Origin::Create(
          fenced_frame_properties_->mapped_url()->GetValueIgnoringVisibility());
    } else if (frame_tree_node_->HasFencedFrameProperties() &&
               frame_tree_node_->GetFencedFrameProperties()->mapped_url()) {
      mapped_origin =
          url::Origin::Create(frame_tree_node_->GetFencedFrameProperties()
                                  ->mapped_url()
                                  ->GetValueIgnoringVisibility());
    }

    // Container policy allowlists are first calculated by the embedder where
    // origin of the fenced frame is opaque. Now that the mapped URL is known,
    // update the container policy allowlists so that any allowlist with
    // "matches_opaque_src=true" points to the final mapped origin. This will be
    // the container policy that is sent to the inner root to construct the
    // final permissions policy.
    if (mapped_origin.has_value()) {
      for (auto& declaration : commit_params_->frame_policy.container_policy) {
        if (declaration.matches_opaque_src) {
          CHECK(!declaration.self_if_matches.has_value());
          declaration.matches_opaque_src = false;
          declaration.self_if_matches = mapped_origin.value();
        }
      }
    }
  }

  // Create a view of the fenced frame properties from the perspective of the
  // fenced frame content, which will be sent to its renderer.
  // On each navigation commit within the fenced frame tree:
  // * If the properties have no mapped url, the browser will send the renderer
  //   the `RedactedFencedFrameProperties` unconditionally.
  // * If the properties do have a mapped url, the browser will send the
  //   renderer the `RedactedFencedFrameProperties`, redacting extra information
  //   based on whether the origin is same-origin to the urn's mapped_url (after
  //   redirects).
  // This is because we want to make fenced frame APIs available only
  // in same-origin contexts, when "same-origin" has a coherent definition.
  const auto& computed_fenced_frame_properties = ComputeFencedFrameProperties();
  if (computed_fenced_frame_properties.has_value()) {
    content::FencedFrameEntity entity =
        content::FencedFrameEntity::kSameOriginContent;
    if (computed_fenced_frame_properties->mapped_url().has_value() &&
        !origin_to_commit.IsSameOriginWith(
            computed_fenced_frame_properties->mapped_url()
                ->GetValueIgnoringVisibility())) {
      entity = content::FencedFrameEntity::kCrossOriginContent;
    }
    commit_params_->fenced_frame_properties =
        computed_fenced_frame_properties->RedactFor(entity);
  }

  commit_params_->load_with_storage_access = ShouldLoadWithStorageAccess(
      begin_params(), common_params(), frame_tree_node()->current_frame_host(),
      did_encounter_cross_origin_redirect(), GetURL(), response());

  auto common_params = common_params_->Clone();
  auto commit_params = commit_params_.Clone();
  auto response_head = response_head_.Clone();
  if (!subresource_loader_params_.prefetched_signed_exchanges.empty()) {
    commit_params->prefetched_signed_exchanges =
        std::move(subresource_loader_params_.prefetched_signed_exchanges);
  }

  // TODO(https://crbug.com/40095391): Convert to CHECK if it proves to be
  // consistently upheld condition.
  DUMP_WILL_BE_CHECK(commit_params->redirect_response.size() ==
                     commit_params->redirect_infos.size());
  if (base::FeatureList::IsEnabled(kSanitizeRedirectUrlsDuringNavigation)) {
    // Before sending the commit parameters to the renderer process, sanitize
    // the redirect URLs to avoid leaking pontentially sensitive data into
    // processes which are cross-site. There is no dependency on the
    // cross-site-ness, therefore just sanitize unilaterally.
    for (auto redirect : commit_params->redirect_infos) {
      redirect.new_url = redirect.new_url.DeprecatedGetOriginAsURL();
    }
    for (auto redirect : commit_params->redirects) {
      redirect = redirect.DeprecatedGetOriginAsURL();
    }
  }

  GetRenderFrameHost()->CommitNavigation(
      this, std::move(common_params), std::move(commit_params),
      std::move(response_head), std::move(response_body_),
      std::move(url_loader_client_endpoints_), std::move(controller),
      std::move(subresource_overrides_),
      std::move(service_worker_container_info), document_token_,
      devtools_navigation_token_);
  if (service_worker_handle_ &&
      service_worker_handle_->service_worker_client()) {
    service_worker_handle_->service_worker_client()->SetContainerReady();
  }
  UpdateNavigationHandleTimingsOnCommitSent();

  // Give SpareRenderProcessHostManager a heads-up about the most recently used
  // BrowserContext.  This is mostly needed to make sure the spare is warmed-up
  // if it wasn't done in RenderProcessHostImpl::GetProcessHostForSiteInstance.
  RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedSiteInstance(
      GetRenderFrameHost()->GetSiteInstance());

  SendDeferredConsoleMessages();
}

void NavigationRequest::CommitPageActivation() {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::CommitPageActivation",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // An activation is either for the back-forward cache or prerendering. They
  // are mutually exclusive.
  DCHECK_NE(IsServedFromBackForwardCache(), IsPrerenderedPageActivation());

  NavigationControllerImpl* controller = GetNavigationController();

  if (IsServedFromBackForwardCache()) {
    std::unique_ptr<BackForwardCacheImpl::Entry> activated_entry;
    // Navigations served from the back-forward cache must be a history
    // navigation, and thus should have a valid |pending_history_list_offset|
    // value. We will pass that value and the |current_history_list_length|
    // value to update the history offset and length information saved in the
    // renderer, which might be stale.
    DCHECK_GE(commit_params_->pending_history_list_offset, 0);

    auto page_restore_params = blink::mojom::PageRestoreParams::New();
    page_restore_params->navigation_start = NavigationStart();
    page_restore_params->pending_history_list_offset =
        commit_params_->pending_history_list_offset;
    page_restore_params->current_history_list_length =
        commit_params_->current_history_list_length;
    activated_entry = controller->GetBackForwardCache().RestoreEntry(
        nav_entry_id_, std::move(page_restore_params));
    CHECK(activated_entry);

    // Restore navigation API entries, since they will probably have changed
    // since the page entered bfcache. We must update all frames, not just the
    // top frame, because it is possible (though unlikely) that an iframe's
    // entries have changed, too.
    activated_entry->render_frame_host()->ForEachRenderFrameHostWithAction(
        [this, &activated_entry](RenderFrameHostImpl* rfh) {
          // |this| is given as a parameter to
          // GetNavigationApiHistoryEntryVectors() only for the frame being
          // committed (i.e., the top frame).
          auto entry_arrays =
              rfh->frame_tree()
                  ->controller()
                  .GetNavigationApiHistoryEntryVectors(
                      rfh->frame_tree_node(),
                      activated_entry->render_frame_host() == rfh ? this
                                                                  : nullptr);
          rfh->GetAssociatedLocalFrame()
              ->SetNavigationApiHistoryEntriesForRestore(
                  std::move(entry_arrays),
                  blink::mojom::NavigationApiEntryRestoreReason::kBFCache);
          return RenderFrameHost::FrameIterationAction::kContinue;
        });

    // When activating from BackForwardCache, properly set the
    // BrowsingContextGroupSwap information. This is required because we
    // otherwise do it in the RenderFrameHost selection, in
    // GetFrameHostForNavigation, which does not happen for BFCache restores.
    // TODO(crbug.com/40922919): This code assumes that pages can only be
    // stored in the BFCache if they live in a different BrowsingInstance in
    // another CoopRelatedGroup, so we enforce that invariant via a CHECK. If
    // this is not the case anymore, `browsing_context_group_swap_` should be
    // set to BrowsingContextGroupSwap::CreateRelatedCoopSwap() if the two
    // SiteInstances live in the same CoopRelatedGroup.
    SiteInstanceImpl* current_site_instance =
        frame_tree_node_->current_frame_host()->GetSiteInstance();
    SiteInstanceImpl* target_site_instance =
        activated_entry->render_frame_host()->GetSiteInstance();
    CHECK(!target_site_instance->IsCoopRelatedSiteInstance(
        current_site_instance));
    browsing_context_group_swap_ =
        BrowsingContextGroupSwap::CreateSecuritySwap();

    base::WeakPtr<NavigationRequest> weak_self(weak_factory_.GetWeakPtr());
    ReadyToCommitNavigation(false /* is_error */);
    // The call above might block on showing a user dialog. The interaction of
    // the user with this dialog might result in the WebContents owning this
    // NavigationRequest to be destroyed. Return if this is the case.
    if (!weak_self)
      return;

    // Use std::exchange instead of move, so that we clear out the optional on
    // the commit_params.
    activated_entry->SetViewTransitionState(
        std::exchange(commit_params_->view_transition_state, {}));

    // Move the BackForwardCacheImpl::Entry into RenderFrameHostManager, in
    // preparation for committing. This entry may be either restored from the
    // backforward cache.
    DCHECK(activated_entry);
    frame_tree_node_->render_manager()->RestorePage(
        activated_entry->TakeStoredPage());
  } else {
    // Copy the prerender trigger type before PrerenderHost is destroyed in
    // ActivateReservedHost().
    PreloadingTriggerType trigger_type =
        GetPrerenderHostRegistry().GetPrerenderTriggerType(
            prerender_frame_tree_node_id());
    const std::string embedder_histogram_suffix =
        GetPrerenderHostRegistry().GetPrerenderEmbedderHistogramSuffix(
            prerender_frame_tree_node_id());

    std::unique_ptr<StoredPage> stored_page =
        GetPrerenderHostRegistry().ActivateReservedHost(
            prerender_frame_tree_node_id_.value(), *this);
    CHECK(stored_page);

    RenderFrameHostImpl* rfh = stored_page->render_frame_host();

    // Set the prerender trigger type and embedder histogram suffix for metrics.
    set_prerender_trigger_type(trigger_type);
    set_prerender_embedder_histogram_suffix(embedder_histogram_suffix);

    // The prerender page might have navigated. Update the URL and the redirect
    // chain, as the prerendered page might have been redirected or performed
    // a same-document navigation.
    // TODO(crbug.com/40170496): Ensure that the tests that navigate
    // MPArch activation flow do not crash. This is a hack to unblock the basic
    // MPArch activation flow for now. There are probably other parameters which
    // are out of sync, and we need to carefully think through how we can
    // activate a RenderFrameHost whose URL doesn't match the one that was
    // initially passed to NavigationRequest (or disallow subsequent navigations
    // in the main frame of the prerender frame tree).
    common_params_->url = rfh->GetLastCommittedURL();
    // TODO(crbug.com/40170496): We may have to add the entire redirect
    // chain.
    redirect_chain_.clear();
    redirect_chain_.push_back(rfh->GetLastCommittedURL());

    base::WeakPtr<NavigationRequest> weak_self(weak_factory_.GetWeakPtr());
    ReadyToCommitNavigation(false /* is_error */);
    // The call above might block on showing a user dialog. The interaction of
    // the user with this dialog might result in the WebContents owning this
    // NavigationRequest to be destroyed. Return if this is the case.
    if (!weak_self)
      return;

    // Use std::exchange instead of move, so that we clear out the optional on
    // the commit_params.
    stored_page->SetViewTransitionState(
        std::exchange(commit_params_->view_transition_state, {}));

    // Update navigation API entries. A prerendered page has only a single
    // history entry, but now it has access to a full back/forward list.
    rfh->ForEachRenderFrameHostWithAction([this, &stored_page](
                                              RenderFrameHostImpl* rfh) {
      // Currently, prerender activation only happens for DIFFERENT_DOCUMENT
      // navigations. If that ever changes, `reason` calculation will need to be
      // updated (and new NavigationApiEntryRestoreReason values added).
      DCHECK_EQ(common_params_->navigation_type,
                blink::mojom::NavigationType::DIFFERENT_DOCUMENT);
      blink::mojom::NavigationApiEntryRestoreReason reason =
          common_params_->should_replace_current_entry
              ? blink::mojom::NavigationApiEntryRestoreReason::
                    kPrerenderActivationReplace
              : blink::mojom::NavigationApiEntryRestoreReason::
                    kPrerenderActivationPush;
      // |this| is given as a parameter to
      // GetNavigationApiHistoryEntryVectors() only for the frame being
      // committed (i.e., the top frame).
      auto entry_arrays =
          rfh->frame_tree()->controller().GetNavigationApiHistoryEntryVectors(
              rfh->frame_tree_node(),
              stored_page->render_frame_host() == rfh ? this : nullptr);
      rfh->GetAssociatedLocalFrame()->SetNavigationApiHistoryEntriesForRestore(
          std::move(entry_arrays), reason);
      return RenderFrameHost::FrameIterationAction::kContinue;
    });

    // Move the StoredPage into RenderFrameHostManager, in
    // preparation for committing. This entry may be used for prerendering.
    frame_tree_node_->render_manager()->ActivatePrerender(
        std::move(stored_page));
  }

  // Commit the page activation. This includes committing the RenderFrameHost
  // and restoring extra state, such as proxies, etc.
  // Note that this will delete the NavigationRequest.
  GetRenderFrameHost()->DidCommitPageActivation(
      this, IsPrerenderedPageActivation()
                ? MakeDidCommitProvisionalLoadParamsForPrerenderActivation()
                : MakeDidCommitProvisionalLoadParamsForBFCacheRestore());
}

void NavigationRequest::SetExpectedProcess(
    RenderProcessHost* expected_process) {
  if (expected_process &&
      expected_process->GetID() == expected_render_process_host_id_) {
    // This |expected_process| has already been informed of the navigation,
    // no need to update it again.
    return;
  }

  ResetExpectedProcess();

  if (expected_process == nullptr)
    return;

  // Keep track of the speculative RenderProcessHost and tell it to expect a
  // navigation to |site_info_|.
  expected_render_process_host_id_ = expected_process->GetID();
  expected_process->AddObserver(this);
  RenderProcessHostImpl::AddExpectedNavigationToSite(
      frame_tree_node()->navigator().controller().GetBrowserContext(),
      expected_process, site_info_);
}

void NavigationRequest::SetExpectedProcessIfAssociated() {
  if (associated_rfh_type_ != AssociatedRenderFrameHostType::NONE) {
    RenderFrameHostImpl* navigating_frame_host =
        associated_rfh_type_ == AssociatedRenderFrameHostType::SPECULATIVE
            ? frame_tree_node_->render_manager()->speculative_frame_host()
            : frame_tree_node_->current_frame_host();
    SetExpectedProcess(navigating_frame_host->GetProcess());
  }
}

void NavigationRequest::ResetExpectedProcess() {
  if (expected_render_process_host_id_ == ChildProcessHost::kInvalidUniqueID) {
    // No expected process is set, nothing to update.
    return;
  }
  RenderProcessHost* process =
      RenderProcessHost::FromID(expected_render_process_host_id_);
  if (process) {
    RenderProcessHostImpl::RemoveExpectedNavigationToSite(
        frame_tree_node()->navigator().controller().GetBrowserContext(),
        process, site_info_);
    process->RemoveObserver(this);
  }
  expected_render_process_host_id_ = ChildProcessHost::kInvalidUniqueID;
}

void NavigationRequest::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(host->GetID(), expected_render_process_host_id_);
  ResetExpectedProcess();
}

void NavigationRequest::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {}

void NavigationRequest::UpdateNavigationHandleTimingsOnResponseReceived(
    bool is_redirect,
    bool is_first_response) {
  base::TimeTicks loader_callback_time = base::TimeTicks::Now();

  const base::TimeDelta domain_lookup_delay =
      response_head_->load_timing.connect_timing.domain_lookup_end -
      response_head_->load_timing.connect_timing.domain_lookup_start;
  const base::TimeDelta connect_delay =
      response_head_->load_timing.connect_timing.connect_end -
      response_head_->load_timing.connect_timing.connect_start;
  const base::TimeDelta ssl_delay =
      response_head_->load_timing.connect_timing.ssl_end -
      response_head_->load_timing.connect_timing.ssl_start;

  if (is_first_response) {
    DCHECK(navigation_handle_timing_.first_request_start_time.is_null());
    DCHECK(navigation_handle_timing_.first_response_start_time.is_null());
    DCHECK(navigation_handle_timing_.first_loader_callback_time.is_null());
    navigation_handle_timing_.first_request_start_time =
        response_head_->load_timing.send_start;
    navigation_handle_timing_.first_response_start_time =
        response_head_->load_timing.receive_headers_start;
    navigation_handle_timing_.first_loader_callback_time = loader_callback_time;

    navigation_handle_timing_.first_request_domain_lookup_delay =
        domain_lookup_delay;
    navigation_handle_timing_.first_request_connect_delay = connect_delay;
    navigation_handle_timing_.first_request_ssl_delay = ssl_delay;

    first_fetch_start_time_ = response_head_->request_start;
  }

  if (!is_redirect) {
    navigation_handle_timing_.non_redirected_request_start_time =
        response_head_->load_timing.send_start;
    navigation_handle_timing_.non_redirect_response_start_time =
        response_head_->load_timing.receive_headers_start;
    navigation_handle_timing_.non_redirect_response_loader_callback_time =
        loader_callback_time;
  }

  navigation_handle_timing_.final_request_start_time =
      response_head_->load_timing.send_start;
  navigation_handle_timing_.final_response_start_time =
      response_head_->load_timing.receive_headers_start;
  navigation_handle_timing_.final_non_informational_response_start_time =
      response_head_->load_timing.receive_non_informational_headers_start;
  navigation_handle_timing_.final_loader_callback_time = loader_callback_time;
  navigation_handle_timing_.final_request_domain_lookup_delay =
      domain_lookup_delay;
  navigation_handle_timing_.final_request_connect_delay = connect_delay;
  navigation_handle_timing_.final_request_ssl_delay = ssl_delay;
  final_receive_headers_end_time_ =
      response_head_->load_timing.receive_headers_end;

  // |navigation_commit_sent_time| will be updated by
  // UpdateNavigationHandleTimingsOnCommitSent() later.
  DCHECK(navigation_handle_timing_.navigation_commit_sent_time.is_null());

  GetDelegate()->DidUpdateNavigationHandleTiming(this);
}

void NavigationRequest::UpdateNavigationHandleTimingsOnCommitSent() {
  DCHECK(navigation_handle_timing_.navigation_commit_sent_time.is_null());
  navigation_handle_timing_.navigation_commit_sent_time =
      base::TimeTicks::Now();

  GetDelegate()->DidUpdateNavigationHandleTiming(this);
}

void NavigationRequest::UpdateSiteInfo(
    RenderProcessHost* post_redirect_process) {
  int post_redirect_process_id = post_redirect_process
                                     ? post_redirect_process->GetID()
                                     : ChildProcessHost::kInvalidUniqueID;

  SiteInfo new_site_info = GetSiteInfoForCommonParamsURL();
  if (new_site_info == site_info_ &&
      post_redirect_process_id == expected_render_process_host_id_) {
    return;
  }

  // Stop expecting a navigation to the current SiteInfo in the current expected
  // process.
  ResetExpectedProcess();

  // Update the SiteInfo and the expected process.
  site_info_ = new_site_info;
  SetExpectedProcess(post_redirect_process);
}

bool NavigationRequest::IsAllowedByCSPDirective(
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& policies,
    network::CSPContext* context,
    network::mojom::CSPDirectiveName directive,
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_opaque_fenced_frame,
    network::CSPContext::CheckCSPDisposition disposition) {
  GURL url;
  // If this request was upgraded in the net stack, downgrade the URL back to
  // HTTP before checking report only policies.
  if (url_upgraded_after_redirect &&
      disposition ==
          network::CSPContext::CheckCSPDisposition::CHECK_REPORT_ONLY_CSP &&
      common_params_->url.SchemeIs(url::kHttpsScheme)) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(url::kHttpScheme);
    url = common_params_->url.ReplaceComponents(replacements);
  } else {
    url = common_params_->url;
  }
  network::CSPCheckResult result = context->IsAllowedByCsp(
      policies, directive, url, commit_params_->original_url,
      has_followed_redirect, common_params_->source_location, disposition,
      begin_params_->is_form_submission, is_opaque_fenced_frame);
  if (result.WouldBlockIfWildcardDoesNotMatchWs()) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        GetParentFrame(),
        blink::mojom::WebFeature::kCspWouldBlockIfWildcardDoesNotMatchWs);
  }
  if (result.WouldBlockIfWildcardDoesNotMatchFtp()) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        GetParentFrame(),
        blink::mojom::WebFeature::kCspWouldBlockIfWildcardDoesNotMatchFtp);
  }
  return result.IsAllowed();
}

net::Error NavigationRequest::CheckCSPDirectives(
    RenderFrameHostCSPContext parent_context,
    const PolicyContainerPolicies* parent_policies,
    RenderFrameHostCSPContext initiator_context,
    const PolicyContainerPolicies* initiator_policies,
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check,
    network::CSPContext::CheckCSPDisposition disposition) {
  // Following directive checks' order is important as the `error` code takes
  // only the result last set.
  net::Error error = net::OK;

  if (initiator_policies) {
    // [form-action]
    if (begin_params_->is_form_submission && !is_response_check &&
        !IsAllowedByCSPDirective(
            initiator_policies->content_security_policies, &initiator_context,
            network::mojom::CSPDirectiveName::FormAction, has_followed_redirect,
            url_upgraded_after_redirect,
            /*is_opaque_fenced_frame=*/false, disposition)) {
      // net::ERR_ABORTED is used instead of net::ERR_BLOCKED_BY_CSP. This is
      // a better user experience as the user is not presented with an error
      // page. However if other CSP directives like frame-src are violated, it
      // may be appropriate for them to use ERR_BLOCKED_BY_CSP so this can be
      // overridden by the checks below.
      error = net::ERR_ABORTED;
    }
  }

  // [frame-src] or [fenced-frame-src]
  if (parent_policies) {
    bool is_opaque_fenced_frame_root_navigation =
        frame_tree_node_->IsFencedFrameRoot() &&
        fenced_frame_properties_.has_value() &&
        fenced_frame_properties_->mapped_url().has_value() &&
        !fenced_frame_properties_->mapped_url()
             ->GetValueForEntity(FencedFrameEntity::kEmbedder)
             .has_value();
    if (!IsAllowedByCSPDirective(
            parent_policies->content_security_policies, &parent_context,
            frame_tree_node_->IsFencedFrameRoot()
                ? network::mojom::CSPDirectiveName::FencedFrameSrc
                : network::mojom::CSPDirectiveName::FrameSrc,
            has_followed_redirect, url_upgraded_after_redirect,
            is_opaque_fenced_frame_root_navigation, disposition)) {
      error = net::ERR_BLOCKED_BY_CSP;
    }
  }

  return error;
}

net::Error NavigationRequest::CheckContentSecurityPolicy(
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check) {
  DCHECK(policy_container_builder_.has_value());
  if (common_params_->url.SchemeIs(url::kAboutScheme))
    return net::OK;

  if (IsSameDocument())
    return net::OK;

  if (common_params_->should_check_main_world_csp ==
      network::mojom::CSPDisposition::DO_NOT_CHECK) {
    return net::OK;
  }

  RenderFrameHostImpl* parent = frame_tree_node()->parent();
  const PolicyContainerPolicies* parent_policies =
      policy_container_builder_->ParentPolicies();
  DCHECK(!parent == !parent_policies);
  bool set_parent_for_nested_frame_tree =
      !parent && frame_tree_node()->IsFencedFrameRoot() &&
      frame_tree_node()->render_manager()->GetOuterDelegateNode();
  if (set_parent_for_nested_frame_tree) {
    parent = frame_tree_node()
                 ->render_manager()
                 ->GetOuterDelegateNode()
                 ->current_frame_host()
                 ->GetParent();
    // TODO(antoniosartori): If we want to keep checking frame-src for fenced
    // frames, consider storing a snapshot of the parent policies in the
    // `policy_container_builder_` at the beginning of the navigation.
    parent_policies = &parent->policy_container_host()->policies();
  }

  const PolicyContainerPolicies* initiator_policies =
      policy_container_builder_->InitiatorPolicies();

  // CSP checking happens in three phases, per steps 3-5 of
  // https://fetch.spec.whatwg.org/#main-fetch:
  //
  // (1) Check report-only policies and trigger reports for any violations.
  // (2) Upgrade the request to HTTPS if necessary.
  // (3) Check enforced policies (triggering reports for any violations of those
  //     policies) and block the request if necessary.
  //
  // This sequence of events allows site owners to learn about (via step 1) any
  // requests that are upgraded in step 2.

  RenderFrameHostCSPContext parent_context(parent);

  // Note: the initiator RenderFrameHost could have been deleted by
  // now. Then this RenderFrameHostCSPContext will do nothing and we won't
  // report violations for this check.
  //
  // If the initiator frame has navigated away in between, we also use a no-op
  // `initiator_csp_context`, in order not to trigger `securitypolicyviolation`
  // events in the wrong document.
  RenderFrameHostCSPContext initiator_context(
      GetInitiatorDocumentRenderFrameHost());

  net::Error report_only_csp_status = CheckCSPDirectives(
      parent_context, parent_policies, initiator_context, initiator_policies,
      has_followed_redirect, url_upgraded_after_redirect, is_response_check,
      network::CSPContext::CHECK_REPORT_ONLY_CSP);

  // upgrade-insecure-requests is handled in the network code for redirects,
  // only do the upgrade here if this is not a redirect.
  // Note that `FrameTreeNode::IsMainFrame()` returns true for fenced frames
  // based on MPArch, but it's fine to skip the logic below as
  // `network::UpgradeInsecureRequest()` does not apply to fenced frame
  // navigation requests. (See https://github.com/WICG/fenced-frame/issues/23)
  if (!has_followed_redirect && !frame_tree_node()->IsMainFrame()) {
    DCHECK(parent_policies);
    if (parent_policies && network::ShouldUpgradeInsecureRequest(
                               parent_policies->content_security_policies)) {
      upgrade_if_insecure_ = true;
      network::UpgradeInsecureRequest(&common_params_->url);
      common_params_->referrer = Referrer::SanitizeForRequest(
          common_params_->url, *common_params_->referrer);
      commit_params_->original_url = common_params_->url;
    }
  }

  net::Error enforced_csp_status = CheckCSPDirectives(
      parent_context, parent_policies, initiator_context, initiator_policies,
      has_followed_redirect, url_upgraded_after_redirect, is_response_check,
      network::CSPContext::CHECK_ENFORCED_CSP);
  if (enforced_csp_status != net::OK)
    return enforced_csp_status;
  return report_only_csp_status;
}

NavigationRequest::CredentialedSubresourceCheckResult
NavigationRequest::CheckCredentialedSubresource() const {
  // It only applies to subframes.
  if (frame_tree_node_->IsOutermostMainFrame())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // URLs with no embedded credentials should load correctly.
  if (!common_params_->url.has_username() &&
      !common_params_->url.has_password())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // Relative URLs on top-level pages that were loaded with embedded credentials
  // should load correctly.
  RenderFrameHostImpl* parent = frame_tree_node_->GetParentOrOuterDocument();
  DCHECK(parent);
  const GURL& parent_url = parent->GetLastCommittedURL();
  if (url::IsSameOriginWith(parent_url, common_params_->url) &&
      parent_url.username() == common_params_->url.username() &&
      parent_url.password() == common_params_->url.password()) {
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;
  }

  // Warn the user about the request being blocked.
  const char* console_message =
      "Subresource requests whose URLs contain embedded credentials (e.g. "
      "`https://user:pass@host/`) are blocked. See "
      "https://www.chromestatus.com/feature/5669008342777856 for more "
      "details.";
  parent->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                              console_message);
  return CredentialedSubresourceCheckResult::BLOCK_REQUEST;
}

NavigationRequest::AboutSrcDocCheckResult NavigationRequest::CheckAboutSrcDoc()
    const {
  if (!common_params_->url.IsAboutSrcdoc())
    return AboutSrcDocCheckResult::ALLOW_REQUEST;

  // Loading about:srcdoc in the main frame can't have any reasonable meaning.
  // There might be a malicious website trying to exploit a bug from this. As a
  // defensive measure, do not proceed. They would have failed anyway later.
  if (frame_tree_node_->IsMainFrame())
    return AboutSrcDocCheckResult::BLOCK_REQUEST;

  // There are 4 cases where we allow a navigation to about:srcdoc:

  // 1) We allow same-document navigations from any frame.
  if (IsSameDocument()) {
    return AboutSrcDocCheckResult::ALLOW_REQUEST;
  }

  const std::optional<url::Origin>& initiator_origin =
      common_params().initiator_origin;
  // 2) Browser-initiated navigations are (temporarily) allowed for
  // about:srcdoc.
  if (!initiator_origin) {
    // TODO(https://crbug.com/40165505): for now, allow this, and land the
    // change to block it in a separate CL in case it breaks things beyond our
    // local test suites.
    return AboutSrcDocCheckResult::ALLOW_REQUEST;
  }

  // 3) An about:srcdoc frame can reload itself (even if it is cross-origin from
  // its parent due to being sandboxed).
  if (frame_tree_node()
          ->current_frame_host()
          ->GetLastCommittedURL()
          .IsAboutSrcdoc() &&
      initiator_origin->IsSameOriginWith(
          frame_tree_node()->current_frame_host()->GetLastCommittedOrigin())) {
    return AboutSrcDocCheckResult::ALLOW_REQUEST;
  }

  // 4) Setting src = 'about:srcdoc' is allowed for now as long as the
  // initiator's origin matches the origin of the srcdoc's parent. It is
  // important not to allow initiators that are cross-origin with the parent,
  // because the content comes from the parent and many places in the code
  // assume the origin comes from the initiator.
  // TODO(https://crbug.com/40165505): navigations to 'about:srcdoc' aren't
  // supposed to ever be allowed according to spec.
  if (*initiator_origin ==
      frame_tree_node()->parent()->GetLastCommittedOrigin()) {
    return AboutSrcDocCheckResult::ALLOW_REQUEST;
  }

  // Navigations with an initiator that is cross-origin to the about:srcdoc
  // parent are not allowed.
  return AboutSrcDocCheckResult::BLOCK_REQUEST;
}

void NavigationRequest::SetupCSPEmbeddedEnforcement() {
  if (IsInMainFrame())
    return;
  // TODO(https://crbug.com/11129645): MHTML iframe not supported yet.
  if (IsForMhtmlSubframe())
    return;

  // TODO(antoniosartori): Probably we should have taken a snapshot of the 'csp'
  // attribute at the beginning of the navigation and not now, since the
  // beforeunload handlers might have modified it in the meantime.
  // See pull request about the spec:
  // https://github.com/w3c/webappsec-cspee/pull/11
  network::mojom::ContentSecurityPolicyPtr frame_csp_attribute =
      frame_tree_node()->csp_attribute()
          ? frame_tree_node()->csp_attribute()->Clone()
          : nullptr;
  if (frame_csp_attribute) {
    // TODO(antoniosartori): Maybe we should revisit what 'self' means in the
    // 'csp' attribute.
    const GURL& url = GetURL();
    frame_csp_attribute->self_origin = network::mojom::CSPSource::New(
        url.scheme(), url.host(), url.EffectiveIntPort(), "", false, false);
  }

  const network::mojom::ContentSecurityPolicy* parent_required_csp =
      frame_tree_node()->parent()->required_csp();

  std::vector<network::mojom::ContentSecurityPolicyPtr> frame_csp;
  frame_csp.push_back(std::move(frame_csp_attribute));
  std::string error_message;
  if (network::IsValidRequiredCSPAttr(frame_csp, parent_required_csp,
                                      error_message)) {
    // If |frame_csp| is valid then it is not null.
    SetRequiredCSP(std::move(frame_csp[0]));
    return;
  }

  if (frame_csp[0]) {
    GetParentFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf("The frame 'csp' attribute ('%s') is invalid and "
                           "will be discarded: %s",
                           frame_csp[0]->header->header_value.c_str(),
                           error_message.c_str()));
  }

  if (parent_required_csp) {
    SetRequiredCSP(parent_required_csp->Clone());
  }
  // TODO(antoniosartori): Consider instead blocking the navigation here,
  // since this seems to be insecure
  // (cf. https://github.com/w3c/webappsec-cspee/pull/11).
}

NavigationRequest::CSPEmbeddedEnforcementResult
NavigationRequest::CheckCSPEmbeddedEnforcement() {
  // We enforce CSPEE only for subframes.
  if (IsInMainFrame())
    return CSPEmbeddedEnforcementResult::ALLOW_RESPONSE;

  if (IsSameDocument())
    return CSPEmbeddedEnforcementResult::ALLOW_RESPONSE;

  if (!required_csp_)
    return CSPEmbeddedEnforcementResult::ALLOW_RESPONSE;

  // The |response()| can be null for navigations that do not require a
  // URLLoader (about:blank, about:srcdoc, ...)
  const network::mojom::AllowCSPFromHeaderValue* allow_csp_from =
      response() ? response()->parsed_headers->allow_csp_from.get() : nullptr;

  if (network::AllowsBlanketEnforcementOfRequiredCSP(
          GetParentFrame()->GetLastCommittedOrigin(), GetURL(), allow_csp_from,
          required_csp_)) {
    // Enforce the required CSPs on the frame by passing them down to blink.
    policy_container_builder_->AddContentSecurityPolicy(required_csp_->Clone());
    return CSPEmbeddedEnforcementResult::ALLOW_RESPONSE;
  }

  // All the URLs that do not |NeedsUrlLoader()| allows blanket enforcement of
  // CSP, Except for MHTML iframe.
  // TODO(arthursonzogni): Make MHTML response to use the normal loading path,
  // by introducing their own MHTML UrlLoader. Then CSPEE can be supported.
  if (!response()) {
    // TODO(https://crbug.com/11129645): Remove MHTML edge case, once MHTML
    // documents are handled through the standard code path with its own
    // URLLoaderFactory.
    CHECK(IsForMhtmlSubframe());
    return CSPEmbeddedEnforcementResult::ALLOW_RESPONSE;
  }

  std::string sanitized_blocked_url =
      GetRedirectChain().front().DeprecatedGetOriginAsURL().spec();
  if (allow_csp_from && allow_csp_from->is_error_message()) {
    AddDeferredConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf("The value of the 'Allow-CSP-From' response header "
                           "returned by %s is invalid: %s",
                           sanitized_blocked_url.c_str(),
                           allow_csp_from->get_error_message().c_str()));
  }

  if (network::Subsumes(*required_csp_,
                        response()->parsed_headers->content_security_policy)) {
    return CSPEmbeddedEnforcementResult::ALLOW_RESPONSE;
  }

  AddDeferredConsoleMessage(
      blink::mojom::ConsoleMessageLevel::kError,
      base::StringPrintf(
          "Refused to display '%s' in a frame. The embedder requires it to "
          "enforce the following Content Security Policy: '%s'. However, the "
          "frame neither accepts that policy using the Allow-CSP-From header "
          "nor delivers a Content Security Policy which is at least as strong "
          "as that one.",
          sanitized_blocked_url.c_str(),
          required_csp_->header->header_value.c_str()));

  return CSPEmbeddedEnforcementResult::BLOCK_RESPONSE;
}

void NavigationRequest::UpdateHistoryParamsInCommitNavigationParams() {
  NavigationController& navigation_controller =
      frame_tree_node_->navigator().controller();
  commit_params_->current_history_list_offset =
      navigation_controller.GetCurrentEntryIndex();
  commit_params_->current_history_list_length =
      navigation_controller.GetEntryCount();
}

void NavigationRequest::RendererRequestedNavigationCancellationForTesting() {
  OnNavigationClientDisconnected(0, "");
}

void NavigationRequest::OnNavigationClientDisconnected(
    uint32_t reason,
    const std::string& description) {
  // Renderer-initiated navigation cancellations can only happen before the
  // navigation gets into the READY_TO_COMMIT state, because
  // RendererCancellationThrottle will prevent renderer-initiated navigations
  // from entering that state before the JS task that started the navigation
  // finishes. After navigation reaches READY_TO_COMMIT stage, we should
  // ignore these navigation cancellations, except for these two cases:
  // 1. It reuses the current RenderFrame(Host), because the RenderFrame expects
  // the navigation to be cancelled successfully (as the state in the renderer
  // is already updated to cancel the navigation).
  // TODO(crbug.com/40615943): This case will eventually go away with
  // RenderDocument as cross-document navigations won't reuse RenderFrameHosts
  // anymore. Fix tests that expect this behavior.
  // 2. The target renderer had crashed, so the speculative RenderFrame is not
  // live anymore, because the navigation can't commit in a crashed renderer.
  std::optional<NavigationDiscardReason> discard_reason;
  if (HasRenderFrameHost() && !GetRenderFrameHost()->IsRenderFrameLive()) {
    discard_reason = NavigationDiscardReason::kRenderProcessGone;
  } else {
    switch (static_cast<mojom::NavigationClientDisconnectReason>(reason)) {
      case mojom::NavigationClientDisconnectReason::kResetForSwap:
        // If the RenderFrame that initiated this navigation request is swapped
        // out (disconnecting its NavigationClient for this request), do not
        // treat it as a cancellation. Otherwise, if a previous navigation
        // before `this` is slow to commit, it would unexpectedly cancel `this`
        // subsequent attempt to navigate elsewhere.
        return;
      case mojom::NavigationClientDisconnectReason::kNoExplicitReason:
        discard_reason = NavigationDiscardReason::kInternalCancellation;
        break;
      case mojom::NavigationClientDisconnectReason::kResetForAbort:
        discard_reason = NavigationDiscardReason::kExplicitCancellation;
        break;
      case mojom::NavigationClientDisconnectReason::kResetForNewNavigation:
        discard_reason =
            NavigationDiscardReason::kNewOtherNavigationRendererInitiated;
        break;
      case mojom::NavigationClientDisconnectReason::
          kResetForDuplicateNavigation:
        discard_reason = NavigationDiscardReason::kNewDuplicateNavigation;
        break;
    }
    if (!discard_reason.has_value()) {
      // TODO(https://crbug.com/366060351): An invalid value was used. Kill
      // either the requesting or committing client's process.
      return;
    }
  }

  if (!IsWaitingToCommit()) {
    // The cancellation happens before READY_TO_COMMIT.
    frame_tree_node_->navigator().CancelNavigation(frame_tree_node_,
                                                   discard_reason.value());
  } else if (GetRenderFrameHost() ==
                 frame_tree_node_->render_manager()->current_frame_host() ||
             !GetRenderFrameHost()->IsRenderFrameLive()) {
    // If the NavigationRequest has already reached READY_TO_COMMIT,
    // `render_frame_host_` owns `this`. Cache any needed state in stack
    // variables to avoid a use-after-free.
    FrameTreeNode* frame_tree_node = frame_tree_node_;
    GetRenderFrameHost()->NavigationRequestCancelled(this,
                                                     discard_reason.value());
    // Ensure that the speculative RFH, if any, is also cleaned up.
    frame_tree_node->render_manager()->DiscardSpeculativeRFHIfUnused(
        discard_reason.value());
  }

  // Do not add code after this, NavigationRequest might have been destroyed.
}

void NavigationRequest::HandleInterfaceDisconnection(
    mojo::AssociatedRemote<mojom::NavigationClient>& navigation_client) {
  // `Unretained()` is safe because the `mojo::AssociatedRemote` reference only
  // refers to fields owned by `this`.
  navigation_client.set_disconnect_with_reason_handler(
      base::BindOnce(&NavigationRequest::OnNavigationClientDisconnected,
                     base::Unretained(this)));
}

void NavigationRequest::IgnoreInterfaceDisconnection() {
  return request_navigation_client_.set_disconnect_handler(base::DoNothing());
}

void NavigationRequest::IgnoreCommitInterfaceDisconnection() {
  return commit_navigation_client_.set_disconnect_handler(base::DoNothing());
}

bool NavigationRequest::IsSameDocument() const {
  return NavigationTypeUtils::IsSameDocument(common_params_->navigation_type);
}

bool NavigationRequest::IsHistory() const {
  return NavigationTypeUtils::IsHistory(common_params_->navigation_type);
}

bool NavigationRequest::IsRestore() const {
  return NavigationTypeUtils::IsRestore(common_params_->navigation_type);
}

bool NavigationRequest::IsReload() const {
  return NavigationTypeUtils::IsReload(common_params_->navigation_type);
}

void NavigationRequest::RecordDownloadUseCountersPrePolicyCheck() {
  RenderFrameHost* rfh = frame_tree_node_->current_frame_host();
  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      rfh, blink::mojom::WebFeature::kDownloadPrePolicyCheck);

  // Log UseCounters for opener navigations.
  if (download_policy().IsType(
          blink::NavigationDownloadType::kOpenerCrossOrigin)) {
    rfh->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kError,
        base::StringPrintf(
            "Navigating a cross-origin opener to a download (%s) is "
            "deprecated, see "
            "https://www.chromestatus.com/feature/5742188281462784.",
            common_params_->url.spec().c_str()));
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kOpenerNavigationDownloadCrossOrigin);
  }

  // Log UseCounters for download in sandbox.
  if (download_policy().IsType(blink::NavigationDownloadType::kSandbox)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInSandbox);
  }

  // Log UseCounters for download without user activation.
  if (download_policy().IsType(blink::NavigationDownloadType::kNoGesture)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadWithoutUserGesture);
  }

  // Log UseCounters for download in ad frame without user activation.
  if (download_policy().IsType(
          blink::NavigationDownloadType::kAdFrameNoGesture)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInAdFrameWithoutUserGesture);
  }

  // Log UseCounters for download in ad frame.
  if (download_policy().IsType(blink::NavigationDownloadType::kAdFrame)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInAdFrame);
  }
}

void NavigationRequest::RecordDownloadUseCountersPostPolicyCheck() {
  DCHECK(is_download_);
  RenderFrameHost* rfh = frame_tree_node_->current_frame_host();
  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      rfh, blink::mojom::WebFeature::kDownloadPostPolicyCheck);
}

void NavigationRequest::OnNavigationEventProcessed(
    NavigationThrottleRunner::Event event,
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_NE(NavigationThrottle::DEFER, result.action());
  switch (event) {
    case NavigationThrottleRunner::Event::kNoEvent:
      DUMP_WILL_BE_NOTREACHED();
      return;
    case NavigationThrottleRunner::Event::kWillStartRequest:
      OnWillStartRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::kWillRedirectRequest:
      OnWillRedirectRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::kWillFailRequest:
      OnWillFailRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::kWillProcessResponse:
      OnWillProcessResponseProcessed(result);
      return;
    case NavigationThrottleRunner::Event::kWillCommitWithoutUrlLoader:
      OnWillCommitWithoutUrlLoaderProcessed(result);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void NavigationRequest::OnWillStartRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(WILL_START_REQUEST, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  DCHECK(processing_navigation_throttle_);
  processing_navigation_throttle_ = false;
  if (result.action() != NavigationThrottle::PROCEED)
    SetState(CANCELING);

  if (complete_callback_for_testing_ &&
      std::move(complete_callback_for_testing_).Run(result)) {
    return;
  }

  if (MaybeEvictFromBackForwardCacheBySubframeNavigation(
          frame_tree_node_->current_frame_host())) {
    // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
    // deleted by the previous calls.
    return;
  }

  OnStartChecksComplete(result);

  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

void NavigationRequest::OnWillRedirectRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(WILL_REDIRECT_REQUEST, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  DCHECK(processing_navigation_throttle_);
  processing_navigation_throttle_ = false;
  if (result.action() == NavigationThrottle::PROCEED) {
    // Notify the delegate that a redirect was encountered and will be followed.
    if (GetDelegate()) {
#if DCHECK_IS_ON()
      DCHECK(is_safe_to_delete_);
      base::AutoReset<bool> resetter(&is_safe_to_delete_, false);
#endif
      GetDelegate()->DidRedirectNavigation(this);
    }
  } else {
    SetState(CANCELING);
  }

  if (complete_callback_for_testing_ &&
      std::move(complete_callback_for_testing_).Run(result)) {
    return;
  }
  OnRedirectChecksComplete(result);

  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

void NavigationRequest::OnWillFailRequestProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(WILL_FAIL_REQUEST, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_RESPONSE, result.action());
  DCHECK(processing_navigation_throttle_);
  processing_navigation_throttle_ = false;
  if (result.action() == NavigationThrottle::PROCEED) {
    result = NavigationThrottle::ThrottleCheckResult(
        NavigationThrottle::PROCEED, net_error_);
  } else {
    SetState(CANCELING);
  }

  if (complete_callback_for_testing_ &&
      std::move(complete_callback_for_testing_).Run(result)) {
    return;
  }
  OnFailureChecksComplete(result);

  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

void NavigationRequest::OnWillProcessResponseProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(WILL_PROCESS_RESPONSE, state_);
  DCHECK_NE(NavigationThrottle::BLOCK_REQUEST, result.action());
  DCHECK_NE(NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE, result.action());
  DCHECK(processing_navigation_throttle_);
  processing_navigation_throttle_ = false;
  if (result.action() != NavigationThrottle::PROCEED) {
    SetState(CANCELING);
  }

  if (complete_callback_for_testing_ &&
      std::move(complete_callback_for_testing_).Run(result)) {
    return;
  }
  OnWillProcessResponseChecksComplete(result);

  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

void NavigationRequest::OnWillCommitWithoutUrlLoaderProcessed(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK_EQ(WILL_COMMIT_WITHOUT_URL_LOADER, state_);
  DCHECK(result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
         result.action() == NavigationThrottle::PROCEED);
  DCHECK(processing_navigation_throttle_);
  processing_navigation_throttle_ = false;
  if (complete_callback_for_testing_ &&
      std::move(complete_callback_for_testing_).Run(result)) {
    return;
  }

  if (MaybeEvictFromBackForwardCacheBySubframeNavigation(
          frame_tree_node_->current_frame_host())) {
    // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
    // deleted by the previous calls.
    return;
  }

  OnWillCommitWithoutUrlLoaderChecksComplete(result);

  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

RenderFrameHostImpl*
NavigationRequest::GetRenderFrameHostRestoredFromBackForwardCache() const {
  if (IsServedFromBackForwardCache()) {
    return &*rfh_restored_from_back_forward_cache_.value();
  }
  return nullptr;
}

NavigatorDelegate* NavigationRequest::GetDelegate() const {
  return frame_tree_node()->navigator().GetDelegate();
}

void NavigationRequest::Resume(NavigationThrottle* resuming_throttle) {
  DCHECK(resuming_throttle);
  CHECK(!is_resuming_) << "This call does not support re-entrancy.";
  EnterChildTraceEvent("Resume", this);
  is_resuming_ = true;

  // Stop watching for response body changes to ensure that the response body
  // callback isn't called later in the throttle's lifetime with a response body
  // that is not relevant to the throttle.
  if (response_body_watcher_) {
    CHECK(response_body_callback_);
    response_body_watcher_.reset();
    std::move(response_body_callback_).Run(std::string());
  }

  is_resuming_ = false;
  throttle_runner_->ResumeProcessingNavigationEvent(resuming_throttle);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::CancelDeferredNavigation(
    NavigationThrottle* cancelling_throttle,
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(cancelling_throttle);
  DCHECK_EQ(cancelling_throttle, throttle_runner_->GetDeferringThrottle());
  CancelDeferredNavigationInternal(result);
}

void NavigationRequest::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  // Throttles will already have run the first time the page was navigated, we
  // won't run them again on activation. See instead CommitDeferringCondition.
  DCHECK(!IsPageActivation())
      << "Attempted to register a NavigationThrottle for an activating "
         "navigation which will not work.";
  throttle_runner_->AddThrottle(std::move(navigation_throttle));
}
bool NavigationRequest::IsDeferredForTesting() {
  return IsDeferred();
}

bool NavigationRequest::IsMhtmlOrSubframe() {
  DCHECK(state_ >= WILL_PROCESS_RESPONSE ||
         state_ == WILL_START_REQUEST && !NeedsUrlLoader());

  return is_mhtml_or_subframe_;
}

bool NavigationRequest::IsForMhtmlSubframe() const {
  return frame_tree_node_->parent() && frame_tree_node_->frame_tree()
                                           .root()
                                           ->current_frame_host()
                                           ->is_mhtml_document();
}

void NavigationRequest::CancelDeferredNavigationInternal(
    NavigationThrottle::ThrottleCheckResult result) {
  DCHECK(processing_navigation_throttle_);
  DCHECK(result.action() == NavigationThrottle::CANCEL_AND_IGNORE ||
         result.action() == NavigationThrottle::CANCEL ||
         result.action() == NavigationThrottle::BLOCK_RESPONSE ||
         result.action() == NavigationThrottle::BLOCK_REQUEST ||
         result.action() == NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE);
  DCHECK((result.action() != NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE &&
          result.action() != NavigationThrottle::BLOCK_REQUEST) ||
         state_ == WILL_START_REQUEST || state_ == WILL_REDIRECT_REQUEST);

  EnterChildTraceEvent("CancelDeferredNavigation", this);
  NavigationState old_state = state_;
  SetState(CANCELING);
  if (complete_callback_for_testing_ &&
      std::move(complete_callback_for_testing_).Run(result)) {
    return;
  }

  switch (old_state) {
    case WILL_START_REQUEST:
      OnStartChecksComplete(result);
      return;
    case WILL_REDIRECT_REQUEST:
      OnRedirectChecksComplete(result);
      return;
    case WILL_FAIL_REQUEST:
      OnFailureChecksComplete(result);
      return;
    case WILL_PROCESS_RESPONSE:
      OnWillProcessResponseChecksComplete(result);
      return;
    case WILL_COMMIT_WITHOUT_URL_LOADER:
      OnWillCommitWithoutUrlLoaderChecksComplete(result);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

void NavigationRequest::WillStartRequest() {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::WillStartRequest",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  EnterChildTraceEvent("WillStartRequest", this);
  DCHECK_EQ(state_, WILL_START_REQUEST);
  will_start_request_time_ = base::TimeTicks::Now();

  if (IsSelfReferentialURL()) {
    SetState(CANCELING);
    DVLOG(1) << "Cancelling self-referential request for " << GetURL();
    if (complete_callback_for_testing_ &&
        std::move(complete_callback_for_testing_)
            .Run(NavigationThrottle::CANCEL)) {
      return;
    }
    OnWillProcessResponseChecksComplete(NavigationThrottle::CANCEL);

    // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
    // deleted by the previous calls.
    return;
  }

  // Throttles will already have run the first time the page was navigated, we
  // won't run them again on activation.
  if (!IsPageActivation()) {
    base::ElapsedTimer duration;
    throttle_runner_->RegisterNavigationThrottles();
    base::UmaHistogramTimes(
        base::StrCat({"Navigation.RegisterNavigationThrottlesTime.",
                      IsInMainFrame() ? "MainFrame" : "Subframe"}),
        duration.Elapsed());
  }

  // If the content/ embedder did not pass the NavigationUIData at the beginning
  // of the navigation, ask for it now.
  if (!navigation_ui_data_) {
    navigation_ui_data_ = GetDelegate()->GetNavigationUIData(this);
  }

  processing_navigation_throttle_ = true;

  base::ScopedUmaHistogramTimer timer(base::StrCat(
      {"Navigation.ProcessNavigationThrottlesTime.WillStartRequest.",
       IsInMainFrame() ? "MainFrame" : "SubFrame"}));
  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::kWillStartRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillRedirectRequest(
    const GURL& new_referrer_url,
    RenderProcessHost* post_redirect_process) {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::WillRedirectRequest",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  EnterChildTraceEvent("WillRedirectRequest", this, "url",
                       common_params_->url.possibly_invalid_spec());
  UpdateStateFollowingRedirect(new_referrer_url);
  UpdateSiteInfo(post_redirect_process);

  if (IsSelfReferentialURL()) {
    SetState(CANCELING);
    if (complete_callback_for_testing_ &&
        std::move(complete_callback_for_testing_)
            .Run(NavigationThrottle::CANCEL)) {
      return;
    }
    OnWillProcessResponseChecksComplete(NavigationThrottle::CANCEL);

    // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
    // deleted by the previous calls.
    return;
  }

  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::kWillRedirectRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillFailRequest() {
  EnterChildTraceEvent("WillFailRequest", this);

  SetState(WILL_FAIL_REQUEST);
  processing_navigation_throttle_ = true;

  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::kWillFailRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillProcessResponse() {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::WillProcessResponse",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  EnterChildTraceEvent("WillProcessResponse", this);
  DCHECK_EQ(state_, WILL_PROCESS_RESPONSE);

  processing_navigation_throttle_ = true;
  was_get_response_body_called_ = false;
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());

  // Notify each throttle of the response.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::kWillProcessResponse);

  // `this` may have been deleted by the previous call.
  if (!this_ptr) {
    // DO NOT ADD CODE HERE.
    return;
  }

  CHECK(!was_get_response_body_called_ || IsDeferred());
}

void NavigationRequest::WillCommitWithoutUrlLoader() {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::WillCommitWithoutUrlLoader",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  EnterChildTraceEvent("WillCommitWithoutUrlLoader", this);

  throttle_runner_->RegisterNavigationThrottlesForCommitWithoutUrlLoader();

  // `CommitNavigation()` expects to be called once the request has reached
  // at least `WILL_PROCESS_REPSONSE`. `WILL_COMMIT_WITHOUT_URL_LOADER` meets
  // that requirement, and is useful to clarify which throttles we are waiting
  // for.
  SetState(WILL_COMMIT_WITHOUT_URL_LOADER);
  processing_navigation_throttle_ = true;

  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::kWillCommitWithoutUrlLoader);
}

bool NavigationRequest::IsSelfReferentialURL() {
  // about: URLs should be exempted since they are reserved for other purposes
  // and cannot be the source of infinite recursion.
  // See https://crbug.com/341858 .
  if (common_params_->url.SchemeIs(url::kAboutScheme))
    return false;

  // Browser-triggered navigations should be exempted.
  if (browser_initiated())
    return false;

  // Some sites rely on constructing frame hierarchies where frames are loaded
  // via POSTs with the same URLs, so exempt POST requests.
  // See https://crbug.com/710008.
  if (common_params_->method == "POST")
    return false;

  // We allow one level of self-reference because some sites depend on that,
  // but we don't allow more than one.
  bool found_self_reference = false;
  for (RenderFrameHost* rfh = frame_tree_node()->parent(); rfh;
       rfh = rfh->GetParent()) {
    if (rfh->GetLastCommittedURL().EqualsIgnoringRef(common_params_->url)) {
      if (found_self_reference)
        return true;
      found_self_reference = true;
    }
  }
  return false;
}

void NavigationRequest::DidCommitNavigation(
    const mojom::DidCommitProvisionalLoadParams& params,
    bool navigation_entry_committed,
    bool did_replace_entry,
    const GURL& previous_main_frame_url) {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::DidCommitNavigation",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  common_params_->url = params.url;
  did_replace_entry_ = did_replace_entry;
  should_update_history_ = params.should_update_history;
  navigation_handle_timing_.navigation_commit_received_time =
      params.commit_navigation_start;
  navigation_handle_timing_.navigation_did_commit_time = base::TimeTicks::Now();
  // A same document navigation with the same url, and no user-gesture is
  // typically the result of 'history.replaceState().' As the page is
  // controlling this, the user doesn't really think of this as a navigation
  // and it doesn't make sense to log this in history. Logging this in history
  // would lead to lots of visits to a particular page, which impacts the
  // visit count.
  // Navigations in non-primary frame trees don't appear in history.
  if ((should_update_history_ && IsSameDocument() && !HasUserGesture() &&
       params.url == previous_main_frame_url) ||
      !GetRenderFrameHost()->GetPage().IsPrimary()) {
    should_update_history_ = false;
  }
  previous_main_frame_url_ = previous_main_frame_url;

  // It should be kept in sync with the check in
  // RenderFrameHostImpl::TakeNewDocumentPropertiesFromNavigation.
  if (DidEncounterError()) {
    EnterChildTraceEvent("DidCommitNavigation: error page", this);
    SetState(DID_COMMIT_ERROR_PAGE);
  } else {
    EnterChildTraceEvent("DidCommitNavigation", this);
    SetState(DID_COMMIT);
  }
  navigation_or_document_handle_->OnNavigationCommitted(*this);

  // If the navigation committed successfully, pass ownership of ViewTransition
  // resources to the new view. This ensures that the resources are cleaned up
  // if the new renderer process terminates before taking ownership of them.
  if (view_transition_resources_ && state_ == DID_COMMIT) {
    GetRenderFrameHost()
        ->GetRenderWidgetHost()
        ->GetRenderWidgetHostViewBase()
        ->SetViewTransitionResources(std::move(view_transition_resources_));
  }

  StopCommitTimeout();

  // Switching BrowsingInstance because of COOP or top-level cross browsing
  // instance navigation resets the name of the frame. The renderer already
  // knows locally about it because we sent an empty name at frame creation
  // time. The renderer has now committed the page and we can safely enforce the
  // empty name on the browser side.
  bool should_clear_browsing_instance_name =
      browsing_context_group_swap().ShouldClearWindowName() ||
      (commit_params().is_cross_site_cross_browsing_context_group &&
       base::FeatureList::IsEnabled(
           features::kClearCrossSiteCrossBrowsingContextGroupWindowName));

  if (should_clear_browsing_instance_name) {
    std::string name, unique_name;
    // The "swap" only affect main frames, that have an empty unique name.
    if (features::GetBrowsingContextMode() ==
        features::BrowsingContextStateImplementationType::
            kLegacyOneToOneWithFrameTreeNode) {
      DCHECK(frame_tree_node_->unique_name().empty());
      GetRenderFrameHost()->browsing_context_state()->SetFrameName(name,
                                                                   unique_name);
    }
  }

  // Record metrics for the time it took to commit the navigation if it was to
  // another document without error.
  if (!IsSameDocument() && state_ != DID_COMMIT_ERROR_PAGE) {
    ui::PageTransition transition =
        ui::PageTransitionFromInt(common_params_->transition);
    base::Process::Priority priority =
        GetRenderFrameHost()->GetProcess()->GetPriority();

    RecordStartToCommitMetrics(
        common_params_->navigation_start, transition, ready_to_commit_time_,
        priority, is_same_process_, frame_tree_node_->IsMainFrame());
  }

  DCHECK(!frame_tree_node_->IsMainFrame() || navigation_entry_committed)
      << "Only subframe navigations can get here without changing the "
      << "NavigationEntry";
  subframe_entry_committed_ = navigation_entry_committed;

  // For successful navigations, ensure the frame owner element is no longer
  // collapsed as a result of a prior navigation.
  if (state_ != DID_COMMIT_ERROR_PAGE &&
      (!frame_tree_node()->IsMainFrame() ||
       frame_tree_node()->IsFencedFrameRoot())) {
    // The last committed load in collapsed frames will be an error page with
    // |kUnreachableWebDataURL|. Same-document navigation should not be
    // possible.
    DCHECK(!IsSameDocument() || !frame_tree_node()->is_collapsed());
    frame_tree_node()->SetCollapsed(false);
  }

  if (service_worker_handle_ &&
      service_worker_handle_->service_worker_client()) {
    // Notify the service worker navigation handle that the navigation finished
    // committing.
    service_worker_handle_->service_worker_client()->OnEndNavigationCommit();
  }

  // TODO(crbug.com/40249865): consider using NavigationOrDocumentHandle
  // instead once we can get a WeakDocumentPtr from NavigationOrDocumentHandle.
  if (subresource_proxying_url_loader_service_bind_context_) {
    DCHECK(!IsSameDocument());

    subresource_proxying_url_loader_service_bind_context_
        ->OnDidCommitNavigation(GetRenderFrameHost()->GetWeakDocumentPtr());
  }
  if (keep_alive_url_loader_factory_context_) {
    DCHECK(!IsSameDocument());

    keep_alive_url_loader_factory_context_->OnDidCommitNavigation(
        GetRenderFrameHost()->GetWeakDocumentPtr());
  }
  if (fetch_later_loader_factory_context_) {
    DCHECK(!IsSameDocument());

    fetch_later_loader_factory_context_->OnDidCommitNavigation(
        GetRenderFrameHost()->GetWeakDocumentPtr());
  }

  // Network status of the entire frame tree needs to be updated once a
  // NavigationRequest commits. When fenced frames revoke network access by
  // calling `window.fence.disableUntrustedNetwork`, the returned promise cannot
  // be resolved until ongoing navigations in descendant frames complete.
  GetRenderFrameHost()
      ->GetOutermostMainFrame()
      ->CalculateUntrustedNetworkStatus();

  if (!pending_commit_metrics_.start_time.is_null()) {
    const bool is_for_mhtml = IsMhtmlMimeType(GetMimeType());
    base::UmaHistogramTimes(
        is_for_mhtml ? "Navigation.PendingCommit.Duration.MHTML"
                     : "Navigation.PendingCommit.Duration.Regular",
        base::TimeTicks::Now() - pending_commit_metrics_.start_time);
    const bool did_block_get_frame_host_for_navigation =
        pending_commit_metrics_.blocked_count > 0;
    base::UmaHistogramBoolean(
        is_for_mhtml
            ? "Navigation.PendingCommit.DidBlockGetFrameHostForNavigation.MHTML"
            : "Navigation.PendingCommit.DidBlockGetFrameHostForNavigation."
              "Regular",
        did_block_get_frame_host_for_navigation);
    if (did_block_get_frame_host_for_navigation) {
      base::UmaHistogramCounts100(
          is_for_mhtml ? "Navigation.PendingCommit.BlockedCount.MHTML"
                       : "Navigation.PendingCommit.BlockedCount.Regular",
          pending_commit_metrics_.blocked_count);
      base::UmaHistogramCounts100(
          is_for_mhtml ? "Navigation.PendingCommit.BlockedCommitCount.MHTML"
                       : "Navigation.PendingCommit.BlockedCommitCount.Regular",
          pending_commit_metrics_.blocked_commit_count);
    }
  }

  // DO NOT ADD CODE after this.
  // UnblockPendingSubframeNavigationRequestsIfNeeded() resumes throttles, which
  // may cause the destruction of this NavigationRequest.
  UnblockPendingSubframeNavigationRequestsIfNeeded();
}

SiteInfo NavigationRequest::GetSiteInfoForCommonParamsURL() {
  UrlInfo url_info = GetUrlInfo();

  // TODO(alexmos): Using |starting_site_instance_|'s IsolationContext may not
  // be correct for cross-BrowsingInstance redirects.
  return SiteInfo::Create(starting_site_instance_->GetIsolationContext(),
                          url_info);
}

// TODO(zetamoo): Try to merge this function inside its callers.
void NavigationRequest::UpdateStateFollowingRedirect(
    const GURL& new_referrer_url) {
  // The navigation should not redirect to a "renderer debug" url. It should be
  // blocked in NavigationRequest::OnRequestRedirected or in
  // ResourceLoader::OnReceivedRedirect.
  // Note: the |common_params_->url| below is the post-redirect URL.
  // See https://crbug.com/728398.
  CHECK(!blink::IsRendererDebugURL(common_params_->url));

  // Re-generate the feature context to ensure that the runtime-enabled features
  // have the correct state values.
  runtime_feature_state_context_ = blink::RuntimeFeatureStateContext();

  // Update the navigation parameters.
  if (!(common_params_->transition & ui::PAGE_TRANSITION_CLIENT_REDIRECT)) {
    sanitized_referrer_->url = new_referrer_url;
    sanitized_referrer_ =
        Referrer::SanitizeForRequest(common_params_->url, *sanitized_referrer_);
  }

  common_params_->referrer = sanitized_referrer_.Clone();

  was_redirected_ = true;
  redirect_chain_.push_back(common_params_->url);

  SetState(WILL_REDIRECT_REQUEST);
  processing_navigation_throttle_ = true;

#if BUILDFLAG(IS_ANDROID)
  navigation_handle_proxy_->DidRedirect();
#endif
}

void NavigationRequest::SetNavigationClient(
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client) {
  DCHECK(from_begin_navigation_ ||
         common_params_->is_history_navigation_in_new_child_frame);
  DCHECK(!request_navigation_client_);
  if (!navigation_client.is_valid())
    return;

  request_navigation_client_.reset();
  request_navigation_client_.Bind(std::move(navigation_client));

  // Binds the OnAbort callback
  HandleInterfaceDisconnection(request_navigation_client_);
}

bool NavigationRequest::NeedsUrlLoader() {
#if BUILDFLAG(IS_ANDROID)
  // If the navigation is for a PDF file, Chrome on Android will render it with
  // a Java NativePage object and the navigation will always be main frame. The
  // NativePage is responsible for reading the file and thus no URLLoader is
  // needed. If NativePage is not enabled for PDF, |is_pdf_| should never be
  // true.
  if (is_pdf_) {
    return false;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  bool is_mhtml_subframe_loaded_from_achive =
      IsForMhtmlSubframe() &&
      // Unlike all other MHTML subframe URLs, data-url are loaded via the
      // URL, not from the MHTML archive. See https://crbug.com/969696.
      !common_params_->url.SchemeIs(url::kDataScheme);

  return IsURLHandledByNetworkStack(common_params_->url) && !IsSameDocument() &&
         !is_mhtml_subframe_loaded_from_achive;
}

void NavigationRequest::UpdatePrivateNetworkRequestPolicy() {
  // It is useless to update this state for same-document navigations as well
  // as pages served from the back-forward cache or prerendered pages.
  DCHECK(!IsSameDocument());
  DCHECK(!IsPageActivation());
  if (GetSocketAddress().address().IsValid() &&
      GetSocketAddress().address().IsZero()) {
    web_features_to_log_.push_back(
        blink::mojom::WebFeature::kPrivateNetworkAccessNullIpAddress);
  }

  ContentBrowserClient* client = GetContentClient()->browser();
  BrowserContext* context =
      frame_tree_node_->navigator().controller().GetBrowserContext();

  url::Origin origin = GetOriginToCommit().value();
  ContentBrowserClient::PrivateNetworkRequestPolicyOverride policy_override =
      client->ShouldOverridePrivateNetworkRequestPolicy(context, origin);

  if (policy_override ==
      ContentBrowserClient::PrivateNetworkRequestPolicyOverride::kForceAllow) {
    private_network_request_policy_ =
        network::mojom::PrivateNetworkRequestPolicy::kAllow;
    return;
  }

  const PolicyContainerPolicies& policies =
      policy_container_builder_->FinalPolicies();

  if (!policies.is_web_secure_context &&
      base::FeatureList::IsEnabled(
          features::kBlockInsecurePrivateNetworkRequestsDeprecationTrial) &&
      // If there is no response or no headers in the response, there are
      // definitely no trial token headers.
      response_head_ && response_head_->headers &&
      blink::TrialTokenValidator().RequestEnablesDeprecatedFeature(
          common_params_->url, response_head_->headers.get(),
          "PrivateNetworkAccessNonSecureContextsAllowed", base::Time::Now())) {
    web_features_to_log_.push_back(
        blink::mojom::WebFeature::
            kPrivateNetworkAccessNonSecureContextsAllowedDeprecationTrial);
    private_network_request_policy_ =
        network::mojom::PrivateNetworkRequestPolicy::kAllow;
    return;
  }

  private_network_request_policy_ = DerivePrivateNetworkRequestPolicy(
      policies, PrivateNetworkRequestContext::kSubresource);

  if (policy_override ==
      ContentBrowserClient::PrivateNetworkRequestPolicyOverride::
          kBlockInsteadOfWarn) {
    private_network_request_policy_ =
        OverrideBlockWithWarn(private_network_request_policy_);
  }
}

std::vector<blink::mojom::WebFeature>
NavigationRequest::TakeWebFeaturesToLog() {
  std::vector<blink::mojom::WebFeature> result;
  result.swap(web_features_to_log_);
  return result;
}

void NavigationRequest::ReadyToCommitNavigation(bool is_error) {
  TRACE_EVENT_WITH_FLOW0("navigation",
                         "NavigationRequest::ReadyToCommitNavigation",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  EnterChildTraceEvent("ReadyToCommitNavigation", this);

  // We may come back to here asynchronously, and the renderer may be destroyed
  // in the meantime. Renderer-initiated navigations listen to mojo
  // disconnection from the renderer NavigationClient; but browser-initiated
  // navigations do not, so we must look explicitly. We should not proceed and
  // claim "ReadyToCommitNavigation" to the delegate if the renderer is gone.
  if (!GetRenderFrameHost()->IsRenderFrameLive()) {
    OnNavigationClientDisconnected(0, "");
    // DO NOT ADD CODE AFTER THIS, as the NavigationHandle has been deleted
    // by the previous call.
    return;
  }

  // Note: This marks the RenderFrameHost as loading. This is important to
  // ensure that FrameTreeNode::IsLoading() still returns correct result for
  // delegate and observer callbacks. Otherwise, there would be a period of time
  // where the FrameTreeNode has no NavigationRequest, yet the
  // RenderFrameHostImpl is not marked as loading yet, causing
  // FrameTreeNode::IsLoading() to incorrectly return false.
  frame_tree_node_->TransferNavigationRequestOwnership(GetRenderFrameHost());

  // When a speculative RenderFrameHost reaches ReadyToCommitNavigation, the
  // browser process has asked the renderer to commit the navigation and is
  // waiting for confirmation of the commit. Update the LifecycleStateImpl to
  // kPendingCommit as RenderFrameHost isn't considered speculative anymore and
  // was chosen to commit as this navigation's final RenderFrameHost.
  if (GetRenderFrameHost()->lifecycle_state() ==
      RenderFrameHostImpl::LifecycleStateImpl::kSpeculative) {
    // Only cross-RenderFrameHost navigations create speculative
    // RenderFrameHosts whereas SameDocument, BackForwardCache and
    // PrerenderedActivation navigations don't.
    DCHECK(!IsSameDocument() && !IsPageActivation());
    GetRenderFrameHost()->SetLifecycleState(
        RenderFrameHostImpl::LifecycleStateImpl::kPendingCommit);
    pending_commit_metrics_.start_time = base::TimeTicks::Now();
  }

  // Reset the source location information, which is not needed anymore. This
  // avoids leaking cross-origin data to another process in case the navigation
  // doesn't commit in the same process as the document that initiated it.
  common_params_->source_location = network::mojom::SourceLocation::New();

  SetState(READY_TO_COMMIT);
  ready_to_commit_time_ = base::TimeTicks::Now();
  RestartCommitTimeout();

  if (!IsSameDocument() && !IsPageActivation())
    UpdatePrivateNetworkRequestPolicy();

  RenderFrameHostImpl* previous_render_frame_host =
      frame_tree_node_->current_frame_host();

  // Record metrics for the time it takes to get to this state from the
  // beginning of the navigation.
  if (!IsSameDocument() && !is_error) {
    is_same_process_ = GetRenderFrameHost()->GetProcess()->GetID() ==
                       previous_render_frame_host->GetProcess()->GetID();

    RecordReadyToCommitMetrics(
        previous_render_frame_host, GetRenderFrameHost(), *common_params_.get(),
        ready_to_commit_time_, origin_agent_cluster_end_result_,
        did_receive_early_hints_before_cross_origin_redirect_);
  }

  // Calculate origin in which this navigation would commit so it can be
  // compared with what is generated by the renderer process and the browser
  // process at commit time.
  // TODO(crbug.com/40092527): Consider using this cached value everywhere
  // we currently call `GetOriginToCommit()`, to prevent nonce mismatches.
  browser_side_origin_to_commit_with_debug_info_ =
      GetOriginToCommitWithDebugInfo();
  std::optional<url::Origin> origin_to_commit =
      browser_side_origin_to_commit_with_debug_info_.first;
  same_origin_ = (previous_render_frame_host->GetLastCommittedOrigin() ==
                  origin_to_commit);

  SetExpectedProcess(GetRenderFrameHost()->GetProcess());

  commit_params_->is_load_data_with_base_url = IsLoadDataWithBaseURL();

  // Set origin_to_commit for all cases if kUseBrowserCalculatedOrigin is
  // enabled, and for these two cases otherwise:
  // 1) Error pages, which should always commit in an opaque origin (with the
  // precursor reflecting the destination URL).
  // 2) data: URLs, which should also be opaque.
  // Set the origin here because otherwise the origin (and hence the nonce) is
  // separately calculated on the renderer side and sent with DidCommit. At that
  // point the origin used to create the SiteInstance will differ from the one
  // committed in the renderer. For data: URLs, a consistent nonce across the
  // browser and renderer can be used to determine which data: URL SiteInstance
  // should be used in RenderFrameProxyHost::OpenURL.
  // We do not need to set it for LoadDataWithBaseURL cases, because it will not
  // lead to ambiguous cases where multiple data: SiteInstances will be in the
  // same group. However, when the base URL is empty, LoadDataWithBaseURL is
  // treated like a regular data: URL.
  if (base::FeatureList::IsEnabled(features::kUseBrowserCalculatedOrigin) ||
      is_error ||
      (common_params_->url.SchemeIs(url::kDataScheme) &&
       !IsLoadDataWithBaseURL())) {
    commit_params_->origin_to_commit = origin_to_commit;
    CHECK(base::FeatureList::IsEnabled(features::kUseBrowserCalculatedOrigin) ||
          !is_error || origin_to_commit->opaque());
  }

  if (!IsSameDocument()) {
#if DCHECK_IS_ON()
    DCHECK(is_safe_to_delete_);
    base::AutoReset<bool> resetter(&is_safe_to_delete_, false);
#endif
    GetDelegate()->ReadyToCommitNavigation(this);
  }

  // View-source URLs can't be prerendered or loaded in a fenced frame.
  if (IsInPrimaryMainFrame()) {
    NavigationEntry* entry = GetNavigationEntry();
    if (entry && entry->IsViewSourceMode()) {
      // Put the renderer in view source mode.
      GetRenderFrameHost()->GetAssociatedLocalFrame()->EnableViewSourceMode();
    }
  }

  // For fenced frames, update the mapped URL to be the URL from navigation
  // commit (after redirects), because we want future same-origin checks to be
  // performed with respect to the first origin committed in the fenced frame.
  if (is_embedder_initiated_fenced_frame_navigation_) {
    // In certain circumstances, the FencedFrameProperties will not have a
    // mapped url.
    // * The initial about:blank navigation in a fenced frame.
    // In those cases, we skip this step.
    if (fenced_frame_properties_.has_value() &&
        fenced_frame_properties_->mapped_url().has_value()) {
      fenced_frame_properties_->UpdateMappedURL(GetURL());
    }

    // For fenced frames with flexible permissions, pass in information needed
    // to build a replica of the embedder's permissions policies. This does not
    // happen for URN iframes as they can get their embedder's permissions
    // policies directly in the renderer.
    if (base::FeatureList::IsEnabled(
            blink::features::kFencedFramesLocalUnpartitionedDataAccess) &&
        GetNavigatingFrameType() == FrameType::kFencedFrameRoot &&
        fenced_frame_properties_->effective_enabled_permissions().size() ==
            0u) {
      fenced_frame_properties_->UpdateParentParsedPermissionsPolicy(
          GetParentFrameOrOuterDocument()->GetPermissionsPolicy(),
          GetParentFrameOrOuterDocument()->GetLastCommittedOrigin());
    }
  }

  if (ready_to_commit_callback_for_testing_)
    std::move(ready_to_commit_callback_for_testing_).Run();
}

bool NavigationRequest::IsWaitingToCommit() {
  return state_ == READY_TO_COMMIT;
}

bool NavigationRequest::WasResourceHintsReceived() {
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE)
      << "Should only be called after the response started";
  return was_resource_hints_received_;
}

bool NavigationRequest::IsPdf() {
  return is_pdf_;
}

bool NavigationRequest::IsLoadDataWithBaseURL() const {
  // A navigation is a loadDataWithBaseURL navigation if it's a successful
  // primary main frame navigation to a data: URL, and its base URL is valid.
  return IsInPrimaryMainFrame() && !DidEncounterError() &&
         common_params_->url.SchemeIs(url::kDataScheme) &&
         common_params_->base_url_for_data_url.is_valid();
}

url::Origin NavigationRequest::GetTentativeOriginAtRequestTime() {
  DCHECK_LT(state_, WILL_PROCESS_RESPONSE);
  return GetOriginForURLLoaderFactoryBeforeResponse(
      commit_params_->frame_policy.sandbox_flags);
}

std::optional<url::Origin> NavigationRequest::GetOriginToCommit() {
  return GetOriginToCommitWithDebugInfo().first;
}

std::pair<std::optional<url::Origin>, std::string>
NavigationRequest::GetOriginToCommitWithDebugInfo() {
  return GetOriginForURLLoaderFactoryAfterResponseWithDebugInfo();
}

url::Origin NavigationRequest::GetOriginForURLLoaderFactoryBeforeResponse(
    network::mojom::WebSandboxFlags sandbox_flags) {
  return GetOriginForURLLoaderFactoryBeforeResponseWithDebugInfo(sandbox_flags)
      .first;
}

std::pair<url::Origin, std::string>
NavigationRequest::GetOriginForURLLoaderFactoryBeforeResponseWithDebugInfo(
    network::mojom::WebSandboxFlags sandbox_flags) {
  // Calculate an approximation of the origin. The sandbox/csp are ignored.
  std::pair<url::Origin, std::string> origin_and_debug_info =
      GetOriginForURLLoaderFactoryUncheckedWithDebugInfo();

  // Apply sandbox flags.
  // See https://html.spec.whatwg.org/#sandboxed-origin-browsing-context-flag
  // ```
  // The 'sandboxed origin browsing context flag' forces content into a unique
  // origin, thus preventing it from accessing other content from the same
  // origin.
  //
  // This flag also prevents script from reading from or writing to the
  // document.cookie IDL attribute, and blocks access to localStorage.
  // ```
  bool use_opaque_origin =
      (sandbox_flags & network::mojom::WebSandboxFlags::kOrigin) ==
      network::mojom::WebSandboxFlags::kOrigin;
  if (use_opaque_origin) {
    origin_and_debug_info =
        std::pair(origin_and_debug_info.first.DeriveNewOpaqueOrigin(),
                  origin_and_debug_info.second + ", sandbox_flags");
  }

  return origin_and_debug_info;
}

std::optional<url::Origin>
NavigationRequest::GetOriginForURLLoaderFactoryAfterResponse() {
  return GetOriginForURLLoaderFactoryAfterResponseWithDebugInfo().first;
}

// TODO(crbug.com/40065692): Remove.
// Determine the relationship between the initiator and the current frame.
// `same`: they are the same frame.
// `ancestor`: the initiator is the ancestor of the navigating frame.
// `descendant`: the initiator is the descendant of the navigating frame.
// `other`: any other scenarios.
std::string DetermineInitiatorRelationship(RenderFrameHost* initiator_frame,
                                           RenderFrameHost* current_frame) {
  if (!current_frame || !initiator_frame) {
    return "other";
  }

  if (current_frame == initiator_frame) {
    return "same";
  }

  RenderFrameHost* rfh = current_frame;
  while (rfh) {
    rfh = rfh->GetParent();
    if (rfh == initiator_frame) {
      return "ancestor";
    }
  }

  rfh = initiator_frame;
  while (rfh) {
    rfh = rfh->GetParent();
    if (rfh == current_frame) {
      return "descendant";
    }
  }

  return "other";
}

std::pair<std::optional<url::Origin>, std::string>
NavigationRequest::GetOriginForURLLoaderFactoryAfterResponseWithDebugInfo() {
  // The origin to commit is not known until we get the final network response.
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);

  // Downloads and/or 204 responses don't commit anything - there is no frame to
  // commit in (and therefore there is no origin that will get committed and we
  // indicate this by returning `nullopt`).
  if (!response_should_be_rendered_) {
    return std::make_pair(std::nullopt, "no_commit");
  }

  if (IsSameDocument() || IsPageActivation()) {
    CHECK(HasRenderFrameHost());
    return std::make_pair(GetRenderFrameHost()->GetLastCommittedOrigin(),
                          "same_doc_or_page_activation");
  }

  std::pair<url::Origin, std::string> origin_with_debug_info =
      GetOriginForURLLoaderFactoryBeforeResponseWithDebugInfo(
          SandboxFlagsToCommit());

  // Add the crash keys for debugging navigation crashes with data URL.
  // TODO(crbug.com/40065692): Remove.
  SCOPED_CRASH_KEY_STRING256("Bug1454273", "calculated_origin",
                             origin_with_debug_info.first.GetDebugString());
  SCOPED_CRASH_KEY_STRING256("Bug1454273", "origin_debug_info",
                             origin_with_debug_info.second);

  SCOPED_CRASH_KEY_BOOL("Bug1454273", "is_in_main_frame", IsInMainFrame());
  SCOPED_CRASH_KEY_STRING256(
      "Bug1454273", "current_origin",
      frame_tree_node_->current_origin().GetDebugString());
  RenderFrameHostImpl* parent = frame_tree_node_->parent();
  SCOPED_CRASH_KEY_STRING256(
      "Bug1454273", "parent_origin",
      parent ? parent->GetLastCommittedOrigin().GetDebugString() : "");
  // `outer_doc` is only set when `parent` is null.
  RenderFrameHostImpl* outer_doc =
      parent ? nullptr : GetParentFrameOrOuterDocument();
  SCOPED_CRASH_KEY_STRING256(
      "Bug1454273", "outer_doc_origin",
      outer_doc ? outer_doc->GetLastCommittedOrigin().GetDebugString() : "");
  // `embedder` is only set when both `parent` and `outer_doc` are null.
  RenderFrameHostImpl* embedder =
      (parent || outer_doc)
          ? nullptr
          : frame_tree_node()->GetParentOrOuterDocumentOrEmbedder();
  SCOPED_CRASH_KEY_STRING256(
      "Bug1454273", "embedder_origin",
      embedder ? embedder->GetLastCommittedOrigin().GetDebugString() : "");

  RenderFrameHost* initiator_rfh = GetInitiatorDocumentRenderFrameHost();
  SCOPED_CRASH_KEY_STRING256(
      "Bug1454273", "initiator_origin",
      initiator_rfh ? initiator_rfh->GetLastCommittedOrigin().GetDebugString()
                    : "");
  SCOPED_CRASH_KEY_STRING32(
      "Bug1454273", "initiator_relationship",
      DetermineInitiatorRelationship(initiator_rfh,
                                     frame_tree_node_->current_frame_host()));

  // MHTML documents should commit as an opaque origin. They should not be able
  // to make network request on behalf of the real origin.
  // TODO(crbug.com/370979008): Migrate to CHECK.
  DUMP_WILL_BE_CHECK(!IsMhtmlOrSubframe() ||
                     origin_with_debug_info.first.opaque());

  // If the target of this navigation will be rendered in a RenderFrameHost,
  // then verify that the chosen origin is allowed to be accessed from that
  // process.
  //
  // Note that GetRenderFrameHost() only allows retrieving the RenderFrameHost
  // once it has been set for this navigation. This happens either at
  // WillProcessResponse time for regular navigations or at WillFailRequest time
  // for error pages.
  //
  // There are some exceptions where this check must be skipped:
  // * Some error pages may commit in an error process that allows many origins.
  // * MHTML iframes can load documents from any origin, no matter the current
  //   policy of the process being used. This is because the content is loaded
  //   from the MHTML archive within the process. There are no data loaded from
  //   the network.
  if (HasRenderFrameHost() &&
      !GetRenderFrameHost()->ShouldBypassSecurityChecksForErrorPage(this) &&
      !IsForMhtmlSubframe()) {
    int process_id = GetRenderFrameHost()->GetProcess()->GetID();
    auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
    CHECK(policy->CanAccessOrigin(
        process_id, origin_with_debug_info.first,
        ChildProcessSecurityPolicyImpl::AccessType::kCanCommitNewOrigin));
  }

  return origin_with_debug_info;
}

void NavigationRequest::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> ctx) const {
  ctx->set_navigation_id(navigation_id_);
  ctx->set_has_committed(HasCommitted());
  ctx->set_is_error_page(IsErrorPage());
  ctx.Set(TraceProto::kFrameTreeNode, frame_tree_node_);
  if (state_ >= WILL_PROCESS_RESPONSE)
    ctx.Set(TraceProto::kRenderFrameHost, GetRenderFrameHost());

  perfetto::TracedDictionary dict = std::move(ctx).AddDebugAnnotations();
  dict.Add("url", common_params_->url);
  dict.Add("net_error", net_error_);
  dict.Add("browser_initiated", commit_params_->is_browser_initiated);
  dict.Add("from_begin_navigation", from_begin_navigation_);
  dict.Add("is_synchronous_renderer_commit", is_synchronous_renderer_commit_);
  dict.Add("reload_type", reload_type_);
  dict.Add("state", state_);
  dict.Add("navigation_type", common_params_->navigation_type);

  if (IsServedFromBackForwardCache()) {
    dict.Add("served_from_bfcache", true);
    dict.Add("rfh_restored_from_bfcache",
             GetRenderFrameHostRestoredFromBackForwardCache());
  }

  if (prerender_frame_tree_node_id_.has_value()) {
    dict.Add("prerender_frame_tree_node_id",
             prerender_frame_tree_node_id_.value());
  }
}

bool NavigationRequest::SetNavigationTimeout(base::TimeDelta timeout) {
  // Setting a navigation-level timeout isn't implemented yet for other states,
  // as the `loader_` must already be created. This means that callers like
  // NavigationThrottles should only call SetNavigationTimeout from
  // WillRedirectRequest(). If other use cases come up that need to set this
  // timeout at other points in the NavigationRequest lifecycle, one possible
  // solution could be to have the NavigationRequest hold the timeout value
  // temporarily until it creates the `loader_` in OnStartChecksCompleted().
  DCHECK_EQ(state_, WILL_REDIRECT_REQUEST);
  if (loader_)
    return loader_->SetNavigationTimeout(timeout);
  return false;
}

void NavigationRequest::CancelNavigationTimeout() {
  if (loader_) {
    loader_->CancelNavigationTimeout();
  }
}

void NavigationRequest::SetAllowCookiesFromBrowser(
    bool allow_cookies_from_browser) {
  allow_cookies_from_browser_ = allow_cookies_from_browser;
}

void NavigationRequest::GetResponseBody(ResponseBodyCallback callback) {
  CHECK_GE(state_, WILL_PROCESS_RESPONSE)
      << "The response body should only be requested after the response body "
         "data pipe is received from the network stack.";
  CHECK(processing_navigation_throttle_ || IsDeferred());
  CHECK(response_body_callback_.is_null());
  CHECK(callback);
  response_body_callback_ = std::move(callback);
  was_get_response_body_called_ = true;

  CHECK(!response_body_watcher_);
  response_body_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  response_body_watcher_->Watch(
      response_body(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&NavigationRequest::OnResponseBodyReady,
                          weak_factory_.GetWeakPtr()));
  response_body_watcher_->ArmOrNotify();
}

void NavigationRequest::RenderProcessBlockedStateChanged(bool blocked) {
  if (blocked)
    StopCommitTimeout();
  else
    RestartCommitTimeout();
}

void NavigationRequest::StopCommitTimeout() {
  commit_timeout_timer_.Stop();
  render_process_blocked_state_changed_subscription_ = {};
  GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsResponsive();
}

void NavigationRequest::RestartCommitTimeout() {
  commit_timeout_timer_.Stop();
  if (state_ >= DID_COMMIT)
    return;

  RenderProcessHost* renderer_host =
      GetRenderFrameHost()->GetRenderWidgetHost()->GetProcess();
  if (!render_process_blocked_state_changed_subscription_) {
    render_process_blocked_state_changed_subscription_ =
        renderer_host->RegisterBlockStateChangedCallback(base::BindRepeating(
            &NavigationRequest::RenderProcessBlockedStateChanged,
            base::Unretained(this)));
  }
  if (!renderer_host->IsBlocked()) {
    commit_timeout_timer_.Start(
        FROM_HERE, g_commit_timeout,
        base::BindOnce(&NavigationRequest::OnCommitTimeout,
                       weak_factory_.GetWeakPtr()));
  }
}

void NavigationRequest::OnCommitTimeout() {
  DCHECK_EQ(READY_TO_COMMIT, state_);
  render_process_blocked_state_changed_subscription_ = {};

  GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsUnresponsive(
      RenderWidgetHostImpl::RendererIsUnresponsiveReason::
          kNavigationRequestCommitTimeout,
      base::BindRepeating(&NavigationRequest::RestartCommitTimeout,
                          weak_factory_.GetWeakPtr()));
}

// static
void NavigationRequest::SetCommitTimeoutForTesting(
    const base::TimeDelta& timeout) {
  if (timeout.is_zero())
    g_commit_timeout = kDefaultCommitTimeout;
  else
    g_commit_timeout = timeout;
}

void NavigationRequest::SetPrerenderActivationNavigationState(
    std::unique_ptr<NavigationEntryImpl> prerender_navigation_entry,
    const blink::mojom::FrameReplicationState& replication_state) {
  DCHECK(IsPrerenderedPageActivation());
  if (!prerender_navigation_state_) {
    prerender_navigation_state_.emplace();
  }
  prerender_navigation_state_->prerender_navigation_entry =
      std::move(prerender_navigation_entry);

  // Store the replication state of the prerender main frame to copy the
  // necessary parameters from it during activation commit and compare with the
  // final replication state after activation to ensure it hasn't changed.
  // TODO(crbug.com/40192974): This will need to be removed when the
  // Browsing Instance Frame State is implemented.
  prerender_navigation_state_->prerender_main_frame_replication_state =
      replication_state;
}

void NavigationRequest::RemoveRequestHeader(const std::string& header_name) {
  DCHECK(state_ == WILL_REDIRECT_REQUEST);
  removed_request_headers_.push_back(header_name);
}

void NavigationRequest::SetRequestHeader(const std::string& header_name,
                                         const std::string& header_value) {
  DCHECK(state_ == WILL_START_REQUEST || state_ == WILL_REDIRECT_REQUEST);
  modified_request_headers_.SetHeader(header_name, header_value);
}

void NavigationRequest::SetCorsExemptRequestHeader(
    const std::string& header_name,
    const std::string& header_value) {
  DCHECK(state_ == WILL_START_REQUEST || state_ == WILL_REDIRECT_REQUEST);
  cors_exempt_request_headers_.SetHeader(header_name, header_value);
}

void NavigationRequest::SetLCPPNavigationHint(
    const blink::mojom::LCPCriticalPathPredictorNavigationTimeHint& hint) {
  DCHECK(WILL_START_REQUEST == state_ || WILL_REDIRECT_REQUEST == state_)
      << state_;
  commit_params_->lcpp_hint = hint.Clone();
}

const blink::mojom::LCPCriticalPathPredictorNavigationTimeHintPtr&
NavigationRequest::GetLCPPNavigationHint() {
  return commit_params_->lcpp_hint;
}

const net::HttpResponseHeaders* NavigationRequest::GetResponseHeaders() {
  return response_head_.get() ? response_head_->headers.get() : nullptr;
}

mojom::DidCommitProvisionalLoadParamsPtr
NavigationRequest::MakeDidCommitProvisionalLoadParamsForActivation() {
  // Use the DidCommitProvisionalLoadParams last used to commit the frame being
  // restored as a starting point.
  mojom::DidCommitProvisionalLoadParamsPtr params =
      GetRenderFrameHost()->GetPage().TakeLastCommitParams();

  // Params must have been set when the RFH being restored from the cache last
  // navigated.
  CHECK(params);

  if (IsPrerenderedPageActivation()) {
    CHECK(!prerender_navigation_utils::IsDisallowedHttpResponseCode(
        params->http_status_code));
  } else {
    DCHECK_EQ(params->http_status_code, net::HTTP_OK);
  }
  DCHECK_EQ(params->url_is_unreachable, false);

  DCHECK_EQ(params->post_id, -1);
  params->navigation_token = commit_params().navigation_token;
  DCHECK_EQ(params->url, common_params().url);
  params->should_update_history = true;
  DCHECK_EQ(params->method, common_params().method);
  params->item_sequence_number = frame_entry_item_sequence_number_;
  params->document_sequence_number = frame_entry_document_sequence_number_;
  params->transition = ui::PageTransitionFromInt(common_params().transition);
  params->history_list_was_cleared = false;
  params->request_id = GetGlobalRequestID().request_id;

  return params;
}

mojom::DidCommitProvisionalLoadParamsPtr
NavigationRequest::MakeDidCommitProvisionalLoadParamsForBFCacheRestore() {
  // Start with the provisional load parameters shared between all page
  // activation types.
  mojom::DidCommitProvisionalLoadParamsPtr params =
      MakeDidCommitProvisionalLoadParamsForActivation();

  // Add bfcache-specific provisional load params:
  params->did_create_new_entry = false;
  params->page_state =
      blink::PageState::CreateFromEncodedData(commit_params().page_state);
  return params;
}

mojom::DidCommitProvisionalLoadParamsPtr
NavigationRequest::MakeDidCommitProvisionalLoadParamsForPrerenderActivation() {
  DCHECK(IsPrerenderedPageActivation());

  // Start with the provisional load parameters shared between all page
  // activation types.
  mojom::DidCommitProvisionalLoadParamsPtr params =
      MakeDidCommitProvisionalLoadParamsForActivation();
  // TODO(crbug.com/40169536): Investigate when a new entry should
  // replace an old one when prerendering a page.
  params->did_create_new_entry = true;
  // Prerendering already has a navigation entry which has correct PageState.
  // Set params->page_state accordingly to ensure that DCHECKs expecting them to
  // match are happy.
  // Note: |params| are using last commit params as a basis (via
  // TakeLastCommitParams call), which have a page state from the last commit,
  // but the page state might have been updated since the last commit.
  params->page_state = prerender_navigation_state_->prerender_navigation_entry
                           ->GetFrameEntry(frame_tree_node())
                           ->page_state();

  // insecure_request_policy field of the replication state is set during the
  // navigation commit based on DidCommitProvisionalLoadParams. As prerendering
  // activates existing page, copy its main frame replication state to ensure
  // that the effective replication state doesn't change after activation.
  // TODO(crbug.com/40192974): replication state should belong to the Browsing
  // Instance Frame State.
  params->insecure_request_policy =
      prerender_navigation_state_->prerender_main_frame_replication_state
          .insecure_request_policy;
  return params;
}

bool NavigationRequest::IsExternalProtocol() {
  return !GetContentClient()->browser()->IsHandledURL(common_params_->url);
}

bool NavigationRequest::IsSignedExchangeInnerResponse() {
  return response() && response()->is_signed_exchange_inner_response;
}

net::IPEndPoint NavigationRequest::GetSocketAddress() {
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);
  return response() ? response()->remote_endpoint : net::IPEndPoint();
}

bool NavigationRequest::HasCommitted() const {
  return state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationRequest::IsErrorPage() const {
  return state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationRequest::DidEncounterError() const {
  return net_error_ != net::OK;
}

net::HttpConnectionInfo NavigationRequest::GetConnectionInfo() {
  return response() ? response()->connection_info : net::HttpConnectionInfo();
}

bool NavigationRequest::IsInMainFrame() const {
  return frame_tree_node()->IsMainFrame();
}

RenderFrameHostImpl* NavigationRequest::GetParentFrame() {
  return IsInMainFrame() ? nullptr : frame_tree_node()->parent();
}

RenderFrameHostImpl* NavigationRequest::GetParentFrameOrOuterDocument() {
  return frame_tree_node()->GetParentOrOuterDocument();
}

bool NavigationRequest::IsInPrimaryMainFrame() const {
  return GetNavigatingFrameType() == FrameType::kPrimaryMainFrame;
}

bool NavigationRequest::IsInOutermostMainFrame() {
  switch (GetNavigatingFrameType()) {
    case FrameType::kPrimaryMainFrame:
    case FrameType::kPrerenderMainFrame:
      return true;
    case FrameType::kSubframe:
    case FrameType::kFencedFrameRoot:
      return false;
  }
}

bool NavigationRequest::IsInPrerenderedMainFrame() const {
  return GetNavigatingFrameType() == FrameType::kPrerenderMainFrame;
}

bool NavigationRequest::IsPrerenderedPageActivation() const {
  CHECK(prerender_frame_tree_node_id_.has_value());
  return !prerender_frame_tree_node_id_.value().is_null();
}

bool NavigationRequest::IsInFencedFrameTree() const {
  return frame_tree_node()->IsInFencedFrameTree();
}

FrameType NavigationRequest::GetNavigatingFrameType() const {
  return frame_tree_node()->GetFrameType();
}

FrameTreeNodeId NavigationRequest::GetFrameTreeNodeId() {
  return frame_tree_node()->frame_tree_node_id();
}

bool NavigationRequest::WasResponseCached() {
  return response() && response()->was_fetched_via_cache;
}

bool NavigationRequest::HasPrefetchedAlternativeSubresourceSignedExchange() {
  return !commit_params_->prefetched_signed_exchanges.empty();
}

int64_t NavigationRequest::GetNavigationId() const {
  return navigation_id_;
}

ukm::SourceId NavigationRequest::GetNextPageUkmSourceId() {
  // If the navigation is restoring from back-forward cache, the UKM id
  // will get restored, too.
  if (IsServedFromBackForwardCache()) {
    return GetRenderFrameHostRestoredFromBackForwardCache()
        ->GetPageUkmSourceId();
  }

  // If this is the same document or a subframe navigation (i.e. iframe or
  // fenced frame), the UKM id will not change from it.
  if (IsSameDocument() || !IsInMainFrame() || IsInFencedFrameTree())
    return previous_page_ukm_source_id_;

  return ukm::ConvertToSourceId(navigation_id_,
                                ukm::SourceIdObj::Type::NAVIGATION_ID);
}

const GURL& NavigationRequest::GetURL() {
  return common_params().url;
}

SiteInstanceImpl* NavigationRequest::GetStartingSiteInstance() {
  return starting_site_instance_.get();
}

SiteInstanceImpl* NavigationRequest::GetSourceSiteInstance() {
  return source_site_instance_.get();
}

bool NavigationRequest::IsRendererInitiated() {
  return !commit_params_->is_browser_initiated;
}

blink::mojom::NavigationInitiatorActivationAndAdStatus
NavigationRequest::GetNavigationInitiatorActivationAndAdStatus() {
  return begin_params_->initiator_activation_and_ad_status;
}

bool NavigationRequest::IsSameOrigin() {
  DCHECK(HasCommitted());
  return same_origin_;
}

bool NavigationRequest::WasServerRedirect() {
  return was_redirected_;
}

const std::vector<GURL>& NavigationRequest::GetRedirectChain() {
  return redirect_chain_;
}

base::TimeTicks NavigationRequest::NavigationStart() {
  return common_params().navigation_start;
}

base::TimeTicks NavigationRequest::NavigationInputStart() {
  return common_params().input_start;
}

const NavigationHandleTiming& NavigationRequest::GetNavigationHandleTiming() {
  return navigation_handle_timing_;
}

bool NavigationRequest::IsPost() {
  return common_params().method == "POST";
}

std::string NavigationRequest::GetRequestMethod() {
  return request_method_;
}

const blink::mojom::Referrer& NavigationRequest::GetReferrer() {
  return *sanitized_referrer_;
}

void NavigationRequest::SetReferrer(blink::mojom::ReferrerPtr referrer) {
  DCHECK(state_ == WILL_START_REQUEST || state_ == WILL_REDIRECT_REQUEST);
  sanitized_referrer_ =
      Referrer::SanitizeForRequest(common_params_->url, *referrer);
  common_params_->referrer = sanitized_referrer_.Clone();
}

bool NavigationRequest::HasUserGesture() {
  return common_params().has_user_gesture;
}

ui::PageTransition NavigationRequest::GetPageTransition() {
  return ui::PageTransitionFromInt(common_params().transition);
}

NavigationUIData* NavigationRequest::GetNavigationUIData() {
  return navigation_ui_data_.get();
}

net::Error NavigationRequest::GetNetErrorCode() {
  return net_error_;
}

// The RenderFrameHost that will commit the navigation or an error page.
// This is computed when the response is received, or when the navigation
// fails and error page should be displayed.
RenderFrameHostImpl* NavigationRequest::GetRenderFrameHost() const {
  // Only allow the RenderFrameHost to be retrieved once it has been set for
  // this navigation. This will happens either at WillProcessResponse time for
  // regular navigations or at WillFailRequest time for error pages.
  // NavigationRequests created for synchronous renderer commits (see
  // documentation for |is_synchronous_renderer_commit_|) have a
  // RenderFrameHost available from the start.
  if (!is_synchronous_renderer_commit()) {
    CHECK_GE(state_, WILL_PROCESS_RESPONSE)
        << "This accessor should only be called after a RenderFrameHost has "
           "been picked for this navigation.";
  }
  static_assert(WILL_FAIL_REQUEST > WILL_PROCESS_RESPONSE,
                "WillFailRequest state should come after WillProcessResponse");
  if (HasRenderFrameHost()) {
    return &*render_frame_host_.value();
  }
  return nullptr;
}

NavigationRequest::AssociatedRenderFrameHostType
NavigationRequest::GetAssociatedRFHType() const {
  // `associated_rfh_type_` might not be accurate after the navigation had
  // moved to the RFH. This is because if another navigation had committed and
  // committed a new RFH that replaces the current RFH, the
  // `associated_rfh_type_` might be stale and needs to be updated. However,
  // we only update the value for non-pending commit navigations (i.e. the
  // NavigationRequest owned by the FrameTreeNode). See the comments in
  // `RenderFrameHostManager::CommitPendingIfNecessary()` for more details.
  CHECK(state_ < READY_TO_COMMIT || state_ == WILL_FAIL_REQUEST)
      << "Use GetRenderFrameHost() instead when the final RenderFrameHost "
         "for the navigation has been picked";
  return associated_rfh_type_;
}

void NavigationRequest::SetAssociatedRFHType(
    AssociatedRenderFrameHostType type) {
  if (associated_rfh_type_ != AssociatedRenderFrameHostType::NONE &&
      type == AssociatedRenderFrameHostType::NONE) {
    // If we're transitioning to "NONE" when the previous state was not "NONE",
    // we might have called SetExpectedProcess() before, so reset it now.
    ResetExpectedProcess();
  }
  associated_rfh_type_ = type;
}

const net::HttpRequestHeaders& NavigationRequest::GetRequestHeaders() {
  if (!request_headers_) {
    request_headers_.emplace();
    request_headers_->AddHeadersFromString(begin_params_->headers);
  }
  return *request_headers_;
}

const std::optional<net::SSLInfo>& NavigationRequest::GetSSLInfo() {
  return ssl_info_;
}

const std::optional<net::AuthChallengeInfo>&
NavigationRequest::GetAuthChallengeInfo() {
  return auth_challenge_info_;
}

net::ResolveErrorInfo NavigationRequest::GetResolveErrorInfo() {
  return resolve_error_info_;
}

net::IsolationInfo NavigationRequest::GetIsolationInfo() {
  if (isolation_info_)
    return isolation_info_.value();

  // TODO(crbug.com/40634002): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information.
  return frame_tree_node_->current_frame_host()
      ->ComputeIsolationInfoForNavigation(
          common_params_->url, is_credentialless(), ComputeFencedFrameNonce());
}

bool NavigationRequest::HasSubframeNavigationEntryCommitted() {
  DCHECK(!frame_tree_node_->IsMainFrame());
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return subframe_entry_committed_;
}

bool NavigationRequest::DidReplaceEntry() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return did_replace_entry_;
}

bool NavigationRequest::ShouldUpdateHistory() {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return should_update_history_;
}

const GURL& NavigationRequest::GetPreviousPrimaryMainFrameURL() {
  DCHECK(IsInPrimaryMainFrame() ||
         GetParentFrame() && GetParentFrame()->GetPage().IsPrimary());
  return GetPreviousMainFrameURL();
}

const GURL& NavigationRequest::GetPreviousMainFrameURL() const {
  DCHECK(state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE);
  return previous_main_frame_url_;
}

bool NavigationRequest::WasStartedFromContextMenu() {
  return common_params().started_from_context_menu;
}

const GURL& NavigationRequest::GetSearchableFormURL() {
  return begin_params().searchable_form_url;
}

const std::string& NavigationRequest::GetSearchableFormEncoding() {
  return begin_params().searchable_form_encoding;
}

ReloadType NavigationRequest::GetReloadType() const {
  return reload_type_;
}

RestoreType NavigationRequest::GetRestoreType() const {
  return restore_type_;
}

const GURL& NavigationRequest::GetBaseURLForDataURL() {
  return common_params().base_url_for_data_url;
}

const GlobalRequestID& NavigationRequest::GetGlobalRequestID() {
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);
  return request_id_;
}

bool NavigationRequest::IsDownload() {
  return is_download_;
}

bool NavigationRequest::IsFormSubmission() {
  return begin_params().is_form_submission;
}

bool NavigationRequest::WasInitiatedByLinkClick() {
  return begin_params().was_initiated_by_link_click;
}

const std::string& NavigationRequest::GetHrefTranslate() {
  return common_params().href_translate;
}

const std::optional<blink::Impression>& NavigationRequest::GetImpression() {
  return begin_params().impression;
}

const std::optional<blink::LocalFrameToken>&
NavigationRequest::GetInitiatorFrameToken() {
  return initiator_frame_token_;
}

int NavigationRequest::GetInitiatorProcessId() {
  return initiator_process_id_;
}

const std::optional<url::Origin>& NavigationRequest::GetInitiatorOrigin() {
  return common_params().initiator_origin;
}

const std::optional<GURL>& NavigationRequest::GetInitiatorBaseUrl() {
  return common_params().initiator_base_url;
}

const std::vector<std::string>& NavigationRequest::GetDnsAliases() {
  static const base::NoDestructor<std::vector<std::string>> emptyvector_result;
  return response_head_ ? response_head_->dns_aliases : *emptyvector_result;
}

bool NavigationRequest::IsSameProcess() {
  return is_same_process_;
}

NavigationEntry* NavigationRequest::GetNavigationEntry() const {
  if (nav_entry_id_ == 0)
    return nullptr;

  return GetNavigationController()->GetEntryWithUniqueIDIncludingPending(
      nav_entry_id_);
}

int NavigationRequest::GetNavigationEntryOffset() {
  return navigation_entry_offset_;
}

GlobalRenderFrameHostId NavigationRequest::GetPreviousRenderFrameHostId() {
  if (previous_render_frame_host_id_ != GlobalRenderFrameHostId()) {
    CHECK_GE(state_, READY_TO_COMMIT);
    // If `previous_render_frame_host_id_` is set to a non-default value, then
    // the navigation had committed and potentially replaced the previous
    // "current RenderFrameHost", so we return the saved value of that previous
    // RenderFrameHost's ID here.
    return previous_render_frame_host_id_;
  }

  // The navigation hasn't committed yet, so the previous RenderFrameHost is
  // still the current RenderFrameHost. Note that this might be different from
  // the current RFH at NavigationRequest construction time (whose FTN id value
  // is saved in `current_render_frame_host_id_at_construction_`), if another
  // navigation caused a new RenderFrameHost to be committed while this
  // navigation is in progress.
  if (frame_tree_node_->current_frame_host()) {
    return frame_tree_node_->current_frame_host()->GetGlobalId();
  } else {
    // It's possible for `frame_tree_node_->current_frame_host()` to be null if
    // we're in the middle of destructing the navigating FrameTreeNode. In this
    // case, just return `current_render_frame_host_id_at_construction_`.
    return current_render_frame_host_id_at_construction_;
  }
}

int NavigationRequest::GetExpectedRenderProcessHostId() {
  DCHECK_LT(state_, READY_TO_COMMIT);
  return expected_render_process_host_id_;
}

bool NavigationRequest::IsServedFromBackForwardCache() {
  const NavigationRequest& request = *this;
  return request.IsServedFromBackForwardCache();
}

void NavigationRequest::SetIsOverridingUserAgent(bool override_ua) {
  // Only add specific headers when creating a NavigationRequest before the
  // network request is made, not at commit time.
  if (is_synchronous_renderer_commit_)
    return;

  // This code assumes it is only called from DidStartNavigation().
  DCHECK(!ua_change_requires_reload_);

  commit_params_->is_overriding_user_agent = override_ua;
  // The new document, created by this navigation, will be honoring the new
  // value. It will be reflected into its NavigationEntry's when committing the
  // new document at DidCommitNavigation time.

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);
  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    UpdateNavigationRequestClientUaHeaders(
        GetTentativeOriginAtRequestTime(), client_hints_delegate,
        is_overriding_user_agent(), frame_tree_node_, &headers,
        common_params_->url);
  }
  headers.SetHeader(
      net::HttpRequestHeaders::kUserAgent,
      ComputeUserAgentValue(headers, GetUserAgentOverride(), browser_context));
  begin_params_->headers = headers.ToString();
  // |request_headers_| comes from |begin_params_|. Clear |request_headers_| now
  // so that if |request_headers_| are needed, they will be updated.
  request_headers_.reset();
}

void NavigationRequest::SetSilentlyIgnoreErrors() {
  silently_ignore_errors_ = true;
}

void NavigationRequest::SetVisitedLinkSalt(uint64_t salt) {
  commit_params_->visited_link_salt = salt;
}

// static
NavigationRequest* NavigationRequest::From(NavigationHandle* handle) {
  return static_cast<NavigationRequest*>(handle);
}

// static
ReloadType NavigationRequest::NavigationTypeToReloadType(
    blink::mojom::NavigationType type) {
  if (type == blink::mojom::NavigationType::RELOAD)
    return ReloadType::NORMAL;
  if (type == blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE)
    return ReloadType::BYPASSING_CACHE;
  return ReloadType::NONE;
}

bool NavigationRequest::IsNavigationStarted() const {
  return is_navigation_started_;
}

bool NavigationRequest::RequiresInitiatorBasedSourceSiteInstance() const {
  // Browser-initiated navigations can supply an initiator origin without having
  // an associated source SiteInstance (e.g. Android intents handled by Chrome).
  // However, the context menu is a case in which we should have one, so it
  // should still require a source SiteInstance.
  if (commit_params_->is_browser_initiated &&
      !common_params().started_from_context_menu) {
    return false;
  }

  // data: URLs, about:blank and empty URL (which are treated the same as
  // about:blank) navigations that have initiator origins require a source
  // SiteInstance.
  const bool is_data_or_about_or_empty =
      common_params_->url.SchemeIs(url::kDataScheme) ||
      common_params_->url.IsAboutBlank() || common_params_->url.is_empty();

  const bool has_valid_initiator =
      common_params_->initiator_origin &&
      common_params_->initiator_origin->GetTupleOrPrecursorTupleIfOpaque()
          .IsValid();

  return is_data_or_about_or_empty && has_valid_initiator &&
         !dest_site_instance_;
}

void NavigationRequest::SetSourceSiteInstanceToInitiatorIfNeeded() {
  if (source_site_instance_ || !RequiresInitiatorBasedSourceSiteInstance())
    return;

  const auto tuple =
      common_params_->initiator_origin->GetTupleOrPrecursorTupleIfOpaque();
  source_site_instance_ = static_cast<SiteInstanceImpl*>(
      frame_tree_node_->current_frame_host()
          ->GetSiteInstance()
          ->GetRelatedSiteInstance(tuple.GetURL())
          .get());
}

void NavigationRequest::ForceEnableOriginTrials(
    const std::vector<std::string>& trials) {
  DCHECK(!HasCommitted());
  commit_params_->force_enabled_origin_trials = trials;
}

network::CrossOriginEmbedderPolicy
NavigationRequest::ComputeCrossOriginEmbedderPolicy() {
  const auto& url = common_params_->url;
  // Fenced Frames should respect the outer frame's COEP.
  RenderFrameHostImpl* const parent = GetParentFrameOrOuterDocument();
  bool is_fenced_frame_from_local_scheme =
      GetNavigatingFrameType() == FrameType::kFencedFrameRoot &&
      (url.SchemeIsBlob() || url.SchemeIs(url::kDataScheme));

  // Some special URLs not loaded using the network inherit the
  // Cross-Origin-Embedder-Policy header from their parent.
  if (parent && (GetContentClient()
                     ->browser()
                     ->ShouldInheritCrossOriginEmbedderPolicyImplicitly(url) ||
                 is_fenced_frame_from_local_scheme)) {
    return parent->cross_origin_embedder_policy();
  }

  // Compute "topLevelCreationURL" for COEP and secure context.
  //
  // [spec]: https://html.spec.whatwg.org/C/#initialise-the-document-object
  // 3. Let creationURL be navigationParams's response's URL.
  // 5. If browsingContext is still on its initial about:blank Document [...]
  // 6. Otherwise:
  // 6.6. Let topLevelCreationURL be creationURL.
  // 6.8. If browsingContext is not a top-level browsing context, then:
  // 6.8.2. Set topLevelCreationURL to parentEnvironment's top-level creation
  //        URL.
  //
  // TODO(arthursonzogni): It would be good to clarify what
  // |topLevelCreationURL| means when loading FencedFrame//GuestView, and how it
  // should affect COEP.
  // Tracking bug:
  // - FencedFrame: https://crbug.com/1277430
  // - GuestView: XXX or slightly related https://crbug.com/1260747
  const GURL& top_level_creation_url = GetParentFrameOrOuterDocument()
                                           ? GetParentFrameOrOuterDocument()
                                                 ->GetOutermostMainFrame()
                                                 ->GetLastCommittedURL()
                                           : url;
  // [spec]: https://html.spec.whatwg.org/C/#obtain-an-embedder-policy
  //
  // 1. Let policy be a new embedder policy.
  // 2. If environment is a non-secure context, then return policy.
  if (network::IsUrlPotentiallyTrustworthy(top_level_creation_url)) {
    if (response_head_) {
      return response_head_->parsed_headers->cross_origin_embedder_policy;
    }
  }
  return network::CrossOriginEmbedderPolicy();
}

// [spec]:
// https://html.spec.whatwg.org/C/#check-a-navigation-response's-adherence-to-its-embedder-policy
//
// Return whether the child's |coep| is compatible with its parent's COEP. It
// also sends COEP reports if needed.
bool NavigationRequest::CheckResponseAdherenceToCoep(const GURL& url) {
  const auto& coep =
      policy_container_builder_->FinalPolicies().cross_origin_embedder_policy;

  // Fenced Frames should respect the outer frame's COEP.
  RenderFrameHostImpl* const parent = GetParentFrameOrOuterDocument();

  // [spec]: 1. If target is not a child browsing context, then return true.
  if (!parent)
    return true;

  // [spec]: 2. Let parentPolicy be target's container document's policy
  //            container's embedder policy.
  const auto& parent_coep = parent->cross_origin_embedder_policy();
  CrossOriginEmbedderPolicyReporter* parent_coep_reporter =
      parent->coep_reporter();

  // [spec]: 3. If parentPolicy's report-only value is compatible with
  // cross-origin isolation and responsePolicy's value is not, then queue a
  // cross-origin embedder policy inheritance violation [...].
  if (CoepBlockIframe(parent_coep.report_only_value, coep.value,
                      is_credentialless())) {
    if (parent_coep_reporter) {
      parent_coep_reporter->QueueNavigationReport(redirect_chain_[0],
                                                  /*report_only=*/true);
    }
  }

  // [spec]: 4. If parentPolicy's value is not compatible with cross-origin
  // isolation or responsePolicy's value is compatible with cross-origin
  // isolation, then return true.
  if (!CoepBlockIframe(parent_coep.value, coep.value, is_credentialless()))
    return true;

  // [spec]: 5 Queue a cross-origin embedder policy inheritance violation with
  // response, "navigation", parentPolicy's reporting endpoint, "enforce", and
  // target's container document's relevant settings object.
  if (parent_coep_reporter) {
    parent_coep_reporter->QueueNavigationReport(redirect_chain_[0],
                                                /*report_only=*/false);
  }

  // [spec]: 6. Return false.
  return false;
}

std::optional<network::mojom::BlockedByResponseReason>
NavigationRequest::EnforceCOEP() {
  // https://html.spec.whatwg.org/C/#check-a-navigation-response's-adherence-to-its-embedder-policy
  // Spec should be updated:
  // https://github.com/shivanigithub/fenced-frame/issues/11

  // Fenced frames should be treated as an embedded frame, thus COEP must apply.
  RenderFrameHostImpl* const parent_frame = GetParentFrameOrOuterDocument();
  if (!parent_frame) {
    return std::nullopt;
  }
  if (is_credentialless()) {
    return std::nullopt;
  }
  const auto& url = common_params_->url;
  // Some special URLs not loaded using the network are inheriting the
  // Cross-Origin-Embedder-Policy header from their parent.
  if (url.SchemeIsBlob() || url.SchemeIs(url::kDataScheme)) {
    return std::nullopt;
  }
  return network::CrossOriginResourcePolicy::IsNavigationBlocked(
      url, redirect_chain_[0], parent_frame->GetLastCommittedOrigin(),
      *response_head_, request_destination(),
      parent_frame->cross_origin_embedder_policy(),
      parent_frame->coep_reporter());
}

bool NavigationRequest::CoopCoepSanityCheck() {
  // Same-document navigations simply reuse the current document and do not use
  // the PolicyContainer, which may contain erroneous information. For example
  // in pushState/popState history navigations. See https://crbug.com/1413081.
  if (IsSameDocument()) {
    return true;
  }

  // Credentialless iframes allow frames to be crossOriginIsolated without ever
  // setting COEP, so the below check does not apply.
  if (is_credentialless_) {
    return true;
  }

  const PolicyContainerPolicies& policies =
      policy_container_builder_->FinalPolicies();
  // Use GetParentFrameOrOuterDocument() to respect the outer frame's COEP for
  // now.
  network::mojom::CrossOriginOpenerPolicyValue coop_value =
      GetParentFrameOrOuterDocument()
          ? GetRenderFrameHost()
                ->GetOutermostMainFrame()
                ->cross_origin_opener_policy()
                .value
          : policies.cross_origin_opener_policy.value;

  if (coop_value ==
          network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep &&
      !CompatibleWithCrossOriginIsolated(
          policies.cross_origin_embedder_policy)) {
    NOTREACHED_IN_MIGRATION();
    base::debug::DumpWithoutCrashing();
    return false;
  }
  return true;
}

bool NavigationRequest::IsFencedFrameRequiredPolicyFeatureAllowed(
    const url::Origin& origin,
    const blink::mojom::PermissionsPolicyFeature feature) {
  const blink::PermissionsPolicyFeatureList& feature_list =
      blink::GetPermissionsPolicyFeatureList(origin);

  // Check if the outer document's permissions policies allow all of the
  // required policies.
  std::optional<const blink::PermissionsPolicy::Allowlist> embedder_allowlist =
      GetParentFrameOrOuterDocument()
          ->permissions_policy()
          ->GetAllowlistForFeatureIfExists(feature);
  if (embedder_allowlist && !embedder_allowlist->MatchesAll()) {
    return false;
  }

  // Check if the container policies to be committed allow all of the required
  // policies for `origin`. This means that the policy must be either
  // explicitly enabled for `origin`, or the policy must by default
  // be enabled for all origins. Note: because the policies have not been
  // read into a RenderFrameHost's permissions_policy_ yet, we need to check
  // the ParsedPermissionsPolicyDeclaration object directly.
  auto policy_iter = std::find_if(
      commit_params_->frame_policy.container_policy.begin(),
      commit_params_->frame_policy.container_policy.end(),
      [feature](const blink::ParsedPermissionsPolicyDeclaration& d) {
        return d.feature == feature;
      });
  if (policy_iter == commit_params_->frame_policy.container_policy.end()) {
    return feature_list.at(feature) ==
           blink::PermissionsPolicyFeatureDefault::EnableForAll;
  }

  return policy_iter->Contains(origin);
}

bool NavigationRequest::CheckPermissionsPoliciesForFencedFrames(
    const url::Origin& origin) {
  // These checks only apply to fenced frames.
  if (!frame_tree_node_->IsFencedFrameRoot())
    return true;

  const std::optional<FencedFrameProperties>& computed_fenced_frame_properties =
      ComputeFencedFrameProperties();

  // Permissions policies only need to be checked for fenced frames created from
  // an API like FLEDGE or Shared Storage.
  if (!computed_fenced_frame_properties) {
    return true;
  }

  // Check that all of the required policies for a new document with origin
  // `origin` in the fenced frame are allowed. This looks at the outer
  // document's policies and the "allow" attribute. Note that the document will
  // eventually only use the required policies without policy inheritance, so
  // extra policies defined in the outer document/"allow" attribute won't have
  // any effect.
  for (const blink::mojom::PermissionsPolicyFeature feature :
       computed_fenced_frame_properties->effective_enabled_permissions()) {
    if (!IsFencedFrameRequiredPolicyFeatureAllowed(origin, feature)) {
      const blink::PermissionsPolicyFeatureToNameMap& feature_to_name_map =
          blink::GetPermissionsPolicyFeatureToNameMap();
      const std::string feature_string(feature_to_name_map.at(feature));
      frame_tree_node_->current_frame_host()->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          base::StringPrintf(
              "Refused to frame '%s' as a fenced frame because permissions "
              "policy '%s' is not allowed for the frame's origin.",
              origin.Serialize().c_str(), feature_string.c_str()));
      return false;
    }
  }
  return true;
}

std::unique_ptr<input::PeakGpuMemoryTracker>
NavigationRequest::TakePeakGpuMemoryTracker() {
  return std::move(loading_mem_tracker_);
}

std::unique_ptr<NavigationEarlyHintsManager>
NavigationRequest::TakeEarlyHintsManager() {
  return std::move(early_hints_manager_);
}

network::mojom::ClientSecurityStatePtr
NavigationRequest::BuildClientSecurityStateForNavigationFetch() {
  switch (GetNavigatingFrameType()) {
    // The client [1] of the navigation fetch request is the navigation
    // initiator, so use the initiator's policies to set the
    // `ClientSecurityState`.
    //
    // [1] https://fetch.spec.whatwg.org/#concept-request-client
    //
    // The `kPrimaryMainFrame` case also covers guest views
    // (https://crbug.com/1261928) since they do not use MPArch.
    //
    // TODO(crbug.com/40258826): Determine how to treat guest views.
    case FrameType::kPrimaryMainFrame:
    case FrameType::kSubframe: {
      if (!policy_container_builder_->InitiatorPolicies()) {
        return nullptr;
      }

      network::mojom::ClientSecurityStatePtr state = DeriveClientSecurityState(
          *policy_container_builder_->InitiatorPolicies(),
          PrivateNetworkRequestContext::kNavigation);

      // Remove the initiator's COEP, it is unused. For iframes, the parent's
      // COEP should be used: that is checked in `EnforceCOEP()`. The value
      // in `ClientSecurityState` is used for subresources only, in which case
      // the network service performs the check on behalf of the client.
      state->cross_origin_embedder_policy =
          network::CrossOriginEmbedderPolicy();

      return state;
    }

    // Fenced frames can only be navigated in two ways:
    //
    // 1. By the embedder document, via the fencedframe.config attribute.
    //    The implementation uses the parent policies directly, because
    //    initiator policies are not currently plumbed in this case. This is
    //    correct anyway because the initiator is the parent.
    //    Note: contrary to an iframe, the navigation can never happens at
    //    distance, using e.g. `window.open(url, target)` or `<a target>`.
    //
    // 2. By a document in the <fencedframe> frame tree. In this case the
    //    initiator policies are properly plumbed and should be used.
    //    TODO(crbug.com/40258851): Use the initiator policies. On can
    //    use `is_embedder_initiated_fenced_frame_navigation_` to discriminate
    //    (1) from (2).
    //
    // NOTE: For an embedder initiated fenced frame navigation that is subject
    // to private network access checks:
    //
    // 1. The preflight request is sent with an opaque origin: "Origin: null".
    //    See: `FencedFrame::Navigate()`.
    // 2. The credentials mode of the preflight request is "include". This
    //    prevents response header `Access-Control-Allow-Origin: '*'` from
    //    working. The response header must explicitly specify the origin.
    // 3. However, we cannot know the origin because of (1).
    // 4. It is also unsafe to respond to the preflight with response header
    //    `Access-Control-Allow-Origin: 'null'`. See:
    //    https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Access-Control-Allow-Origin
    //
    // This implies there is a limitation for fenced frames that send a
    // preflight request because of private network access. Fenced frame
    // embedder initiated private network accesses always fail.
    //
    // NOTE: Fenced frames always have an outer document,
    // `GetParentFrameOrOuterDocument()` is never nullptr.
    case FrameType::kFencedFrameRoot: {
      auto client_security_state =
          GetParentFrameOrOuterDocument()->BuildClientSecurityState();

      // TODO(crbug.com/40258851): Remove COEP from
      // `client_security_state`, see the reasoning for subframes above.

      // TODO(crbug.com/40258851): Consider enabling PNA for fenced
      // frames independently of PNA for iframes.
      client_security_state->private_network_request_policy =
          DerivePrivateNetworkRequestPolicy(
              client_security_state->ip_address_space,
              client_security_state->is_web_secure_context,
              PrivateNetworkRequestContext::kNavigation);

      return client_security_state;
    }

    // TODO(crbug.com/40258824): Determine how to treat prerendered
    // main frames.
    case FrameType::kPrerenderMainFrame:
      return nullptr;
  }
}

network::mojom::ClientSecurityStatePtr
NavigationRequest::BuildClientSecurityStateForCommittedDocument() {
  const PolicyContainerPolicies& policies =
      policy_container_builder_->FinalPolicies();

  return network::mojom::ClientSecurityState::New(
      policies.cross_origin_embedder_policy, policies.is_web_secure_context,
      policies.ip_address_space, private_network_request_policy_,
      policies.document_isolation_policy);
}

std::string NavigationRequest::GetUserAgentOverride() {
  return is_overriding_user_agent() ? frame_tree_node_->navigator()
                                          .GetDelegate()
                                          ->GetUserAgentOverride()
                                          .ua_string_override
                                    : std::string();
}

NavigationControllerImpl* NavigationRequest::GetNavigationController() const {
  return &frame_tree_node_->navigator().controller();
}

PrerenderHostRegistry& NavigationRequest::GetPrerenderHostRegistry() {
  PrerenderHostRegistry* registry = frame_tree_node_->current_frame_host()
                                        ->delegate()
                                        ->GetPrerenderHostRegistry();
  CHECK(registry);
  return *registry;
}

mojo::PendingRemote<network::mojom::CookieAccessObserver>
NavigationRequest::CreateCookieAccessObserver() {
  mojo::PendingRemote<network::mojom::CookieAccessObserver> remote;
  cookie_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
NavigationRequest::CreateTrustTokenAccessObserver() {
  mojo::PendingRemote<network::mojom::TrustTokenAccessObserver> remote;
  trust_token_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
NavigationRequest::CreateSharedDictionaryAccessObserver() {
  mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver> remote;
  shared_dictionary_observers_.Add(this,
                                   remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void NavigationRequest::OnCookiesAccessed(
    std::vector<network::mojom::CookieAccessDetailsPtr> details_vector) {
  TRACE_EVENT_WITH_FLOW0("navigation", "NavigationRequest::OnCookiesAccessed",
                         TRACE_ID_WITH_SCOPE(kNavigationRequestScope,
                                             TRACE_ID_LOCAL(navigation_id_)),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  for (auto& details : details_vector) {
    // TODO(crbug.com/40520047): We should not send information to the current
    // frame about (potentially unrelated) ongoing navigation, but at the moment
    // we don't have another way to add messages to DevTools console.
    EmitCookieWarningsAndMetrics(frame_tree_node()->current_frame_host(),
                                 /*navigation_request=*/this, details);

    CookieAccessDetails allowed;
    CookieAccessDetails blocked;
    SplitCookiesIntoAllowedAndBlocked(details, &allowed, &blocked);
    if (!allowed.cookie_access_result_list.empty()) {
      GetDelegate()->OnCookiesAccessed(this, allowed);
    }
    if (!blocked.cookie_access_result_list.empty()) {
      GetDelegate()->OnCookiesAccessed(this, blocked);
    }

    // When determining the BFCache eligibility, we explicitly ignore the cookie
    // changes from the navigation itself because we want the
    // `CookieChangeListener` to only track the cookie changes that potentially
    // make the document initially rendered by the navigation request outdated.
    if (allowed.type == CookieAccessDetails::Type::kChange) {
      uint64_t cookie_modification_count =
          allowed.cookie_access_result_list.size();
      uint64_t http_only_cookie_modification_count = 0u;
      for (const net::CookieWithAccessResult& cookie_with_access_result :
           allowed.cookie_access_result_list) {
        if (cookie_with_access_result.cookie.IsHttpOnly()) {
          http_only_cookie_modification_count++;
        }
      }
      if (cookie_change_listener_) {
        cookie_change_listener_->RemoveNavigationCookieModificationCount(
            base::PassKey<NavigationRequest>(), cookie_modification_count,
            http_only_cookie_modification_count);
      }
    }
  }
}

void NavigationRequest::Clone(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> observer) {
  cookie_observers_.Add(this, std::move(observer));
}

std::vector<mojo::PendingReceiver<network::mojom::CookieAccessObserver>>
NavigationRequest::TakeCookieObservers() {
  return cookie_observers_.TakeReceivers();
}

void NavigationRequest::OnTrustTokensAccessed(
    network::mojom::TrustTokenAccessDetailsPtr details) {
  GetDelegate()->OnTrustTokensAccessed(this, TrustTokenAccessDetails(details));
}

void NavigationRequest::Clone(
    mojo::PendingReceiver<network::mojom::TrustTokenAccessObserver> observer) {
  trust_token_observers_.Add(this, std::move(observer));
}

std::vector<mojo::PendingReceiver<network::mojom::TrustTokenAccessObserver>>
NavigationRequest::TakeTrustTokenObservers() {
  return trust_token_observers_.TakeReceivers();
}

void NavigationRequest::OnSharedDictionaryAccessed(
    network::mojom::SharedDictionaryAccessDetailsPtr details) {
  GetDelegate()->OnSharedDictionaryAccessed(this, *details);
}

void NavigationRequest::Clone(
    mojo::PendingReceiver<network::mojom::SharedDictionaryAccessObserver>
        observer) {
  shared_dictionary_observers_.Add(this, std::move(observer));
}

std::vector<
    mojo::PendingReceiver<network::mojom::SharedDictionaryAccessObserver>>
NavigationRequest::TakeSharedDictionaryAccessObservers() {
  return shared_dictionary_observers_.TakeReceivers();
}

RenderFrameHostImpl* NavigationRequest::GetInitiatorDocumentRenderFrameHost() {
  return initiator_document_token_
             ? RenderFrameHostImpl::FromDocumentToken(
                   initiator_process_id_, *initiator_document_token_)
             : nullptr;
}

void NavigationRequest::RecordAddressSpaceFeature() {
  DCHECK(response_head_);
  DCHECK(policy_container_builder_);

  RenderFrameHostImpl* initiator_render_frame_host =
      GetInitiatorDocumentRenderFrameHost();
  if (!initiator_render_frame_host) {
    // The initiator document is no longer available, so we cannot log a feature
    // use against it. This case may result in a slight undercounting, but is
    // expected to be rare enough that it should not matter for compat risk
    // evaluation.
    return;
  }

  // If there is an initiator document, then `initiator_frame_token_` should
  // have a value, and thus there should be initiator policies.
  const PolicyContainerPolicies* initiator_policies =
      policy_container_builder_->InitiatorPolicies();
  DCHECK(initiator_policies);
  if (!initiator_policies) {
    base::debug::DumpWithoutCrashing();  // Just in case.
    return;
  }

  std::optional<blink::mojom::WebFeature> optional_feature =
      blink::AddressSpaceFeature(blink::FetchType::kNavigation,
                                 initiator_policies->ip_address_space,
                                 initiator_policies->is_web_secure_context,
                                 response_head_->response_address_space);
  if (!optional_feature.has_value()) {
    return;
  }

  ContentBrowserClient* client = GetContentClient()->browser();
  client->LogWebFeatureForCurrentPage(initiator_render_frame_host,
                                      *optional_feature);
  client->LogWebFeatureForCurrentPage(
      initiator_render_frame_host,
      IsInOutermostMainFrame()
          ? blink::mojom::WebFeature::kPrivateNetworkAccessFetchedTopFrame
          : blink::mojom::WebFeature::kPrivateNetworkAccessFetchedSubFrame);
}

void NavigationRequest::ComputePoliciesToCommit() {
  const auto& url = common_params_->url;

  network::mojom::IPAddressSpace response_address_space =
      CalculateIPAddressSpace(url, response_head_.get(),
                              GetContentClient()->browser());
  policy_container_builder_->SetIPAddressSpace(response_address_space);

  if (!devtools_instrumentation::ShouldBypassCSP(*this)) {
    if (response_head_) {
      policy_container_builder_->AddContentSecurityPolicies(
          mojo::Clone(response_head_->parsed_headers->content_security_policy));
    }
  }

  // Use the unchecked / non-sandboxed origin to calculate potential
  // trustworthiness. Indeed, the potential trustworthiness check should apply
  // to the origin of the creation URL, prior to opaquification.
  policy_container_builder_->SetIsOriginPotentiallyTrustworthy(
      network::IsOriginPotentiallyTrustworthy(
          GetOriginForURLLoaderFactoryUnchecked()));

  policy_container_builder_->SetCrossOriginEmbedderPolicy(
      ComputeCrossOriginEmbedderPolicy());

  // If the navigation is the result of a network response, set DIP to the
  // one in the network response. Otherwise, DIP should follow normal rules of
  // policy inheritance, which should be handled by the policy container.
  if (response_head_) {
    SanitizeDocumentIsolationPolicyHeader();
    policy_container_builder_->SetDocumentIsolationPolicy(
        response_head_->parsed_headers->document_isolation_policy);
  }

  // COOP origins always match after a main frame navigation, so we only need
  // to check sub frames. The main frame could be about:blank so we still might
  // inherit `true`.
  policy_container_builder_->SetAllowCrossOriginIsolation(
      IsInMainFrame() || GetParentFrame()
                             ->policy_container_host()
                             ->policies()
                             .allow_cross_origin_isolation);

  DCHECK(commit_params_);
  DCHECK(!HasCommitted());
  DCHECK(!IsErrorPage());

  policy_container_builder_->ComputePolicies(
      url, IsMhtmlOrSubframe(), commit_params_->frame_policy.sandbox_flags,
      is_credentialless());
}

void NavigationRequest::ComputePoliciesToCommitForError() {
  CHECK(!IsMhtmlOrSubframe());
  policy_container_builder_->ComputePoliciesForError();
}

void NavigationRequest::CheckStateTransition(NavigationState state) const {
#if DCHECK_IS_ON()
  // See
  // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/navigation-request-navigation-state.png
  // clang-format off
  static const base::NoDestructor<base::StateTransitions<NavigationState>>
      transitions(base::StateTransitions<NavigationState>({
          {NOT_STARTED, {
              WAITING_FOR_RENDERER_RESPONSE,
              WILL_START_NAVIGATION,
              WILL_START_REQUEST,
          }},
          {WAITING_FOR_RENDERER_RESPONSE, {
              WILL_START_NAVIGATION,
              WILL_START_REQUEST,
          }},
          {WILL_START_NAVIGATION, {
              WILL_START_REQUEST,
              WILL_FAIL_REQUEST,
          }},
          {WILL_START_REQUEST, {
              WILL_REDIRECT_REQUEST,
              WILL_PROCESS_RESPONSE,
              WILL_COMMIT_WITHOUT_URL_LOADER,
              READY_TO_COMMIT,
              DID_COMMIT,
              CANCELING,
              WILL_FAIL_REQUEST,
              DID_COMMIT_ERROR_PAGE,
          }},
          {WILL_REDIRECT_REQUEST, {
              WILL_REDIRECT_REQUEST,
              WILL_PROCESS_RESPONSE,
              CANCELING,
              WILL_FAIL_REQUEST,
          }},
          {WILL_PROCESS_RESPONSE, {
              READY_TO_COMMIT,
              CANCELING,
              WILL_FAIL_REQUEST,
          }},
          {WILL_COMMIT_WITHOUT_URL_LOADER, {
              READY_TO_COMMIT,
              CANCELING,
              WILL_FAIL_REQUEST,
          }},
          {READY_TO_COMMIT, {
              NOT_STARTED,
              DID_COMMIT,
              DID_COMMIT_ERROR_PAGE,
          }},
          {CANCELING, {
              READY_TO_COMMIT,
              WILL_FAIL_REQUEST,
          }},
          {WILL_FAIL_REQUEST, {
              READY_TO_COMMIT,
              CANCELING,
              WILL_FAIL_REQUEST,
          }},
          {DID_COMMIT, {}},
          {DID_COMMIT_ERROR_PAGE, {}},
      }));
  // clang-format on
  DCHECK_STATE_TRANSITION(transitions, state_, state);
#endif  // DCHECK_IS_ON()
}

void NavigationRequest::SetState(NavigationState state) {
  CheckStateTransition(state);
  state_ = state;
}

bool NavigationRequest::MaybeCancelFailedNavigation() {
  // TODO(crbug.com/41349746): Maybe take `ThrottleCheckResult::action()` into
  // account as well.
  // If the request was canceled by the user, do not show an error page.
  if (net::ERR_ABORTED == net_error_ ||
      // Some embedders suppress error pages to allow custom error handling.
      silently_ignore_errors_ ||
      // <webview> guests suppress net::ERR_BLOCKED_BY_CLIENT.
      (net::ERR_BLOCKED_BY_CLIENT == net_error_ &&
       silently_ignore_blocked_by_client_)) {
    frame_tree_node_->ResetNavigationRequest(
        NavigationDiscardReason::kInternalCancellation);
    return true;
  }

  // Per https://whatwg.org/C/iframe-embed-object.html#the-object-element,
  // this implements step 4.7 from "determine what the object element
  // represents": "If the load failed (e.g. there was an HTTP 404 error, there
  // was a DNS error), fire an event named error at the element, then jump to
  // the step below labeled fallback."
  //
  // This case handles navigation failure, e.g. due to the navigation being
  // blocked by WebRequest, DNS errors, et cetera.
  if (frame_tree_node()->frame_owner_element_type() ==
      blink::FrameOwnerElementType::kObject) {
    RenderFallbackContentForObjectTag();
    frame_tree_node_->ResetNavigationRequest(
        NavigationDiscardReason::kInternalCancellation);
    return true;
  }

  return false;
}

bool NavigationRequest::ShouldRenderFallbackContentForResponse(
    const net::HttpResponseHeaders& http_headers) const {
  return frame_tree_node()->frame_owner_element_type() ==
             blink::FrameOwnerElementType::kObject &&
         !network::IsSuccessfulStatus(http_headers.response_code());
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#navigating-across-documents:hh-replace
bool NavigationRequest::ShouldReplaceCurrentEntryForSameUrlNavigation() const {
  DCHECK_LE(state_, WILL_START_NAVIGATION);

  // Not a same-url navigation. Note that this is comparing against the "last
  // loading URL" since this is what was used in the renderer check that was
  // moved here. This means for error pages we should compare against the URL
  // that failed to load (the last committed URL), while for other navigations
  // we should compare against the last document URL, which might be different
  // from the last committed URL due to document.open() changing the URL.
  if (common_params_->url !=
      GetLastLoadingURLInRendererForNavigationReplacement(
          frame_tree_node_->current_frame_host())) {
    return false;
  }

  if (IsLoadDataWithBaseURL()) {
    // Preserve old behavior of loadDataWithBaseURL() navigations, which almost
    // never does same-URL replacement when the (data) URL is the same, since it
    // used to compare the data URL against the "history URL", which in almost
    // all cases wouldn't match the data: URL (in most cases it's either the
    // same as the base/document URL, or about:blank [the default value], see
    // https://crbug.com/1244746#c1 for more details).
    return false;
  }

  // Never replace if there is no NavigationEntry to replace.
  if (!frame_tree_node_->navigator().controller().GetEntryCount())
    return false;

  // The NavigationAPI allows a page to request a navigation that pushes even in
  // situations where the browser would implicitly convert the navigation to
  // a replace.
  if (begin_params_->force_history_push ==
      blink::mojom::ForceHistoryPush::kYes) {
    return false;
  }

  // Reloads and history navigations have special handling and don't need to
  // set |common_params_->should_replace_current_entry|.
  if (common_params_->navigation_type !=
      blink::mojom::NavigationType::DIFFERENT_DOCUMENT) {
    return false;
  }
  // Form submissions to the same url should not replace.
  if (begin_params_->is_form_submission)
    return false;

  // If the initiating frame is cross-origin to the target frame, do not
  // replace. Replacing in this case can be used to guess the exact current url
  // of a cross-origin frame, see https://crbug.com/1208614. Exempt error pages
  // from this rule so that we don't leave an error page in the back/forward
  // list if a cross-origin iframe happens to successfully re-naivgate a frame
  // that had previously failed.
  if (!frame_tree_node_->current_frame_host()->IsErrorDocument() &&
      common_params_->initiator_origin &&
      !common_params_->initiator_origin->IsSameOriginWith(
          frame_tree_node_->current_origin())) {
    return false;
  }

  // Otherwise, replace current entry.
  return true;
}

bool NavigationRequest::
    ShouldReplaceCurrentEntryForNavigationFromInitialEmptyDocumentOrEntry()
        const {
  DCHECK_LE(state_, WILL_START_NAVIGATION);
  // Never replace if there is no NavigationEntry to replace.
  if (!frame_tree_node_->navigator().controller().GetEntryCount())
    return false;

  if (common_params_->navigation_type !=
          blink::mojom::NavigationType::SAME_DOCUMENT &&
      common_params_->navigation_type !=
          blink::mojom::NavigationType::DIFFERENT_DOCUMENT) {
    // History navigations, session restore, and reloads shouldn't do
    // replacement.
    return false;
  }

  // Check the "initial NavigationEntry" status and the "initial empty document"
  // status.

  if (frame_tree_node_->navigator()
          .controller()
          .GetLastCommittedEntry()
          ->IsInitialEntry()) {
    // Initial NavigationEntry must always be replaced, to ensure that as long
    // as the NavigationEntry exists, it will be the only NavigationEntry in
    // the session history list, making history navigations to initial
    // NavigationEntry possible. See the comment for `is_initial_entry_` in
    // NavigationEntryImpl for more details.
    return true;
  }

  // For non-initial NavigationEntries, the initial empty document should also
  // be replaced in subframes and non-outermost main frames (fenced frames).
  // For outermost main frames, the initial empty document will usually only
  // exist when on the initial NavigationEntry, but it can also exist in a
  // restored or cloned NavigationController before the first commit. It is
  // important not to replace one of the restored entries in that case. See
  // https://crbug.com/1284566 and https://crbug.com/1295723.
  return frame_tree_node_->is_on_initial_empty_document() &&
         frame_tree_node_->GetParentOrOuterDocument();
}

bool NavigationRequest::ShouldReplaceCurrentEntryForFailedNavigation() const {
  DCHECK(state_ == CANCELING || state_ == WILL_FAIL_REQUEST);

  if (common_params_->should_replace_current_entry)
    return true;

  // Never replace if there is no NavigationEntry to replace.
  if (!frame_tree_node_->navigator().controller().GetEntryCount())
    return false;

  auto page_state =
      blink::PageState::CreateFromEncodedData(commit_params_->page_state);
  // Failed history navigations with valid PageState should not do replacement.
  if (page_state.IsValid())
    return false;

  bool is_reload_or_history =
      NavigationTypeUtils::IsReload(common_params_->navigation_type) ||
      NavigationTypeUtils::IsHistory(common_params_->navigation_type);
  // Otherwise, these navigations should do replacement:
  // - Failed history/reloads with invalid PageState (e.g. same-URL navigations
  //   that got converted to a reload).
  // - Same-URL navigations. Note that even though we had a same-URL check
  //   earlier in the navigation's lifetime (which would convert same-URL
  //   navigations to reload or replacement), those compare against the initial
  //   URL instead of the final URL, which is what we're using here. Also, this
  //   is using the "loading URL", since that is the URL that was used in the
  //   renderer before we moved the replacement conversion here.
  // TODO(crbug.com/40755155): Reconsider whether these two cases should
  // do replacement or not, since we're just preserving old behavior here.
  return is_reload_or_history ||
         (common_params_->url ==
          GetLastLoadingURLInRendererForNavigationReplacement(
              frame_tree_node_->current_frame_host()));
}

const std::optional<FencedFrameProperties>&
NavigationRequest::ComputeFencedFrameProperties(
    FencedFramePropertiesNodeSource node_source) const {
  if (node_source == FencedFramePropertiesNodeSource::kFrameTreeRoot &&
      !frame_tree_node_->IsFencedFrameRoot()) {
    // Sometimes nodes other than the frame tree root (urn iframes) have
    // FencedFrameProperties in their navigation requests. When the node source
    // is kFrameTreeRoot and this navigation request is not for the frame tree
    // root, get the properties from the frame tree root.
    return frame_tree_node_->GetFencedFrameProperties(node_source);
  }

  if (fenced_frame_properties_) {
    return fenced_frame_properties_;
  }

  return frame_tree_node_->GetFencedFrameProperties(node_source);
}

const std::optional<base::UnguessableToken>
NavigationRequest::ComputeFencedFrameNonce() const {
  // For partition nonce, all nested frame inside a fenced frame tree should
  // operate on the partition nonce of the frame tree root.
  const std::optional<FencedFrameProperties>& computed_fenced_frame_properties =
      ComputeFencedFrameProperties(
          /*node_source=*/FencedFramePropertiesNodeSource::kFrameTreeRoot);
  if (!computed_fenced_frame_properties.has_value()) {
    return std::nullopt;
  }
  if (!computed_fenced_frame_properties->partition_nonce().has_value()) {
    // It is only possible for there to be `FencedFrameProperties` but no
    // partition nonce in urn iframes (which could indeed be nested inside a
    // fenced frame).
    CHECK(blink::features::IsAllowURNsInIframeEnabled());
    return std::nullopt;
  }
  return computed_fenced_frame_properties->partition_nonce()
      ->GetValueIgnoringVisibility();
}

void NavigationRequest::RenderFallbackContentForObjectTag() {
  // https://whatwg.org/C/iframe-embed-object.html#the-object-element:fallback-content-5:
  // Fallback content is represented by the children of the <object> tag, so it
  // will be rendered in the process of the parent's document.
  DCHECK_EQ(blink::FrameOwnerElementType::kObject,
            frame_tree_node_->frame_owner_element_type());
  if (RenderFrameProxyHost* proxy =
          frame_tree_node_->render_manager()->GetProxyToParent()) {
    if (proxy->is_render_frame_proxy_live()) {
      proxy->GetAssociatedRemoteFrame()->RenderFallbackContent();
    }
  } else {
    frame_tree_node_->current_frame_host()
        ->GetAssociatedLocalFrame()
        ->RenderFallbackContent();
  }
}

std::optional<base::UnguessableToken>
NavigationRequest::GetNavigationTokenForDeferringSubframes() {
  DCHECK(IsInMainFrame());
  if (!IsSameDocument() ||
      !NavigationTypeUtils::IsHistory(common_params_->navigation_type)) {
    return std::nullopt;
  }
  RenderFrameHostImpl* current_frame_host =
      frame_tree_node_->current_frame_host();
  if (!current_frame_host->has_navigate_event_handler()) {
    return std::nullopt;
  }
  if (commit_params_->is_browser_initiated &&
      !current_frame_host->IsHistoryUserActivationActive()) {
    return std::nullopt;
  }
  return commit_params_->navigation_token;
}

void NavigationRequest::AddDeferredSubframeNavigationThrottle(
    base::WeakPtr<SubframeHistoryNavigationThrottle> throttle) {
  DCHECK(IsInMainFrame());
  subframe_history_navigation_throttles_.push_back(throttle);
}

void NavigationRequest::UnblockPendingSubframeNavigationRequestsIfNeeded() {
  // After a main frame same-document history navigation completes successfully,
  // we can resume any corresponding subframe history navigations that were
  // blocked on it.
  base::WeakPtr<NavigationRequest> self = GetWeakPtr();
  for (auto& throttle : subframe_history_navigation_throttles_) {
    if (throttle) {
      throttle->Resume();
      if (!self) {
        return;
      }
    }
  }
  subframe_history_navigation_throttles_.clear();
}

void NavigationRequest::MaybeDispatchNavigateEventForCrossDocumentTraversal() {
  // If this is a cross-document history navigation, notify the renderer to
  // fire the navigate event now that we know which frames are navigating and
  // whether the navigation is same-origin. Note that while the navigate event
  // can normally intercept or cancel a navigation, it has neither of those
  // powers for a cross-document history navigation, and therefore can be
  // dispatched without waiting for a result. The worst it can do is detach the
  // frame asynchronously, which javascript could do at any time anyway.
  if (common_params_->navigation_type !=
      blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT) {
    return;
  }
  // Only fire the navigate event if the destination is same-origin. Because
  // this check is performed at navigation start time, `destination_origin` is
  // based on the pre-redirect URL, which is consistent with the renderer
  // process logic for firing the navigate event for non-history navigations.
  url::Origin destination_origin = url::Origin::Resolve(
      common_params_->url,
      common_params_->initiator_origin.value_or(url::Origin()));
  if (!frame_tree_node_->current_origin().IsSameOriginWith(
          destination_origin)) {
    return;
  }
  frame_tree_node_->current_frame_host()
      ->GetAssociatedLocalFrame()
      ->DispatchNavigateEventForCrossDocumentTraversal(
          common_params_->url, commit_params_->page_state,
          commit_params_->is_browser_initiated);
}

bool NavigationRequest::IsServedFromBackForwardCache() const {
  return is_back_forward_cache_restore_;
}

bool NavigationRequest::IsPageActivation() const {
  return const_cast<NavigationRequest*>(this)->IsPrerenderedPageActivation() ||
         IsServedFromBackForwardCache();
}

std::unique_ptr<NavigationEntryImpl>
NavigationRequest::TakePrerenderNavigationEntry() {
  DCHECK(IsPrerenderedPageActivation());
  return std::move(prerender_navigation_state_->prerender_navigation_entry);
}

bool NavigationRequest::IsWaitingForBeforeUnload() {
  return state_ < WILL_START_NAVIGATION;
}

void NavigationRequest::AddDeferredConsoleMessage(
    blink::mojom::ConsoleMessageLevel level,
    std::string message) {
  DCHECK_LE(state_, READY_TO_COMMIT);
  console_messages_.push_back(ConsoleMessage{level, std::move(message)});
}

void NavigationRequest::SendDeferredConsoleMessages() {
  for (auto& message : console_messages_) {
    // TODO(crbug.com/40520047): We should have a way of sending console
    // messaged to devtools without going through the renderer.
    GetRenderFrameHost()->AddMessageToConsole(message.level,
                                              std::move(message.message));
  }
  console_messages_.clear();
}

std::optional<AgentClusterKey::CrossOriginIsolationKey>
NavigationRequest::ComputeCrossOriginIsolationKey() {
  // If the final security policies have not been computed yet, return an empty
  // CrossOriginIsolationKey. This is because we cannot compute the proper
  // CrossOriginIsolationKey for the navigation yet.
  // TODO(crbug.com/343914483): When navigating between same-origin documents,
  // consider passing the CrossOriginIsolationKey of the current document to
  // avoid creating spurious speculative RFH when navigating between two
  // same-origin documents with crossOriginIsolation.
  if (!policy_container_builder_->HasComputedPolicies()) {
    return std::nullopt;
  }

  // If the navigation does not have a Document-Isolation-Policy, return an
  // empty CrossOriginIsolationKey.
  // TODO(crbug.com/342365083): Use a CrossOriginIsolationKey when the
  // navigation has COOP and COEP, or happens in cross-origin isolated
  // BrowsingInstance.
  if (policy_container_builder_->FinalPolicies()
          .document_isolation_policy.value ==
      network::mojom::DocumentIsolationPolicyValue::kNone) {
    return std::nullopt;
  }

  // The document we're navigating to has a DocumentIsolationPolicy of
  // "isolate-and-require-corp" or "isolate-and-credentialless". This means that
  // the document requested crossOriginIsolation, so return a cross-origin
  // isolation key with the current origin. Its cross-origin isolation mode
  // depends on the capabilities of the platform.  Currently, we only support a
  // cross-origin isolation mode of kConcrete and platforms with full Site
  // Isolation.
  // TODO(crbug.com/342364564): Support platforms that do not
  // support OOPIF and return an AgentClusterKey with a CrossOriginIsolationKey
  // that has a kLogical cross-origin isolation mode.
  CHECK(policy_container_builder_->FinalPolicies()
                .document_isolation_policy.value ==
            network::mojom::DocumentIsolationPolicyValue::
                kIsolateAndRequireCorp ||
        policy_container_builder_->FinalPolicies()
                .document_isolation_policy.value ==
            network::mojom::DocumentIsolationPolicyValue::
                kIsolateAndCredentialless);

  // If the navigation doesn't have an origin, we cannot create a
  // CrossOriginIsolationKey for it, since it must be tied to an origin.
  if (!GetOriginToCommit().has_value()) {
    return std::nullopt;
  }

  url::Origin origin = GetOriginToCommit().value();
  return AgentClusterKey::CrossOriginIsolationKey(
      origin, CrossOriginIsolationMode::kConcrete);
}

std::optional<WebExposedIsolationInfo>
NavigationRequest::ComputeWebExposedIsolationInfo() {
  // If we are in an iframe, we inherit the isolation state of the top level
  // frame. This can be inferred from the main frame SiteInstance. Note that
  // Iframes have to pass COEP tests in |OnResponseStarted| before being loaded
  // and inheriting this cross-origin isolated state.
  //
  // Embedded content that cannot always provide a separate process (Fenced
  // frames) should use the crossOriginIsolated state of their
  // parent. Therefore we use IsOutermostMainFrame.
  //
  // TODO(crbug.com/40180791): This may change as we work out the model for
  // isolation mechanisms beyond "cross-origin isolation".
  if (!frame_tree_node_->IsOutermostMainFrame()) {
    return frame_tree_node_->current_frame_host()
        ->GetMainFrame()
        ->GetSiteInstance()
        ->GetWebExposedIsolationInfo();
  }

  // This accommodates for web tests that use COOP. They expect an about:blank
  // page to stay in process, and hang otherwise. In general, it is safe to
  // allow about:blank pages to stay in process, since scriptability is limited
  // to the BrowsingInstance and all pages with the same web-exposed isolation
  // level are trusted.
  if (common_params_->url.IsAboutBlank())
    return std::nullopt;

  // If we haven't yet received a definitive network response, it is too early
  // to guess the isolation state.
  if (state_ < WILL_PROCESS_RESPONSE)
    return std::nullopt;

  // We consider navigations to be cross-origin isolated if the response
  // asserts proper COOP and COEP headers.
  if ((coop_status().current_coop().value !=
       network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep) &&
      (coop_status().current_coop().value !=
       network::mojom::CrossOriginOpenerPolicyValue::
           kRestrictPropertiesPlusCoep)) {
    return WebExposedIsolationInfo::CreateNonIsolated();
  }

  CHECK(coop_status().current_coop().origin.has_value());

  const GURL& url = common_params().url;
  const url::Origin& origin = *coop_status().current_coop().origin;

  return SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
             GetNavigationController()->GetBrowserContext(), url)
             ? WebExposedIsolationInfo::CreateIsolatedApplication(origin)
             : WebExposedIsolationInfo::CreateIsolated(origin);
}

std::optional<url::Origin> NavigationRequest::ComputeCommonCoopOrigin() {
  // Embedded content that cannot set COOP directly should inherit their COOP
  // common origin from their embedder. For iframes, this is to ensure that they
  // do not reuse a SiteInstance in the wrong page. For other embedded
  // content it is simply for consistency as they should never try to get
  // another SiteInstance in the same CoopRelatedGroup anyway. For this reason
  // we use IsOutermostMainFrame.
  if (!frame_tree_node_->IsOutermostMainFrame()) {
    return frame_tree_node_->current_frame_host()
        ->GetMainFrame()
        ->GetSiteInstance()
        ->GetCommonCoopOrigin();
  }

  using CoopValue = network::mojom::CrossOriginOpenerPolicyValue;

  switch (coop_status().current_coop().value) {
    case CoopValue::kSameOrigin:
    case CoopValue::kSameOriginPlusCoep:
    case CoopValue::kRestrictProperties:
    case CoopValue::kNoopenerAllowPopups:
    case CoopValue::kRestrictPropertiesPlusCoep:
      // If we're early in the navigation process and the PolicyContainer was
      // not yet computed, use a best effort origin.
      // TODO(crbug.com/40879437): This is probably not very helpful. If
      // we have a { Value+Origin } COOP bundle, we should be able to return a
      // nullopt value that is distinct from { unsafe-none, nullopt }, similar
      // to what exists for WebExposedIsolationInfo.
      return policy_container_builder_->HasComputedPolicies()
                 ? coop_status().current_coop().origin
                 : GetTentativeOriginAtRequestTime();

    case CoopValue::kUnsafeNone:
    case CoopValue::kSameOriginAllowPopups:
      return std::nullopt;
  };
}

void NavigationRequest::MaybeAssignInvalidPrerenderFrameTreeNodeId() {
  if (!prerender_frame_tree_node_id_.has_value()) {
    // This navigation won't activate a prerendered page. Otherwise,
    // `prerender_frame_tree_node_id_` should have already been set before this
    // in OnPrerenderingActivationChecksComplete().
    prerender_frame_tree_node_id_ = FrameTreeNodeId();
  }
}

void NavigationRequest::RendererCancellationWindowEnded() {
  // The renderer had indicated that the navigation cancellation window had
  // ended, so the navigation can resume if it is currently waiting for this
  // signal.
  renderer_cancellation_window_ended_ = true;
  renderer_cancellation_listener_.reset();
  if (renderer_cancellation_window_ended_callback_) {
    std::move(renderer_cancellation_window_ended_callback_).Run();
    // DO NOT ADD CODE after this. The callback triggers
    // RendererCancellationThrottle::NavigationCancellationWindowEnded() and
    // eventually NavigationThrottle::Resume(), which might have destroyed the
    // NavigationRequest.
  }
}

bool NavigationRequest::ShouldWaitForRendererCancellationWindowToEnd() {
  return renderer_cancellation_listener_.is_bound() &&
         !renderer_cancellation_window_ended_;
}

NavigationRequest::ScopedCrashKeys::ScopedCrashKeys(
    NavigationRequest& navigation_request)
    : initiator_origin_(
          GetNavigationRequestInitiatorCrashKey(),
          base::OptionalToPtr(navigation_request.GetInitiatorOrigin())),
      url_(GetNavigationRequestUrlCrashKey(), navigation_request.GetURL()),
      is_same_document_(
          GetNavigationRequestIsSameDocumentCrashKey(),
          navigation_request.IsSameDocument() ? "same-doc" : "cross-doc") {}

NavigationRequest::ScopedCrashKeys::~ScopedCrashKeys() = default;

PreloadingTriggerType NavigationRequest::GetPrerenderTriggerType() {
  DCHECK(prerender_trigger_type_.has_value());
  return prerender_trigger_type_.value();
}

std::string NavigationRequest::GetPrerenderEmbedderHistogramSuffix() {
  return prerender_embedder_histogram_suffix_;
}

#if BUILDFLAG(IS_ANDROID)
const base::android::JavaRef<jobject>&
NavigationRequest::GetJavaNavigationHandle() {
  return navigation_handle_proxy_->java_navigation_handle();
}
#endif

void NavigationRequest::SetViewTransitionState(
    std::unique_ptr<ScopedViewTransitionResources> resources,
    blink::ViewTransitionState view_transition_state) {
  commit_params_->view_transition_state = std::move(view_transition_state);
  CHECK(resources);
  view_transition_resources_ = std::move(resources);
}

void NavigationRequest::ResetViewTransitionState() {
  if (!commit_params_->view_transition_state) {
    CHECK(!view_transition_resources_);
    return;
  }

  CHECK(view_transition_resources_);
  commit_params_->view_transition_state.reset();
  view_transition_resources_.reset();

  // If we cached a view transition for the old Document and the transition
  // has been aborted, inform the old Document to discard the pending
  // ViewTransition.
  //
  // Note: If the transition is aborted before the renderer acks the
  // snapshot IPC, we won't have any resources here. The
  // ViewTransitionCommitDeferringCondition is responsible for discarding the
  // pending transition in this case.
  if (auto* previous_rfh =
          RenderFrameHostImpl::FromID(GetPreviousRenderFrameHostId())) {
    previous_rfh->GetAssociatedLocalFrame()
        ->NotifyViewTransitionAbortedToOldDocument();
  }
}

bool NavigationRequest::IsDisabledEmbedderInitiatedFencedFrameNavigation() {
  // The untrusted network access check only applies to embedder-initiated
  // fenced frame root navigations. Note that
  // `is_embedder_initiated_fenced_frame_navigation_` being true includes fenced
  // frame and urn iframe embedder initiated navigations, so we need the
  // additional `IsFencedFrameRoot` check.
  if (frame_tree_node_->IsFencedFrameRoot() &&
      is_embedder_initiated_fenced_frame_navigation_ &&
      base::FeatureList::IsEnabled(
          blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
    const std::optional<FencedFrameProperties>&
        embedder_fenced_frame_properties = GetParentFrameOrOuterDocument()
                                               ->frame_tree_node()
                                               ->GetFencedFrameProperties();
    const std::optional<FencedFrameProperties>& target_fenced_frame_properties =
        frame_tree_node_->GetFencedFrameProperties(
            FencedFramePropertiesNodeSource::kFrameTreeRoot);

    if (target_fenced_frame_properties.has_value() &&
        target_fenced_frame_properties
            ->HasDisabledNetworkForCurrentFrameTree() &&
        embedder_fenced_frame_properties.has_value() &&
        embedder_fenced_frame_properties
            ->HasDisabledNetworkForCurrentFrameTree()) {
      // Navigation should be aborted if:
      // 1. The nested fenced frame has disabled the untrusted network access.
      // 2. The embedder fenced frame has disabled the untrusted network access
      // after the navigation starts.
      //
      // Note: The navigation is allowed if only embedder fenced frame has
      // disabled the untrusted network access. This allows the fenced frame
      // to navigate its nested fenced frame to a nested config while the parent
      // fenced frame disables its own network.
      //
      // After top-level FF disables its network, the nested FF's navigation
      // may not have committed yet. Top-level FF has no way of knowing when it
      // is safe to disable network for nested FF (and it would be a privacy
      // violation for it to know), so nested FF has to disable network for
      // itself if it wants to get shared storage access.
      //
      // For top-level FF, it does not have shared storage access until all
      // its descendants have also disabled network.
      return true;
    }
  }

  return false;
}

blink::RuntimeFeatureStateContext&
NavigationRequest::GetMutableRuntimeFeatureStateContext() {
  // runtime_feature_state_context_ shouldn't be modified after READY_TO_COMMIT
  // as its state has already been sent to the renderer.
  DCHECK_LE(state_, NavigationState::READY_TO_COMMIT);
  return runtime_feature_state_context_;
}

const blink::RuntimeFeatureStateContext&
NavigationRequest::GetRuntimeFeatureStateContext() {
  return runtime_feature_state_context_;
}

// The NavigationDownloadPolicy is currently computed by the renderer process.
// The problem: not every navigation are initiated from the renderer. Most
// fields from the bitfield can be computed from the browser process. This
// function is a partial attempt at doing it.
void NavigationRequest::ComputeDownloadPolicy() {
  // [ViewSource]
  if (GetNavigationEntry() && GetNavigationEntry()->IsViewSourceMode()) {
    download_policy().SetDisallowed(blink::NavigationDownloadType::kViewSource);
  }

  // [Sandbox]
  if ((commit_params_->frame_policy.sandbox_flags &
       network::mojom::WebSandboxFlags::kDownloads) ==
      network::mojom::WebSandboxFlags::kDownloads) {
    download_policy().SetDisallowed(blink::NavigationDownloadType::kSandbox);
  }

  // TODO(arthursonzogni): Check if the following fields from the
  // NavigationDownloadPolicy could be computed here from the browser process
  // instead:
  //
  // [NoGesture]
  // [OpenerCrossOrigin]
  // [AdFrameNoGesture]
  // [AdFrame]
  // [Interstitial]
}

bool NavigationRequest::ShouldQueueDueToExistingPendingCommitRFH() const {
  CHECK_EQ(this, frame_tree_node_->navigation_request());
  CHECK(state_ < READY_TO_COMMIT || state_ == WILL_FAIL_REQUEST);

  if (RenderFrameHostImpl* speculative_rfh =
          frame_tree_node_->render_manager()->speculative_frame_host()) {
    // Queue the navigation if there is a pending commit RenderFrameHost.
    return speculative_rfh->HasPendingCommitForCrossDocumentNavigation();
  }
  return false;
}

void NavigationRequest::RecordMetricsForBlockedGetFrameHostAttempt(
    bool commit_attempt) {
  DCHECK(!pending_commit_metrics_.start_time.is_null());
  ++pending_commit_metrics_.blocked_count;
  if (commit_attempt) {
    ++pending_commit_metrics_.blocked_commit_count;
  }
}

void NavigationRequest::PostResumeCommitTask() {
  DCHECK(ShouldAvoidRedundantNavigationCancellations());
  DCHECK(!ShouldQueueDueToExistingPendingCommitRFH());
  // TODO(crbug.com/40186427): Add some metrics for how often:
  // - this is run
  // - how long navigations remain queued
  // - how often it ends up having to simply re-queue itself
  if (resume_commit_closure_) {
    // Post a task so that we resume the navigation asynchronously. Note
    // that we're guaranteed to not have a new RFH get into a pending commit
    // stage in between the time we post this task and the time we run it.
    // `this` is the previously-queued NavigationRequest and is still owned by
    // the `FrameTreeNode`. If a new `NavigationRequest` is created, it will
    // replace and delete `this` and the resume callback for `this` will be
    // skipped.
    base::SequencedTaskRunner::GetCurrentDefault()->PostNonNestableTask(
        FROM_HERE, std::move(resume_commit_closure_));
  }
}

void NavigationRequest::CheckSoftNavigationHeuristicsInvariants() {
  if (!commit_params_->soft_navigation_heuristics_task_id) {
    return;
  }
  // TODO(crbug.com/40283341): Checking for a restore here, to
  // accommodate for restored same document navigations. They are currently
  // NOT executed as same-document. The task ID is cleared to ensure it never
  // leak toward a different document.
  if (IsRestore()) {
    DCHECK(!IsSameDocument());
    commit_params_->soft_navigation_heuristics_task_id = {};
    return;
  }

  // In NavigationControllerImpl::NavigateToExistingPendingEntry we're verifying
  // that the task ID is only passed along if the initiator RFH is the same as
  // the navigated RFH.
  DCHECK(IsSameDocument());
  DCHECK(IsInMainFrame());
  DCHECK(!frame_tree_node()->IsFencedFrameRoot());
}

StoragePartition* NavigationRequest::GetStoragePartitionWithCurrentSiteInfo() {
  // `site_info_`'s StoragePartitionConfig should refer to the correct
  // `StoragePartition` for this navigation.
  return frame_tree_node_->navigator()
      .controller()
      .GetBrowserContext()
      ->GetStoragePartition(site_info_.storage_partition_config());
}

void NavigationRequest::CreateWebUIIfNeeded(RenderFrameHostImpl* frame_host) {
  TRACE_EVENT1("content", "NavigationRequest::CreateWebUI", "url", GetURL());

  WebUI::TypeID new_web_ui_type =
      WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          frame_tree_node_->navigator().controller().GetBrowserContext(),
          GetURL());
  if (new_web_ui_type == WebUI::kNoWebUI) {
    // The navigation doesn't need a WebUI.
    return;
  }
  CHECK(!web_ui_);

  // We reuse WebUI on navigations with the same WebUI type where we use the
  // same RFH, so don't create a new one if there is already an existing WebUI
  // in `frame_host`. However, it is useful to verify that its type hasn't
  // changed. Site isolation guarantees that RenderFrameHostImpl will be changed
  // if the WebUI type differs.
  if (frame_host && frame_host->web_ui()) {
    CHECK_EQ(new_web_ui_type, frame_host->web_ui_type());
    return;
  }

  web_ui_ = std::make_unique<WebUIImpl>(this);
  std::unique_ptr<WebUIController> controller(
      WebUIControllerFactoryRegistry::GetInstance()
          ->CreateWebUIControllerForURL(web_ui_.get(), GetURL()));

  // If we have assigned (zero or more) bindings to the NavigationEntry in
  // the past, make sure we're not granting it different bindings than it
  // had before. If so, note it and don't give it any bindings, to avoid a
  // potential privilege escalation.
  if (bindings().has_value() && bindings().value() != web_ui_->GetBindings()) {
    RecordAction(base::UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
    base::WeakPtr<NavigationRequest> self = GetWeakPtr();
    // Reset `controller` first before resetting `web_ui_`, since the controller
    // still has a pointer to `web_ui_`, to avoid referencing to the already
    // deleted  `web_ui_` object from `controller`'s destructor. See also
    // https://crbug.com/345640549.
    controller.reset();
    web_ui_.reset();
    // Resetting the WebUI may indirectly call content's embedders and delete
    // `this`. There are no known occurrences of it, so we assume this never
    // happen and crash immediately if it does, because there are no easy ways
    // to recover.
    CHECK(self);
    return;
  }

  web_ui_->SetController(std::move(controller));
}

bool NavigationRequest::IsDeferred() {
  return throttle_runner_->GetDeferringThrottle() != nullptr;
}

void NavigationRequest::OnResponseBodyReady(MojoResult) {
  size_t available_bytes = 0;
  MojoResult result = response_body().ReadData(
      MOJO_READ_DATA_FLAG_QUERY, base::span<uint8_t>(), available_bytes);
  CHECK_EQ(result, MOJO_RESULT_OK);

  std::string response_body_contents(available_bytes, '\0');
  size_t actually_read_bytes = 0;
  result = response_body().ReadData(
      MOJO_READ_DATA_FLAG_PEEK,
      base::as_writable_byte_span(response_body_contents), actually_read_bytes);
  switch (result) {
    case MOJO_RESULT_OK:
      // The watcher is reset before calling the callback since the callback may
      // resume the throttle. If the watcher is still active, resumption via
      // callback results in running the OnceCallback twice.
      response_body_watcher_.reset();
      response_body_contents.resize(actually_read_bytes);
      std::move(response_body_callback_).Run(std::move(response_body_contents));
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      response_body_watcher_->ArmOrNotify();
      break;
    default:
      // The watcher is reset before calling the callback since the callback may
      // resume the throttle. If the watcher is still active, resumption via
      // callback results in running the OnceCallback twice.
      response_body_watcher_.reset();
      // The client throttle may be waiting for the response body before
      // resuming navigation, so call the callback with an empty response body
      // to unblock the throttle.
      std::move(response_body_callback_).Run(std::string());
      break;
  }
}

void NavigationRequest::RecordEarlyRenderFrameHostSwapMetrics() {
  base::UmaHistogramEnumeration("Navigation.EarlyRenderFrameHostSwapType",
                                early_render_frame_host_swap_type_);
  if (early_render_frame_host_swap_type_ !=
      NavigationRequest::EarlyRenderFrameHostSwapType::kNone) {
    base::UmaHistogramBoolean(
        "Navigation.EarlyRenderFrameHostSwap.HasCommitted", HasCommitted());
    base::UmaHistogramBoolean(
        "Navigation.EarlyRenderFrameHostSwap.IsInOutermostMainFrame",
        IsInOutermostMainFrame());
  }
}

std::pair<url::Origin, std::string>
NavigationRequest::GetOriginForURLLoaderFactoryUncheckedWithDebugInfo() {
  if (DidEncounterError()) {
    // Error pages commit in an opaque origin in the renderer process. If this
    // NavigationRequest resulted in committing an error page, return an
    // opaque origin that has precursor information consistent with the URL
    // being requested.  Note: this is intentionally done first; cases like
    // errors in srcdoc frames need not inherit the parent's origin for errors.
    return std::make_pair(
        url::Origin::Create(common_params().url).DeriveNewOpaqueOrigin(),
        "error");
  }

  // Check if this is loadDataWithBaseUrl (which needs special treatment).
  if (IsLoadDataWithBaseURL()) {
    // A (potentially attacker-controlled) renderer process should not be able
    // to use loadDataWithBaseUrl code path to initiate fetches on behalf of a
    // victim origin (fetches controlled by attacker-provided
    // |common_params.url| data: URL in a victim's origin from the
    // attacker-provided |common_params.base_url_for_data_url|).  Browser
    // process should verify that |common_params.base_url_for_data_url| is empty
    // for all renderer-initiated navigations (e.g. see
    // VerifyBeginNavigationCommonParams), but as a defense-in-depth this is
    // also asserted below.
    // History navigations are exempt from this rule because, although they can
    // be renderer-initaited via the js history API, the renderer does not
    // choose the url being navigated to. A renderer-initiated history
    // navigation may therefore navigate back to a previous browser-initiated
    // loadDataWithBaseUrl.
    CHECK(browser_initiated() ||
          NavigationTypeUtils::IsHistory(common_params().navigation_type));

    // loadDataWithBaseUrl submits a data: |common_params.url| (which has a
    // opaque origin), but commits that URL as if it came from
    // |common_params.base_url_for_data_url|.  See also
    // https://crbug.com/976253.
    return std::make_pair(
        url::Origin::Create(common_params().base_url_for_data_url),
        "load_data_with_base_url");
  }

  // Use the cached tentative data origin to commit in the data: URL case. When
  // there are multiple data: URLs in the same SiteInstanceGroup, we can rely on
  // the nonce from the origin and that of the site URL to match, which will let
  // us uniquely identify the correct data: SiteInstance.
  if (common_params().url.SchemeIs(url::kDataScheme) &&
      tentative_data_origin_to_commit_.has_value()) {
    return std::make_pair(tentative_data_origin_to_commit_.value(),
                          "data: URL");
  }

  // Srcdoc subframes need to inherit their origin from their parent frame.
  if (GetURL().IsAboutSrcdoc()) {
    RenderFrameHostImpl* parent = frame_tree_node()->parent();

    if (parent) {
      return std::make_pair(parent->GetLastCommittedOrigin(), "about_srcdoc");
    } else {
      // The only path for `parent` to be missing for a srcdoc navigation is if
      // a mainframe renderer executes `location = "about:srcdoc` instead of
      // embedding an <iframe srcdoc="..."></iframe> element; this is covered by
      // NavigationBrowserTest.BlockedSrcDoc* tests. While this will result in
      // an error page, we might still get here via GetURLInfo if the navigation
      // encounters a COOP header. In that case we return the origin of the
      // page that executed the script, knowing that the navigation will fail
      // anyways.
      return std::make_pair(
          frame_tree_node()->current_frame_host()->GetLastCommittedOrigin(),
          "about_srcdoc, no-parent");
    }
  }

  if (GetURL().SchemeIsBlob()) {
    // Blob URLs either have the origin embedded within the URL, or have a
    // URL -> origin mapping for it saved in the BlobURLRegistry.
    std::optional<int> target_rph_id;
    if (HasRenderFrameHost() && GetRenderFrameHost()->GetProcess()) {
      target_rph_id = GetRenderFrameHost()->GetProcess()->GetID();
    }
    return std::make_pair(
        static_cast<StoragePartitionImpl*>(
            GetStoragePartitionWithCurrentSiteInfo())
            ->GetBlobUrlRegistry()
            ->GetOriginForNavigation(
                GetURL(),
                common_params().initiator_origin.value_or(url::Origin()),
                target_rph_id),
        "blob");
  }

  // In cases not covered above, URLLoaderFactory should be associated with the
  // origin of |common_params.url| and/or |common_params.initiator_origin|.
  url::Origin resolved_origin = url::Origin::Resolve(
      common_params().url,
      common_params().initiator_origin.value_or(url::Origin()));

  if (common_params().url.SchemeIs(url::kDataScheme)) {
    // Cache the origin for data: URLs, so that its nonce remains stable.
    tentative_data_origin_to_commit_ = resolved_origin;
  }

  return std::make_pair(resolved_origin, "url_or_initiator");
}

url::Origin NavigationRequest::GetOriginForURLLoaderFactoryUnchecked() {
  return GetOriginForURLLoaderFactoryUncheckedWithDebugInfo().first;
}

bool NavigationRequest::HasLoader() const {
  return loader_.get() != nullptr;
}

blink::mojom::PageSwapEventParamsPtr NavigationRequest::WillDispatchPageSwap() {
  CHECK(ShouldDispatchPageSwapEvent());

  did_fire_page_swap_ = true;

  if (did_encounter_cross_origin_redirect_) {
    return nullptr;
  }

  // The `pageswap` event is fired on the old Document to provide information
  // about the new Document. The information shared must be restricted to
  // same-origin Documents.
  const bool is_same_origin =
      frame_tree_node_->current_origin().IsSameOriginWith(
          is_running_potential_prerender_activation_checks_
              ? GetTentativeOriginAtRequestTime()
              : *GetOriginToCommit());
  if (!is_same_origin) {
    return nullptr;
  }

  auto page_swap_event_params = blink::mojom::PageSwapEventParams::New();
  page_swap_event_params->url = common_params_->url;

  switch (common_params_->navigation_type) {
    case blink::mojom::NavigationType::RELOAD:
    case blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE:
      page_swap_event_params->navigation_type =
          blink::mojom::NavigationTypeForNavigationApi::kReload;
      break;

    case blink::mojom::NavigationType::RESTORE:
    case blink::mojom::NavigationType::RESTORE_WITH_POST:
      // When traversing to a restored entry, we use these navigation types.
      // Process them same as traverse navigations.
    case blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
      page_swap_event_params->navigation_type =
          blink::mojom::NavigationTypeForNavigationApi::kTraverse;
      page_swap_event_params->page_state = commit_params_->page_state;
      break;

    case blink::mojom::NavigationType::DIFFERENT_DOCUMENT:
      page_swap_event_params->navigation_type =
          common_params_->should_replace_current_entry
              ? blink::mojom::NavigationTypeForNavigationApi::kReplace
              : blink::mojom::NavigationTypeForNavigationApi::kPush;
      break;

    case blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT:
    case blink::mojom::NavigationType::SAME_DOCUMENT:
      NOTREACHED() << "Same-document navigations shouldn't fire pageswap";
  }

  return page_swap_event_params;
}

std::optional<NavigationDiscardReason>
NavigationRequest::GetNavigationDiscardReason() {
  return navigation_discard_reason_;
}

NavigationDiscardReason NavigationRequest::GetTypeForNavigationDiscardReason() {
  if (NavigationTypeUtils::IsReload(common_params_->navigation_type)) {
    return NavigationDiscardReason::kNewReloadNavigation;
  }

  if (NavigationTypeUtils::IsHistory(common_params_->navigation_type)) {
    return NavigationDiscardReason::kNewHistoryNavigation;
  }

  if (IsRendererInitiated()) {
    return NavigationDiscardReason::kNewOtherNavigationRendererInitiated;
  }
  return NavigationDiscardReason::kNewOtherNavigationBrowserInitiated;
}

void NavigationRequest::MaybeRecordTraceEventsAndHistograms() {
  if (navigation_handle_timing_.navigation_commit_sent_time.is_null()) {
    return;
  }

  bool record_uma =
      !IsSameDocument() && !IsRestore() &&
      !NavigationTypeUtils::IsHistory(common_params_->navigation_type) &&
      !NavigationTypeUtils::IsReload(common_params_->navigation_type) &&
      common_params_->url.SchemeIsHTTPOrHTTPS() &&
      !IsPrerenderedPageActivation();

  DCHECK(!blink::IsRendererDebugURL(common_params_->url));
  base::TimeTicks navigation_start_time = common_params_->navigation_start;
  DCHECK(!navigation_start_time.is_null());
  const auto trace_id = TRACE_ID_WITH_SCOPE("NavigationBreakdown",
                                            TRACE_ID_LOCAL(navigation_id_));
  const base::TimeTicks loader_start_time =
      navigation_handle_timing_.loader_start_time;
  const base::TimeTicks first_request_start_time =
      navigation_handle_timing_.first_request_start_time;
  const base::TimeTicks navigation_commit_sent_time =
      navigation_handle_timing_.navigation_commit_sent_time;

#define MAYBE_RECORD_TRACE_AND_HISTOGRAM0(name, begin_time, end_time)         \
  do {                                                                        \
    if (!begin_time.is_null() && !end_time.is_null() &&                       \
        navigation_start_time <= begin_time &&                                \
        end_time <= navigation_commit_sent_time) {                            \
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0("navigation", name,    \
                                                       trace_id, begin_time); \
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("navigation", name,      \
                                                     trace_id, end_time);     \
      if (record_uma) {                                                       \
        base::UmaHistogramTimes(                                              \
            "Navigation.MainFrame.NewNavigation.IgnoreRestore."               \
            "IsHTTPOrHTTPS." name ".Time2",                                   \
            end_time - begin_time);                                           \
      }                                                                       \
    }                                                                         \
  } while (0)

#define MAYBE_RECORD_TRACE_AND_HISTOGRAM1(name, begin_time, end_time,     \
                                          arg1_name, arg1_val)            \
  do {                                                                    \
    if (!begin_time.is_null() && !end_time.is_null() &&                   \
        navigation_start_time <= begin_time &&                            \
        end_time <= navigation_commit_sent_time) {                        \
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(                   \
          "navigation", name, trace_id, begin_time, arg1_name, arg1_val); \
      TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0("navigation", name,  \
                                                     trace_id, end_time); \
      if (record_uma) {                                                   \
        base::UmaHistogramTimes(                                          \
            "Navigation.MainFrame.NewNavigation.IgnoreRestore."           \
            "IsHTTPOrHTTPS." name ".Time2",                               \
            end_time - begin_time);                                       \
      }                                                                   \
    }                                                                     \
  } while (0)

  MAYBE_RECORD_TRACE_AND_HISTOGRAM0("NavigationStartToBeginNavigation",
                                    navigation_start_time,
                                    begin_navigation_time_);
  MAYBE_RECORD_TRACE_AND_HISTOGRAM0("BeginNavigationToLoaderStart",
                                    begin_navigation_time_, loader_start_time);
  MAYBE_RECORD_TRACE_AND_HISTOGRAM1("LoaderStartToReceiveResponse",
                                    loader_start_time, receive_response_time_,
                                    "URL", common_params_->url.spec());

  // `first_fetch_start_time_` can be earlier than
  // `loader_start_time` when Prefetch or Prerendering
  // is enabled. The following UMAs are not recorded in such cases because it
  // will skew the data. Also the following trace events are not recorded in
  // such cases because such traces will not be rendered correctly.
  if (loader_start_time <= first_fetch_start_time_) {
    MAYBE_RECORD_TRACE_AND_HISTOGRAM0(
        "LoaderStartToFetchStart", loader_start_time, first_fetch_start_time_);
    MAYBE_RECORD_TRACE_AND_HISTOGRAM0("FetchStart", first_fetch_start_time_,
                                      first_request_start_time);
    MAYBE_RECORD_TRACE_AND_HISTOGRAM0("ReceiveHeaders",
                                      first_request_start_time,
                                      final_receive_headers_end_time_);
    MAYBE_RECORD_TRACE_AND_HISTOGRAM0("ReceiveHeadersToReceiveResponse",
                                      final_receive_headers_end_time_,
                                      receive_response_time_);
  }

  MAYBE_RECORD_TRACE_AND_HISTOGRAM0("ReceiveResponseToCommitNavigation",
                                    receive_response_time_,
                                    navigation_commit_sent_time);

  // UKM data is sampled at a frequency of `kUkmSamplingRate`.
  if (record_uma && base::RandDouble() < kUkmSamplingRate &&
      !navigation_start_time.is_null() && !begin_navigation_time_.is_null() &&
      !loader_start_time.is_null() && !receive_response_time_.is_null() &&
      navigation_start_time <= begin_navigation_time_ &&
      begin_navigation_time_ <= loader_start_time &&
      loader_start_time <= receive_response_time_ &&
      receive_response_time_ <= navigation_commit_sent_time) {
    ukm::builders::NavigationRequestBreakDown(GetNextPageUkmSourceId())
        .SetNavigationStartToBeginNavigation(
            (begin_navigation_time_ - navigation_start_time).InMilliseconds())
        .SetBeginNavigationToLoaderStart(
            (loader_start_time - begin_navigation_time_).InMilliseconds())
        .SetLoaderStartToReceiveResponse(
            (receive_response_time_ - loader_start_time).InMilliseconds())
        .SetReceiveResponseToCommitNavigation(
            (navigation_commit_sent_time - receive_response_time_)
                .InMilliseconds())
        .Record(ukm::UkmRecorder::Get());
  }

#undef MAYBE_RECORD_TRACE_AND_HISTOGRAM0
#undef MAYBE_RECORD_TRACE_AND_HISTOGRAM1
}

void NavigationRequest::SanitizeDocumentIsolationPolicyHeader() {
  if (!response_head_) {
    return;
  }

  // Set DocumentIsolationPolicy to its default value if the feature is not
  // enabled.
  if (!base::FeatureList::IsEnabled(
          network::features::kDocumentIsolationPolicy)) {
    response_head_->parsed_headers->document_isolation_policy =
        network::DocumentIsolationPolicy();
    return;
  }

  // DocumentIsolationPolicy is only supported in strict SiteIsolation mode for
  // now. Set it to its default value if the platform does not support strict
  // SiteIsolation.
  if (!SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    response_head_->parsed_headers->document_isolation_policy =
        network::DocumentIsolationPolicy();
    return;
  }

  network::DocumentIsolationPolicy& dip =
      response_head_->parsed_headers->document_isolation_policy;
  bool has_dip_header =
      dip.value != network::mojom::DocumentIsolationPolicyValue::kNone ||
      dip.report_only_value !=
          network::mojom::DocumentIsolationPolicyValue::kNone ||
      dip.reporting_endpoint || dip.report_only_reporting_endpoint;

  // DocumentIsolationPolicy should only be used by secure contexts.
  if (!network::IsUrlPotentiallyTrustworthy(GetURL()) && has_dip_header) {
    response_head_->parsed_headers->document_isolation_policy =
        network::DocumentIsolationPolicy();
    AddDeferredConsoleMessage(
        blink::mojom::ConsoleMessageLevel::kError,
        "The Document-Isolation-Policy header has been ignored because "
        "the URL's origin was untrustworthy. Please deliver the response using "
        "the HTTPS protocol. You can also use the 'localhost' origin "
        "instead. See "
        "https://www.w3.org/TR/powerful-features/"
        "#potentially-trustworthy-origin.");
  }
}

}  // namespace content
