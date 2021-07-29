// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_request.h"

#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/client_hints/client_hints.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/loader/cached_navigation_url_loader.h"
#include "content/browser/loader/navigation_early_hints_manager.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/object_navigation_fallback_body_loader.h"
#include "content/browser/net/cross_origin_embedder_policy_reporter.h"
#include "content/browser/net/cross_origin_opener_policy_reporter.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/renderer_host/commit_deferring_condition.h"
#include "content/browser/renderer_host/cookie_utils.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_request_info.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/origin_policy_throttle.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/scoped_active_url.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/subresource_web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_handle_tracker.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_constants_internal.h"
#include "content/common/debug_utils.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params_mojom_traits.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/state_transitions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/filename_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/redirect_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_resource_policy.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/navigation_policy.h"
#include "third_party/blink/public/common/net/ip_address_space_util.h"
#include "third_party/blink/public/common/permissions_policy/document_policy.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom-shared.h"
#include "third_party/blink/public/mojom/loader/mixed_content.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Default timeout for the READY_TO_COMMIT -> COMMIT transition. Chosen
// initially based on the Navigation.ReadyToCommitUntilCommit UMA, and then
// refined based on feedback based on CrashExitCodes.Renderer/RESULT_CODE_HUNG.
constexpr base::TimeDelta kDefaultCommitTimeout =
    base::TimeDelta::FromSeconds(30);

// Timeout for the READY_TO_COMMIT -> COMMIT transition.
// Overrideable via SetCommitTimeoutForTesting.
base::TimeDelta g_commit_timeout = kDefaultCommitTimeout;

// crbug.com/954271: This feature is a part of an ablation study which makes
// history navigations slower.
// TODO(altimin): Clean this up after the study finishes.
constexpr base::Feature kHistoryNavigationDoNotUseCacheAblationStudy{
    "HistoryNavigationDoNotUseCacheAblationStudy",
    base::FEATURE_DISABLED_BY_DEFAULT};
constexpr base::FeatureParam<double> kDoNotUseCacheProbability{
    &kHistoryNavigationDoNotUseCacheAblationStudy, "probability", 0.0};

// Corresponds to the "NavigationURLScheme" histogram enumeration type in
// src/tools/metrics/histograms/enums.xml.
//
// DO NOT REORDER OR CHANGE THE MEANING OF THESE VALUES.
enum class NavigationURLScheme {
  UNKNOWN = 0,
  ABOUT = 1,
  BLOB = 2,
  CONTENT = 3,
  CONTENT_ID = 4,
  DATA = 5,
  FILE = 6,
  FILE_SYSTEM = 7,
  FTP = 8,
  HTTP = 9,
  HTTPS = 10,
  kMaxValue = HTTPS
};

NavigationURLScheme GetScheme(const GURL& url) {
  static const base::NoDestructor<std::map<std::string, NavigationURLScheme>>
      kSchemeMap({
          {url::kAboutScheme, NavigationURLScheme::ABOUT},
          {url::kBlobScheme, NavigationURLScheme::BLOB},
          {url::kContentScheme, NavigationURLScheme::CONTENT},
          {url::kContentIDScheme, NavigationURLScheme::CONTENT_ID},
          {url::kDataScheme, NavigationURLScheme::DATA},
          {url::kFileScheme, NavigationURLScheme::FILE},
          {url::kFileSystemScheme, NavigationURLScheme::FILE_SYSTEM},
          {url::kFtpScheme, NavigationURLScheme::FTP},
          {url::kHttpScheme, NavigationURLScheme::HTTP},
          {url::kHttpsScheme, NavigationURLScheme::HTTPS},
      });
  auto it = kSchemeMap->find(url.scheme());
  if (it != kSchemeMap->end())
    return it->second;
  return NavigationURLScheme::UNKNOWN;
}

// Returns the net load flags to use based on the navigation type.
// TODO(clamy): Remove the blink code that sets the caching flags.
void UpdateLoadFlagsWithCacheFlags(int* load_flags,
                                   mojom::NavigationType navigation_type,
                                   bool is_post) {
  switch (navigation_type) {
    case mojom::NavigationType::RELOAD:
    case mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL:
      *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case mojom::NavigationType::RELOAD_BYPASSING_CACHE:
      *load_flags |= net::LOAD_BYPASS_CACHE;
      break;
    case mojom::NavigationType::RESTORE:
      *load_flags |= net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case mojom::NavigationType::RESTORE_WITH_POST:
      *load_flags |=
          net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
      break;
    case mojom::NavigationType::SAME_DOCUMENT:
    case mojom::NavigationType::DIFFERENT_DOCUMENT:
      if (is_post)
        *load_flags |= net::LOAD_VALIDATE_CACHE;
      break;
    case mojom::NavigationType::HISTORY_SAME_DOCUMENT:
    case mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
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

// TODO(clamy): This should match what's happening in
// blink::FrameFetchContext::addAdditionalRequestHeaders.
void AddAdditionalRequestHeaders(
    net::HttpRequestHeaders* headers,
    const GURL& url,
    mojom::NavigationType navigation_type,
    ui::PageTransition transition,
    BrowserContext* browser_context,
    const std::string& method,
    const std::string& user_agent_override,
    const absl::optional<url::Origin>& initiator_origin,
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
  UpdateAdditionalHeadersForBrowserInitiatedRequest(headers, browser_context,
                                                    is_reload, render_prefs);

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec/specs/upgrade/#feature-detect
  headers->SetHeaderIfMissing("Upgrade-Insecure-Requests", "1");

  headers->SetHeaderIfMissing(
      net::HttpRequestHeaders::kUserAgent,
      user_agent_override.empty()
          ? GetContentClient()->browser()->GetUserAgent()
          : user_agent_override);

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
      absl::optional<std::string> policy_header =
          blink::DocumentPolicy::Serialize(required_policy);
      DCHECK(policy_header);
      headers->SetHeader("Sec-Required-Document-Policy", policy_header.value());
    }
  }
}

// Should match the definition of
// blink::SchemeRegistry::ShouldTreatURLSchemeAsLegacy.
bool ShouldTreatURLSchemeAsLegacy(const GURL& url) {
  return url.SchemeIs(url::kFtpScheme);
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
// as well as supplementary UMAs (depending on |transition| and |is_background|)
// for BackForward/Reload/NewNavigation variants.
//
// kMaxTime and kBuckets constants are consistent with
// UMA_HISTOGRAM_MEDIUM_TIMES, but a custom kMinTime is used for high fidelity
// near the low end of measured values.
//
// TODO(csharrison,nasko): This macro is incorrect for subframe navigations,
// which will only have subframe-specific transition types. This means that all
// subframes currently are tagged as NewNavigations.
#define LOG_NAVIGATION_TIMING_HISTOGRAM(histogram, transition, is_background, \
                                        duration)                             \
  do {                                                                        \
    const base::TimeDelta kMinTime = base::TimeDelta::FromMilliseconds(1);    \
    const base::TimeDelta kMaxTime = base::TimeDelta::FromMinutes(3);         \
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
      NOTREACHED() << "Invalid page transition: " << transition;              \
    }                                                                         \
    if (is_background.has_value()) {                                          \
      if (is_background.value()) {                                            \
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
                                absl::optional<bool> is_background,
                                bool is_same_process,
                                bool is_main_frame) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - navigation_start_time;
  LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit", transition, is_background,
                                  delta);
  if (is_main_frame) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.MainFrame", transition,
                                    is_background, delta);
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.Subframe", transition,
                                    is_background, delta);
  }
  if (is_same_process) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess", transition,
                                    is_background, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.MainFrame",
                                      transition, is_background, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.SameProcess.Subframe",
                                      transition, is_background, delta);
    }
  } else {
    LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess", transition,
                                    is_background, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.MainFrame",
                                      transition, is_background, delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("StartToCommit.CrossProcess.Subframe",
                                      transition, is_background, delta);
    }
  }
  if (!ready_to_commit_time.is_null()) {
    LOG_NAVIGATION_TIMING_HISTOGRAM("ReadyToCommitUntilCommit2", transition,
                                    is_background, now - ready_to_commit_time);
  }
}

void RecordReadyToCommitMetrics(
    RenderFrameHostImpl* old_rfh,
    RenderFrameHostImpl* new_rfh,
    const mojom::CommonNavigationParams& common_params,
    base::TimeTicks ready_to_commit_time,
    NavigationRequest::OriginAgentClusterEndResult
        origin_agent_cluster_end_result) {
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
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    ProcessLock process_lock =
        policy->GetProcessLock(new_rfh->GetProcess()->GetID());
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

  // Navigation.IsSameProcess
  {
    UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess", is_same_process);
    if (common_params.transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.BackForward",
                            is_same_process);
    } else if (ui::PageTransitionCoreTypeIs(common_params.transition,
                                            ui::PAGE_TRANSITION_RELOAD)) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.Reload", is_same_process);
    } else if (ui::PageTransitionIsNewNavigation(common_params.transition)) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.IsSameProcess.NewNavigation",
                            is_same_process);
    } else {
      NOTREACHED() << "Invalid page transition: " << common_params.transition;
    }
  }

  // TimeToReadyToCommit2
  {
    constexpr absl::optional<bool> kIsBackground = absl::nullopt;
    base::TimeDelta delta =
        ready_to_commit_time - common_params.navigation_start;

    LOG_NAVIGATION_TIMING_HISTOGRAM(
        "TimeToReadyToCommit2", common_params.transition, kIsBackground, delta);
    if (is_main_frame) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.MainFrame",
                                      common_params.transition, kIsBackground,
                                      delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.Subframe",
                                      common_params.transition, kIsBackground,
                                      delta);
    }
    if (is_same_process) {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.SameProcess",
                                      common_params.transition, kIsBackground,
                                      delta);
    } else {
      LOG_NAVIGATION_TIMING_HISTOGRAM("TimeToReadyToCommit2.CrossProcess",
                                      common_params.transition, kIsBackground,
                                      delta);
    }
  }

  // Navigation.OriginAgentCluster
  {
    UMA_HISTOGRAM_ENUMERATION("Navigation.OriginAgentCluster.Result",
                              origin_agent_cluster_end_result);
  }
}

// Convert the navigation type to the appropriate cross-document one.
//
// This is currently used when:
// 1) Restarting a same-document navigation as cross-document.
// 2) Failing a navigation and committing an error page.
mojom::NavigationType ConvertToCrossDocumentType(mojom::NavigationType type) {
  switch (type) {
    case mojom::NavigationType::SAME_DOCUMENT:
      return mojom::NavigationType::DIFFERENT_DOCUMENT;
    case mojom::NavigationType::HISTORY_SAME_DOCUMENT:
      return mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
    case mojom::NavigationType::RELOAD:
    case mojom::NavigationType::RELOAD_BYPASSING_CACHE:
    case mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL:
    case mojom::NavigationType::RESTORE:
    case mojom::NavigationType::RESTORE_WITH_POST:
    case mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT:
    case mojom::NavigationType::DIFFERENT_DOCUMENT:
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

class ScopedNavigationRequestCrashKeys {
 public:
  explicit ScopedNavigationRequestCrashKeys(
      NavigationRequest* navigation_request)
      : initiator_origin_(
            GetNavigationRequestInitiatorCrashKey(),
            base::OptionalOrNullptr(navigation_request->GetInitiatorOrigin())),
        url_(GetNavigationRequestUrlCrashKey(),
             navigation_request->GetURL().possibly_invalid_spec()) {}
  ~ScopedNavigationRequestCrashKeys() = default;

  // No copy constructor and no copy assignment operator.
  ScopedNavigationRequestCrashKeys(const ScopedNavigationRequestCrashKeys&) =
      delete;
  ScopedNavigationRequestCrashKeys& operator=(
      const ScopedNavigationRequestCrashKeys&) = delete;

 private:
  url::debug::ScopedOriginCrashKey initiator_origin_;
  base::debug::ScopedCrashKeyString url_;
};

// Start a new nested async event with the given name.
void EnterChildTraceEvent(const char* name, NavigationRequest* request) {
  // Tracing no longer outputs the end event name, so we can simply pass an
  // empty string here.
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", "", request->GetNavigationId());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("navigation", name,
                                    request->GetNavigationId());
}

// Start a new nested async event with the given name and args.
template <typename ArgType>
void EnterChildTraceEvent(const char* name,
                          NavigationRequest* request,
                          const char* arg_name,
                          ArgType arg_value) {
  // Tracing no longer outputs the end event name, so we can simply pass an
  // empty string here.
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", "", request->GetNavigationId());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "navigation", name, request->GetNavigationId(), arg_name, arg_value);
}

network::mojom::RequestDestination GetDestinationFromFrameTreeNode(
    FrameTreeNode* frame_tree_node) {
  if (frame_tree_node->IsMainFrame()) {
    return frame_tree_node->current_frame_host()
                   ->GetRenderViewHost()
                   ->GetDelegate()
                   ->IsPortal()
               ? network::mojom::RequestDestination::kIframe
               : network::mojom::RequestDestination::kDocument;
  } else {
    switch (frame_tree_node->frame_owner_element_type()) {
      case blink::mojom::FrameOwnerElementType::kObject:
        return network::mojom::RequestDestination::kObject;
      case blink::mojom::FrameOwnerElementType::kEmbed:
        return network::mojom::RequestDestination::kEmbed;
      case blink::mojom::FrameOwnerElementType::kIframe:
        return network::mojom::RequestDestination::kIframe;
      case blink::mojom::FrameOwnerElementType::kFrame:
        return network::mojom::RequestDestination::kFrame;
      case blink::mojom::FrameOwnerElementType::kPortal:
      case blink::mojom::FrameOwnerElementType::kNone:
        NOTREACHED();
        return network::mojom::RequestDestination::kDocument;
    }
    NOTREACHED();
    return network::mojom::RequestDestination::kDocument;
  }
}

url::Origin GetOriginForURLLoaderFactoryUnchecked(
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);

  // Check if this is loadDataWithBaseUrl (which needs special treatment).
  const mojom::CommonNavigationParams& common_params =
      navigation_request->common_params();
  if (NavigationRequest::IsLoadDataWithBaseURL(common_params)) {
    // A (potentially attacker-controlled) renderer process should not be able
    // to use loadDataWithBaseUrl code path to initiate fetches on behalf of a
    // victim origin (fetches controlled by attacker-provided
    // |common_params.url| data: URL in a victim's origin from the
    // attacker-provided |common_params.base_url_for_data_url|).  Browser
    // process should verify that |common_params.base_url_for_data_url| is empty
    // for all renderer-initiated navigations (e.g. see
    // VerifyBeginNavigationCommonParams), but as a defense-in-depth this is
    // also asserted below.
    CHECK(navigation_request->browser_initiated());

    // loadDataWithBaseUrl submits a data: |common_params.url| (which has a
    // opaque origin), but commits that URL as if it came from
    // |common_params.base_url_for_data_url|.  See also
    // https://crbug.com/976253.
    return url::Origin::Create(common_params.base_url_for_data_url);
  }

  // Srcdoc subframes need to inherit their origin from their parent frame.
  if (navigation_request->GetURL().IsAboutSrcdoc()) {
    RenderFrameHostImpl* parent =
        navigation_request->frame_tree_node()->parent();

    // The `parent` may be missing if a renderer executes `location =
    // "about:srcdoc` instead of embedding an <iframe srcdoc="..."></iframe>
    // element.  Such case should use an error page with an opaque, unique
    // origin.
    //
    // See also NavigationBrowserTest.BlockedSrcDoc* tests.
    DCHECK(parent || navigation_request->GetNetErrorCode() != net::OK);
    return parent ? parent->GetLastCommittedOrigin() : url::Origin();
  }

  // urn: subframes from WebBundles have opaque origins derived from the
  // Bundle's origin.
  if (common_params.url.SchemeIs(url::kUrnScheme) &&
      navigation_request->GetWebBundleURL().is_valid()) {
    return url::Origin::Resolve(
        common_params.url,
        url::Origin::Create(navigation_request->GetWebBundleURL()));
  }

  // In cases not covered above, URLLoaderFactory should be associated with the
  // origin of |common_params.url| and/or |common_params.initiator_origin|.
  return url::Origin::Resolve(
      common_params.url,
      common_params.initiator_origin.value_or(url::Origin()));
}

// Special chrome schemes cannot directly be categorized in public/private/local
// address spaces using information from the network or the PolicyContainer. We
// have to classify them manually. In its default state an unhandled scheme will
// have an IPAddressSpace of kUnknown, which is equivalent to public.
// This means a couple of things:
// - They cannot embed anything private or local without being secure contexts
//   and triggering a CORS preflight.
// - Private Network Access does not prevent them being embedded by less private
//   content.
// - It pollutes metrics since kUnknown could also mean a missed edge case.
// To address these issues we list here a number of schemes that should be
// considered local.
// TODO(titouan): It might be better to have these schemes (and in general
// other schemes such as data: or blob:) handled directly by the URLLoaders.
// Investigate on whether this is worth doing.
network::mojom::IPAddressSpace IPAddressSpaceForSpecialScheme(const GURL& url) {
  // This only handles schemes that are known to the content/ layer.
  // List here: content/public/common/url_constants.cc.
  const char* special_content_schemes[] = {
    kChromeDevToolsScheme,
    kChromeUIScheme,
    kChromeUIUntrustedScheme,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    kExternalFileScheme,
#endif
  };

  for (auto* scheme : special_content_schemes) {
    if (url.SchemeIs(scheme))
      return network::mojom::IPAddressSpace::kLocal;
  }

  // Some of these schemes are only known from the chrome layer. Query the
  // embedder for these.
  ContentBrowserClient* client = GetContentClient()->browser();
  return client->DetermineAddressSpaceFromURL(url);
}

network::mojom::IPAddressSpace CalculateIPAddressSpace(
    const GURL& url,
    network::mojom::URLResponseHead* response_head) {
  // Determine the IPAddressSpace, based on the IP address and the response
  // headers received.
  network::mojom::IPAddressSpace computed_ip_address_space =
      blink::CalculateClientAddressSpace(url, response_head);

  // Some navigation aren't loaded from the network. An IPAddressSpace can still
  // be attributed for some, whose scheme are known from the content/ layer. For
  // instance chrome: or devtools:
  if (computed_ip_address_space == network::mojom::IPAddressSpace::kUnknown) {
    computed_ip_address_space = IPAddressSpaceForSpecialScheme(url);
  }

  return computed_ip_address_space;
}

// Returns true if the parent's COEP policy should block a child embedded
// in an <iframe>.
bool CoepBlockIframe(
    network::mojom::CrossOriginEmbedderPolicyValue parent_coep,
    network::mojom::CrossOriginEmbedderPolicyValue child_coep) {
  return network::CompatibleWithCrossOriginIsolated(parent_coep) &&
         !network::CompatibleWithCrossOriginIsolated(child_coep);
}

}  // namespace

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateBrowserInitiated(
    FrameTreeNode* frame_tree_node,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool browser_initiated,
    bool was_opener_suppressed,
    const blink::LocalFrameToken* initiator_frame_token,
    int initiator_process_id,
    const std::string& extra_headers,
    FrameNavigationEntry* frame_entry,
    NavigationEntryImpl* entry,
    const scoped_refptr<network::ResourceRequestBody>& post_body,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    const absl::optional<blink::Impression>& impression) {
  TRACE_EVENT0("navigation", "NavigationRequest::CreateBrowserInitiated");
  // TODO(arthursonzogni): Form submission with the "GET" method is possible.
  // This is not currently handled here.
  bool is_form_submission = !!post_body;

  network::mojom::RequestDestination destination =
      GetDestinationFromFrameTreeNode(frame_tree_node);

  absl::optional<network::ResourceRequest::WebBundleTokenParams>
      web_bundle_token_params;
  if (frame_entry && frame_entry->subresource_web_bundle_navigation_info()) {
    auto* bundle_info = frame_entry->subresource_web_bundle_navigation_info();
    web_bundle_token_params =
        absl::make_optional(network::ResourceRequest::WebBundleTokenParams(
            bundle_info->bundle_url(), bundle_info->token(),
            bundle_info->render_process_id()));
  }

  auto navigation_params = mojom::BeginNavigationParams::New(
      base::OptionalFromPtr(initiator_frame_token), extra_headers,
      net::LOAD_NORMAL, false /* skip_service_worker */,
      blink::mojom::RequestContextType::LOCATION, destination,
      blink::mojom::MixedContentContextType::kBlockable, is_form_submission,
      false /* was_initiated_by_link_click */, GURL() /* searchable_form_url */,
      std::string() /* searchable_form_encoding */,
      GURL() /* client_side_redirect_url */,
      absl::nullopt /* devtools_initiator_info */,
      nullptr /* trust_token_params */, impression,
      base::TimeTicks() /* renderer_before_unload_start */,
      base::TimeTicks() /* renderer_before_unload_end */,
      std::move(web_bundle_token_params));

  // Shift-Reload forces bypassing caches and service workers.
  if (common_params->navigation_type ==
      mojom::NavigationType::RELOAD_BYPASSING_CACHE) {
    navigation_params->load_flags |= net::LOAD_BYPASS_CACHE;
    navigation_params->skip_service_worker = true;
  }

  RenderFrameHostImpl* rfh_restored_from_back_forward_cache = nullptr;
  if (entry) {
    BackForwardCacheImpl::Entry* restored_entry =
        frame_tree_node->navigator()
            .controller()
            .GetBackForwardCache()
            .GetEntry(entry->GetUniqueID());
    if (restored_entry) {
      rfh_restored_from_back_forward_cache =
          restored_entry->render_frame_host.get();
    }
  }

  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, std::move(common_params), std::move(navigation_params),
      std::move(commit_params), browser_initiated,
      false /* from_begin_navigation */, false /* is_for_commit */, frame_entry,
      entry, std::move(navigation_ui_data), mojo::NullAssociatedRemote(),
      rfh_restored_from_back_forward_cache, initiator_process_id,
      was_opener_suppressed));

  if (frame_entry) {
    navigation_request->blob_url_loader_factory_ =
        frame_entry->blob_url_loader_factory();

    if (navigation_request->common_params().url.SchemeIsBlob() &&
        !navigation_request->blob_url_loader_factory_) {
      // If this navigation entry came from session history then the blob
      // factory would have been cleared in
      // NavigationEntryImpl::ResetForCommit(). This is avoid keeping large
      // blobs alive unnecessarily and the spec is unclear. So create a new blob
      // factory which will work if the blob happens to still be alive,
      // resolving the blob URL in the site instance it was loaded in.
      navigation_request->blob_url_loader_factory_ =
          ChromeBlobStorageContext::URLLoaderFactoryForUrl(
              frame_tree_node->navigator()
                  .controller()
                  .GetBrowserContext()
                  ->GetStoragePartition(frame_entry->site_instance()),
              navigation_request->common_params().url);
    }
  }

  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateRendererInitiated(
    FrameTreeNode* frame_tree_node,
    NavigationEntryImpl* entry,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    int current_history_list_offset,
    int current_history_list_length,
    bool override_user_agent,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    std::unique_ptr<WebBundleHandleTracker> web_bundle_handle_tracker) {
  TRACE_EVENT0("navigation", "NavigationRequest::CreateRendererInitiated");
  // Only normal navigations to a different document or reloads are expected.
  // - Renderer-initiated same document navigations never start in the browser.
  // - Restore-navigations are always browser-initiated.
  // - History-navigations use the browser-initiated path, even the ones that
  //   are initiated by a javascript script.
  DCHECK(NavigationTypeUtils::IsReload(common_params->navigation_type) ||
         common_params->navigation_type ==
             mojom::NavigationType::DIFFERENT_DOCUMENT);

  begin_params->request_destination =
      GetDestinationFromFrameTreeNode(frame_tree_node);

  // TODO(clamy): See if the navigation start time should be measured in the
  // renderer and sent to the browser instead of being measured here.
  mojom::CommitNavigationParamsPtr commit_params =
      mojom::CommitNavigationParams::New(
          absl::nullopt, network::mojom::WebSandboxFlags(), override_user_agent,
          /*redirects=*/std::vector<GURL>(),
          /*redirect_response=*/
          std::vector<network::mojom::URLResponseHeadPtr>(),
          /*redirect_infos=*/std::vector<net::RedirectInfo>(),
          /*post_content_type=*/std::string(), common_params->url,
          common_params->method,
          /*can_load_local_resources=*/false,
          /*page_state=*/blink::PageState(),
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
          /*navigation_timing=*/mojom::NavigationTiming::New(),
          /*appcache_host_id=*/absl::nullopt,
          mojom::WasActivatedOption::kUnknown,
          /*navigation_token=*/base::UnguessableToken::Create(),
          /*prefetched_signed_exchanges=*/
          std::vector<mojom::PrefetchedSignedExchangeInfoPtr>(),
#if defined(OS_ANDROID)
          /*data_url_as_string=*/std::string(),
#endif
          /*is_browser_initiated=*/false,
          frame_tree_node->frame_tree()->is_prerendering(),
          /*web_bundle_physical_url=*/GURL(),
          /*base_url_override_for_web_bundle=*/GURL(),
          /*document_ukm_source_id=*/ukm::kInvalidSourceId,
          frame_tree_node->pending_frame_policy(),
          /*force_enabled_origin_trials=*/std::vector<std::string>(),
          /*origin_agent_cluster=*/false,
          /*enabled_client_hints=*/
          std::vector<network::mojom::WebClientHintsType>(),
          /*is_cross_browsing_instance=*/false,
          /*old_page_info=*/nullptr, /*http_response_code=*/-1,
          std::vector<
              mojom::AppHistoryEntryPtr>() /* app_history_back_entries */,
          std::vector<
              mojom::AppHistoryEntryPtr>() /* app_history_forward_entries */);

  // CreateRendererInitiated() should only be triggered when the navigation is
  // initiated by a frame in the same process.
  // TODO(https://crbug.com/1074464): Find a way to DCHECK that the routing ID
  // is from the current RFH.
  int initiator_process_id =
      frame_tree_node->current_frame_host()->GetProcess()->GetID();

  // `was_opener_suppressed` can be true for renderer initiated navigations, but
  // only in cases which get routed through `CreateBrowserInitiated()` instead.
  std::unique_ptr<NavigationRequest> navigation_request(
      new NavigationRequest(frame_tree_node, std::move(common_params),
                            std::move(begin_params), std::move(commit_params),
                            false,    // browser_initiated
                            true,     // from_begin_navigation
                            false,    // is_for_commit
                            nullptr,  // frame_entry
                            entry,
                            nullptr,  // navigation_ui_data
                            std::move(navigation_client),
                            nullptr,  // rfh_restored_from_back_forward_cache
                            initiator_process_id,
                            /*was_opener_suppressed=*/false));
  navigation_request->blob_url_loader_factory_ =
      std::move(blob_url_loader_factory);
  navigation_request->prefetched_signed_exchange_cache_ =
      std::move(prefetched_signed_exchange_cache);
  navigation_request->web_bundle_handle_tracker_ =
      std::move(web_bundle_handle_tracker);

  return navigation_request;
}

// static
std::unique_ptr<NavigationRequest> NavigationRequest::CreateForCommit(
    FrameTreeNode* frame_tree_node,
    RenderFrameHostImpl* render_frame_host,
    bool is_same_document,
    const GURL& url,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info_for_subresources,
    blink::mojom::ReferrerPtr referrer,
    const ui::PageTransition& transition,
    bool should_replace_current_entry,
    const std::string& method,
    const NavigationGesture& gesture,
    bool is_overriding_user_agent,
    const std::vector<GURL>& redirects,
    const GURL& original_url,
    std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter,
    std::unique_ptr<WebBundleNavigationInfo> web_bundle_navigation_info,
    std::unique_ptr<SubresourceWebBundleNavigationInfo>
        subresource_web_bundle_navigation_info,
    int http_response_code) {
  TRACE_EVENT0("navigation", "NavigationRequest::CreateForCommit");
  // TODO(clamy): Improve the *NavigationParams and *CommitParams to avoid
  // copying so many parameters here.
  mojom::CommonNavigationParamsPtr common_params =
      mojom::CommonNavigationParams::New(
          url,
          // TODO(nasko): Investigate better value to pass for
          // |initiator_origin|.
          origin, std::move(referrer), transition,
          is_same_document ? mojom::NavigationType::SAME_DOCUMENT
                           : mojom::NavigationType::DIFFERENT_DOCUMENT,
          blink::NavigationDownloadPolicy(), should_replace_current_entry,
          GURL() /* base_url_for_data_url*/,
          GURL() /* history_url_for_data_url */,
          blink::PreviewsTypes::PREVIEWS_UNSPECIFIED, base::TimeTicks::Now(),
          method /* method */, nullptr /* post_data */,
          network::mojom::SourceLocation::New(),
          false /* started_from_context_menu */,
          gesture == NavigationGestureUser, false /* has_text_fragment_token */,
          network::mojom::CSPDisposition::CHECK,
          std::vector<int>() /* initiator_origin_trial_features */,
          std::string() /* href_translate */,
          false /* is_history_navigation_in_new_child_frame */,
          base::TimeTicks::Now() /* input_start */);
  // Note that some params are set to default values (e.g. page_state set to
  // the default blink::PageState()) even if the DidCommit message that came
  // from the renderer contained relevant info that can be used to fill the
  // params, because setting those values don't match with the pattern used
  // by navigations that went through the browser (e.g. page_state is only
  // set in CommitNavigationParams of history navigations) or these values are
  // not used by the browser after commit.
  mojom::CommitNavigationParamsPtr commit_params =
      mojom::CommitNavigationParams::New(
          origin, network::mojom::WebSandboxFlags(), is_overriding_user_agent,
          redirects, std::vector<network::mojom::URLResponseHeadPtr>(),
          std::vector<net::RedirectInfo>(),
          std::string() /* redirect_response */, original_url,
          method /* original_method */, false /* can_load_local_resources */,
          blink::PageState(), 0 /* nav_entry_id*/,
          base::flat_map<std::string, bool>() /* subframe_unique_names */,
          false /* intended_as_new_entry */,
          -1 /* pending_history_list_offset */,
          -1 /* current_history_list_offset */,
          -1 /* current_history_list_length */, false /* was_discard */,
          false /* is_view_source */, false /* should_clear_history_list */,
          mojom::NavigationTiming::New(), absl::nullopt /* appcache_host_id */,
          mojom::WasActivatedOption::kUnknown,
          base::UnguessableToken::Create() /* navigation_token */,
          std::vector<mojom::PrefetchedSignedExchangeInfoPtr>(),
#if defined(OS_ANDROID)
          std::string() /* data_url_as_string */,
#endif
          false /* is_browser_initiated */,
          frame_tree_node->frame_tree()->is_prerendering(),
          GURL() /* web_bundle_physical_url */,
          GURL() /* base_url_override_for_web_bundle */,
          ukm::kInvalidSourceId /* document_ukm_source_id */,
          frame_tree_node->pending_frame_policy(),
          std::vector<std::string>() /* force_enabled_origin_trials */,
          false /* origin_agent_cluster */,
          std::vector<
              network::mojom::WebClientHintsType>() /* enabled_client_hints */,
          false /* is_cross_browsing_instance */, nullptr /* old_page_info */,
          http_response_code,
          std::vector<
              mojom::AppHistoryEntryPtr>() /* app_history_back_entries */,
          std::vector<
              mojom::AppHistoryEntryPtr>() /* app_history_forward_entries */);
  mojom::BeginNavigationParamsPtr begin_params =
      mojom::BeginNavigationParams::New();
  std::unique_ptr<NavigationRequest> navigation_request(new NavigationRequest(
      frame_tree_node, std::move(common_params), std::move(begin_params),
      std::move(commit_params), false /* browser_initiated */,
      false /* from_begin_navigation */, true /* is_for_commit */,
      nullptr /* frame_navigation_entry */, nullptr /* navigation_entry */,
      nullptr /* navigation_ui_data */, mojo::NullAssociatedRemote(),
      nullptr /* rfh_restored_from_back_forward_cache */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      false /* was_opener_suppressed */));

  navigation_request->web_bundle_navigation_info_ =
      std::move(web_bundle_navigation_info);
  if (subresource_web_bundle_navigation_info) {
    navigation_request->begin_params_->web_bundle_token =
        absl::make_optional(network::ResourceRequest::WebBundleTokenParams(
            subresource_web_bundle_navigation_info->bundle_url(),
            subresource_web_bundle_navigation_info->token(),
            subresource_web_bundle_navigation_info->render_process_id()));
  }
  navigation_request->render_frame_host_ = render_frame_host;
  navigation_request->coep_reporter_ = std::move(coep_reporter);
  navigation_request->isolation_info_for_subresources_ =
      isolation_info_for_subresources;
  navigation_request->StartNavigation(true);
  DCHECK(navigation_request->IsNavigationStarted());

  return navigation_request;
}

// static class variable used to generate unique navigation ids for
// NavigationRequest.
int64_t NavigationRequest::unique_id_counter_ = 0;

NavigationRequest::NavigationRequest(
    FrameTreeNode* frame_tree_node,
    mojom::CommonNavigationParamsPtr common_params,
    mojom::BeginNavigationParamsPtr begin_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool browser_initiated,
    bool from_begin_navigation,
    bool is_for_commit,
    const FrameNavigationEntry* frame_entry,
    NavigationEntryImpl* entry,
    std::unique_ptr<NavigationUIData> navigation_ui_data,
    mojo::PendingAssociatedRemote<mojom::NavigationClient> navigation_client,
    RenderFrameHostImpl* rfh_restored_from_back_forward_cache,
    int initiator_process_id,
    bool was_opener_suppressed)
    : frame_tree_node_(frame_tree_node),
      is_for_commit_(is_for_commit),
      common_params_(std::move(common_params)),
      begin_params_(std::move(begin_params)),
      commit_params_(std::move(commit_params)),
      browser_initiated_(browser_initiated),
      navigation_ui_data_(std::move(navigation_ui_data)),
      restore_type_(entry ? entry->restore_type() : RestoreType::kNotRestored),
      // Some navigations, such as renderer-initiated subframe navigations,
      // won't have a NavigationEntryImpl. Set |reload_type_| if applicable
      // for them.
      reload_type_(
          entry ? entry->reload_type()
                : NavigationTypeToReloadType(common_params_->navigation_type)),
      nav_entry_id_(entry ? entry->GetUniqueID() : 0),
      bindings_(FrameNavigationEntry::kInvalidBindings),
      from_begin_navigation_(from_begin_navigation),
      expected_render_process_host_id_(ChildProcessHost::kInvalidUniqueID),
      rfh_restored_from_back_forward_cache_(
          rfh_restored_from_back_forward_cache),
      // Store the old RenderFrameHost id at request creation to be used later.
      previous_render_frame_host_id_(GlobalFrameRoutingId(
          frame_tree_node->current_frame_host()->GetProcess()->GetID(),
          frame_tree_node->current_frame_host()->GetRoutingID())),
      initiator_frame_token_(begin_params_->initiator_frame_token),
      initiator_process_id_(initiator_process_id),
      was_opener_suppressed_(was_opener_suppressed),
      coop_status_(this),
      previous_page_ukm_source_id_(
          frame_tree_node_->current_frame_host()->GetPageUkmSourceId()) {
  DCHECK(browser_initiated_ || common_params_->initiator_origin.has_value());
  DCHECK(!blink::IsRendererDebugURL(common_params_->url));
  DCHECK(common_params_->method == "POST" || !common_params_->post_data);
  DCHECK_EQ(common_params_->url, commit_params_->original_url);
  ScopedNavigationRequestCrashKeys crash_keys(this);

  // There should be no navigations to about:newtab, about:version or other
  // similar URLs (see https://crbug.com/1145717):
  //
  // 1. For URLs coming from outside the browser (e.g. from user input into the
  //    omnibox, from other apps, etc) the //content embedder should fix
  //    the URL using the url_formatter::FixupURL API from
  //    //components/url_formatter (which would for example translate
  //    "about:version" into "chrome://version/", "localhost:1234" into
  //    "http://localhost:1234/", etc.).
  //
  // 2. Most tests should directly use correct, final URLs (e.g.
  //    chrome://version instead of about:version;  or about:blank instead of
  //    about://blank).  Similarly, links in the product (e.g. links inside
  //    chrome://about/) should use correct, final URLs.
  //
  // 3. Renderer-initiated navigations (e.g. ones initiated via
  //    <a href="...">...</a> links embedded in web pages) should typically be
  //    blocked (via RenderProcessHostImpl::FilterURL).
  if (GetURL().SchemeIs(url::kAboutScheme) && !GetURL().IsAboutBlank() &&
      !GetURL().IsAboutSrcdoc()) {
    NOTREACHED();
    base::debug::DumpWithoutCrashing();
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("navigation", "NavigationRequest",
                                    navigation_id_, "navigation_request", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("navigation", "Initializing",
                                    navigation_id_);

  if (GetInitiatorFrameToken().has_value()) {
    RenderFrameHostImpl* initiator_rfh = RenderFrameHostImpl::FromFrameToken(
        GetInitiatorProcessID(), GetInitiatorFrameToken().value());
    if (initiator_rfh) {
      initiator_commit_navigation_sent_counter_ =
          initiator_rfh->commit_navigation_sent_counter();
    }
  }

  policy_container_navigation_bundle_.emplace(
      GetParentFrame(),
      initiator_frame_token_.has_value() ? &*initiator_frame_token_ : nullptr,
      frame_entry);

  // Initialize the ClientSecurityState's COEP to that of the current document.
  // It will be updated when a network response is received. For navigations
  // that do not result in a network request, the COEP of the current document
  // is passed to the next one.
  // TODO(pmeuleman, clamy): Should we take into account the initiator COEP
  // instead?
  cross_origin_embedder_policy_ =
      frame_tree_node_->current_frame_host()->cross_origin_embedder_policy();

  NavigationControllerImpl* controller = GetNavigationController();

  if (frame_entry) {
    frame_entry_item_sequence_number_ = frame_entry->item_sequence_number();
    frame_entry_document_sequence_number_ =
        frame_entry->document_sequence_number();
    if (frame_entry->web_bundle_navigation_info()) {
      web_bundle_navigation_info_ =
          frame_entry->web_bundle_navigation_info()->Clone();
    }
  }

  // Sanitize the referrer.
  common_params_->referrer = Referrer::SanitizeForRequest(
      common_params_->url, *common_params_->referrer);

  if (frame_tree_node_->IsMainFrame()) {
    loading_mem_tracker_ =
        PeakGpuMemoryTracker::Create(PeakGpuMemoryTracker::Usage::PAGE_LOAD);
  }

  if (from_begin_navigation_) {
    // This is needed to have data URLs commit in the same SiteInstance as the
    // initiating renderer.
    source_site_instance_ =
        frame_tree_node->current_frame_host()->GetSiteInstance();

    DCHECK(navigation_client.is_valid());
    SetNavigationClient(std::move(navigation_client));
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
          common_params_->navigation_type == mojom::NavigationType::RESTORE ||
          common_params_->navigation_type ==
              mojom::NavigationType::RESTORE_WITH_POST ||
          was_opener_suppressed) {
        SetSourceSiteInstanceToInitiatorIfNeeded();
      }
    }
    isolation_info_ = entry->isolation_info();
    is_view_source_ = entry->IsViewSourceMode();

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
  // TODO(crbug.com/1099431): determine why some link navigations on chrome://
  //     pages have |browser_initiated_| set to true and others set to false.
  if (source_site_instance_) {
    bool is_renderer_initiated = !browser_initiated_;
    Referrer referrer(*common_params_->referrer);
    GetContentClient()->browser()->OverrideNavigationParams(
        controller->GetWebContents(), source_site_instance_.get(),
        &common_params_->transition, &is_renderer_initiated, &referrer,
        &common_params_->initiator_origin);
    common_params_->referrer =
        blink::mojom::Referrer::New(referrer.url, referrer.policy);
    browser_initiated_ = !is_renderer_initiated;
    commit_params_->is_browser_initiated = browser_initiated_;
  }

  // Update the load flags with cache information.
  UpdateLoadFlagsWithCacheFlags(&begin_params_->load_flags,
                                common_params_->navigation_type,
                                common_params_->method == "POST");

  // Add necessary headers that may not be present in the
  // mojom::BeginNavigationParams.
  if (entry) {
    // TODO(altimin, crbug.com/933147): Remove this logic after we are done
    // with implementing back-forward cache.
    if (frame_tree_node->IsMainFrame() && entry->back_forward_cache_metrics()) {
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
  if (!is_for_commit) {
    BrowserContext* browser_context = controller->GetBrowserContext();
    ClientHintsControllerDelegate* client_hints_delegate =
        browser_context->GetClientHintsControllerDelegate();
    if (client_hints_delegate) {
      net::HttpRequestHeaders client_hints_headers;
      AddNavigationRequestClientHintsHeaders(
          common_params_->url, &client_hints_headers, browser_context,
          client_hints_delegate, is_overriding_user_agent(), frame_tree_node_,
          commit_params_->frame_policy.container_policy);
      headers.MergeFrom(client_hints_headers);
    }

    headers.AddHeadersFromString(begin_params_->headers);
    AddAdditionalRequestHeaders(
        &headers, common_params_->url, common_params_->navigation_type,
        common_params_->transition, controller->GetBrowserContext(),
        common_params_->method, GetUserAgentOverride(),
        common_params_->initiator_origin, common_params_->referrer.get(),
        frame_tree_node);

    if (begin_params_->is_form_submission) {
      if (browser_initiated_ && !commit_params_->post_content_type.empty()) {
        // This is a form resubmit, so make sure to set the Content-Type header.
        headers.SetHeaderIfMissing(net::HttpRequestHeaders::kContentType,
                                   commit_params_->post_content_type);
      } else if (!browser_initiated_) {
        // Save the Content-Type in case the form is resubmitted. This will get
        // sent back to the renderer in the CommitNavigation IPC. The renderer
        // will then send it back with the post body so that we can access it
        // along with the body in FrameNavigationEntry::page_state_.
        headers.GetHeader(net::HttpRequestHeaders::kContentType,
                          &commit_params_->post_content_type);
      }
    }
  }

  begin_params_->headers = headers.ToString();

  navigation_entry_offset_ = EstimateHistoryOffset();

  commit_params_->is_browser_initiated = browser_initiated_;
}

NavigationRequest::~NavigationRequest() {
#if DCHECK_IS_ON()
  // If |is_safe_to_delete_| is false, it means |this| is being deleted at an
  // unexpected time, more specifically a time that is likely to lead to
  // crashing when the stack unwinds (use after free). The typical scenario for
  // this is calling to the delegate when the delegate is not expected to make
  // any sort of state change. For example, when the delegate is informed that a
  // navigation has started the delegate is not expected to call Stop().
  DCHECK(is_safe_to_delete_);
#endif

  // Close the last child event. Tracing no longer outputs the end event name,
  // so we can simply pass an empty string here.
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", "", navigation_id_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("navigation", "NavigationRequest",
                                  navigation_id_);
  if (loading_mem_tracker_)
    loading_mem_tracker_->Cancel();
  ResetExpectedProcess();
  if (state_ >= WILL_START_NAVIGATION && !HasCommitted()) {
    devtools_instrumentation::OnNavigationRequestFailed(
        *this, network::URLLoaderCompletionStatus(net::ERR_ABORTED));
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

#if defined(OS_ANDROID)
  if (navigation_handle_proxy_)
    navigation_handle_proxy_->DidFinish();
#endif

  if (IsNavigationStarted()) {
    GetDelegate()->DidFinishNavigation(this);
    ProcessOriginAgentClusterEndResult();
    if (IsInMainFrame()) {
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          "navigation", "Navigation StartToCommit",
          TRACE_ID_WITH_SCOPE("StartToCommit", TRACE_ID_LOCAL(this)), "URL",
          common_params_->url.spec(), "Net Error Code", net_error_);
    }

    // Abandon the prerender host reserved for activation if it exists.
    if (blink::features::IsPrerender2Enabled() &&
        IsPrerenderedPageActivation()) {
      GetPrerenderHostRegistry().AbandonReservedHost(
          prerender_frame_tree_node_id_);
    }

    if (IsServedFromBackForwardCache()) {
      BackForwardCacheImpl::Entry* bfcache_entry =
          GetNavigationController()->GetBackForwardCache().GetEntry(
              nav_entry_id());
      if (!bfcache_entry)
        return;

      RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(
          bfcache_entry->render_frame_host->GetGlobalFrameRoutingId());
      // RFH could have been deleted. E.g. eviction timer fired
      if (rfh && rfh->IsInBackForwardCache()) {
        // rfh is still in the cache so the navigation must have failed. But we
        // have already disabled eviction so the safest thing to do here to
        // recover is to evict.
        rfh->EvictFromBackForwardCacheWithReason(
            BackForwardCacheMetrics::NotRestoredReason::
                kNavigationCancelledWhileRestoring);
      }
    }
  }
}

void NavigationRequest::RegisterCommitDeferringConditionForTesting(
    std::unique_ptr<CommitDeferringCondition> condition) {
  commit_deferrer_->AddConditionForTesting(std::move(condition));  // IN-TEST
}

bool NavigationRequest::IsCommitDeferringConditionDeferredForTesting() {
  return commit_deferrer_->is_deferred_for_testing();  // IN-TEST
}

void NavigationRequest::BeginNavigation() {
  EnterChildTraceEvent("BeginNavigation", this);
  DCHECK(!loader_);
  DCHECK(!render_frame_host_);
  ScopedNavigationRequestCrashKeys crash_keys(this);

  SetState(WILL_START_NAVIGATION);

#if defined(OS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());
  bool should_override_url_loading = false;

  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(), browser_initiated_,
          commit_params_->original_url, commit_params_->original_method,
          common_params_->has_user_gesture, false,
          frame_tree_node_->IsMainFrame(), common_params_->transition,
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
        false /*skip_throttles*/, absl::nullopt /*error_page_content*/,
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
    StartNavigation(false);
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            absl::nullopt /* error_page_content */,
                            false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
          CredentialedSubresourceCheckResult::BLOCK_REQUEST ||
      CheckLegacyProtocolInSubresource() ==
          LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST) {
    // Create a navigation handle so that the correct error code can be set on
    // it by OnRequestFailedInternal().
    StartNavigation(false);
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /* skip_throttles  */, absl::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  StartNavigation(false);

  if (CheckAboutSrcDoc() == AboutSrcDocCheckResult::BLOCK_REQUEST) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_URL),
        true /* skip_throttles */, absl::nullopt /* error_page_content*/,
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
      NOTREACHED();
      base::debug::DumpWithoutCrashing();
    }

    ComputePoliciesToCommit();

    // Same-document navigations occur in the currently loaded document. See
    // also RenderFrameHostManager::DidCreateNavigationRequest() which will
    // expect us to use the current RenderFrameHost for this NavigationRequest,
    // and https://crbug.com/1125106.
    if (IsSameDocument()) {
      render_frame_host_ = frame_tree_node_->current_frame_host();
    } else {
      // Enforce cross-origin-opener-policy for about:blank, about:srcdoc and
      // MHTML iframe, before selecting the RenderFrameHost.
      const url::Origin origin = GetOriginForURLLoaderFactoryUnchecked(this);
      const absl::optional<network::mojom::BlockedByResponseReason>
          coop_requires_blocking = coop_status_.EnforceCOOP(
              policy_container_navigation_bundle_->FinalPolicies()
                  .cross_origin_opener_policy,
              origin, net::NetworkIsolationKey(origin, origin));
      DCHECK(!coop_requires_blocking);

      // Select an appropriate RenderFrameHost.
      std::string frame_host_choice_reason;
      render_frame_host_ =
          frame_tree_node_->render_manager()->GetFrameHostForNavigation(
              this, &frame_host_choice_reason);

      // TODO(crbug.com/1116320): Remove the ad-hoc |frame_host_choice_reason|
      // and other crash keys once the bug investigation completes.  Note that
      // the crash related to crbug/1116320 is expected to happen inside the
      // call to CommitNavigation below, a few statements down.
      SCOPED_CRASH_KEY_STRING256("nav_request", "host_choice_reason",
                                 frame_host_choice_reason);
      SCOPED_CRASH_KEY_BOOL("nav_request", "has_source_instance",
                            !!GetSourceSiteInstance());
      // Crash keys capturing values for/related to |was_opener_suppressed_|.
      SCOPED_CRASH_KEY_BOOL("nav_request", "was_opener_suppressed",
                            was_opener_suppressed_);
      SCOPED_CRASH_KEY_BOOL("nav_request", "is_main_frame", IsInMainFrame());
      SCOPED_CRASH_KEY_BOOL("nav_request", "got_initiator_routing_id",
                            GetInitiatorFrameToken() != absl::nullopt);
      SCOPED_CRASH_KEY_BOOL("nav_request", "is_renderer_initiated",
                            IsRendererInitiated());
      // Crash keys capturing values affecting whether
      // SetSourceSiteInstanceToInitiatorIfNeeded is called:
      SCOPED_CRASH_KEY_BOOL("nav_request", "from_begin_navigation",
                            from_begin_navigation_);
      SCOPED_CRASH_KEY_NUMBER(
          "nav_request", "navigation_type",
          static_cast<int>(common_params().navigation_type));
      SCOPED_CRASH_KEY_BOOL(
          "nav_request", "is_hist_nav_in_new_child",
          common_params().is_history_navigation_in_new_child_frame);
      SCOPED_CRASH_KEY_BOOL("nav_request", "has_nav_entry",
                            !!GetNavigationEntry());

      CHECK(Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
          render_frame_host_, GetUrlInfo(),
          /*is_renderer_initiated_check=*/false));
    }

    // No throttles will actually run, but `CommitNavigation()` expects to be
    // called only once the request has reached `WILL_PROCESS_RESPONSE`.
    SetState(WILL_PROCESS_RESPONSE);

    CommitNavigation();
    return;
  }
  // If the navigation is served from the back-forward cache or is activating a
  // prerendered page, we already know its preview type from the first time we
  // navigated into the page, so we should only set |previews_state| when the
  // navigation is not served from one of these.
  if (!IsPageActivation()) {
    common_params_->previews_state =
        GetContentClient()->browser()->DetermineAllowedPreviews(
            common_params_->previews_state, this, common_params_->url);
  }

  // Prerender2:
  // Find an available prerendered page for the request URL. If it's found,
  // this navigation will activate it instead of loading a page via network.
  if (blink::features::IsPrerender2Enabled()) {
    prerender_frame_tree_node_id_ =
        GetPrerenderHostRegistry().ReserveHostToActivate(common_params_->url,
                                                         *frame_tree_node_);
    // If `prerender_frame_tree_node_id_` is not
    // RenderFrameHost::kNoFrameTreeNodeId, this navigation will activate the
    // prerendered page on navigation commit.
  }

  WillStartRequest();
}

void NavigationRequest::SetWaitingForRendererResponse() {
  EnterChildTraceEvent("WaitingForRendererResponse", this);
  SetState(WAITING_FOR_RENDERER_RESPONSE);
}

void NavigationRequest::StartNavigation(bool is_for_commit) {
  DCHECK(frame_tree_node_->navigation_request() == this || is_for_commit);
  FrameTreeNode* frame_tree_node = frame_tree_node_;

  // This is needed to get site URLs and assign the expected RenderProcessHost.
  // This is not always the same as |source_site_instance_|, as it only depends
  // on the current frame host, and does not depend on |entry|.
  // The |starting_site_instance_| needs to be set here instead of the
  // constructor since a navigation can be started after the constructor and
  // before here, which can set a different RenderFrameHost and a different
  // starting SiteInstance.
  starting_site_instance_ =
      frame_tree_node->current_frame_host()->GetSiteInstance();
  site_info_ = GetSiteInfoForCommonParamsURL(
      starting_site_instance_->GetWebExposedIsolationInfo());

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
  if (!is_for_commit) {
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
    sanitized_referrer_ = blink::mojom::Referrer::New(
        redirect_chain_[0], Referrer::SanitizeForRequest(
                                common_params_->url, *common_params_->referrer)
                                ->policy);
  } else {
    sanitized_referrer_ = Referrer::SanitizeForRequest(
        common_params_->url, *common_params_->referrer);
  }

  DCHECK(!IsNavigationStarted());
  SetState(WILL_START_REQUEST);
  is_navigation_started_ = true;

  modified_request_headers_.Clear();
  removed_request_headers_.clear();

  throttle_runner_ =
      base::WrapUnique(new NavigationThrottleRunner(this, navigation_id_));

  commit_deferrer_ = CommitDeferringConditionRunner::Create(*this);

#if defined(OS_ANDROID)
  navigation_handle_proxy_ = std::make_unique<NavigationHandleProxy>(this);
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

  // The previous call to DidStartNavigation could have changed the
  // is_overriding_user_agent value in CommitNavigationParams. If we're trying
  // to restore an entry from the back-forward cache, we need to ensure that
  // the is_overriding_user_agent used in the RenderFrameHost to restore matches
  // the value set in CommitNavigationParams.
  if (IsServedFromBackForwardCache() &&
      rfh_restored_from_back_forward_cache_->is_overriding_user_agent() !=
          commit_params_->is_overriding_user_agent) {
    // Trigger an eviction, which will cancel this navigation and trigger a new
    // one to the same entry (but won't try to restore the entry from the
    // back-forward cache) asynchrnously.
    rfh_restored_from_back_forward_cache_->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::kUserAgentOverrideDiffers);
  }
}

void NavigationRequest::ResetForCrossDocumentRestart() {
  DCHECK(IsSameDocument());

  // TODO(crbug.com/1188513): A same document history navigation was performed
  // but the renderer thinks there's a different document loaded. Where did
  // this navigation come from?
  if (common_params_->navigation_type ==
      mojom::NavigationType::HISTORY_SAME_DOCUMENT) {
    CaptureTraceForNavigationDebugScenario(
        DebugScenario::kDebugSameDocNavigationDocIdMismatch);
  }

  // Reset the NavigationHandle, which is now incorrectly marked as
  // same-document. Ensure |loader_| does not exist as it can hold raw pointers
  // to objects owned by the handle (see the comment in the header).
  DCHECK(!loader_);

#if defined(OS_ANDROID)
  if (navigation_handle_proxy_)
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
  sandbox_flags_to_commit_.reset();

#if defined(OS_ANDROID)
  if (navigation_handle_proxy_)
    navigation_handle_proxy_.reset();
#endif

  // Reset the previously selected RenderFrameHost. This is expected to be null
  // at the beginning of a new navigation. See https://crbug.com/936962.
  DCHECK(render_frame_host_);
  render_frame_host_ = nullptr;

  // Convert the navigation type to the appropriate cross-document one.
  common_params_->navigation_type =
      ConvertToCrossDocumentType(common_params_->navigation_type);

  // Reset navigation handle timings.
  navigation_handle_timing_ = NavigationHandleTiming();

  policy_container_navigation_bundle_->ResetForCrossDocumentRestart();
}

void NavigationRequest::ResetStateForSiteInstanceChange() {
  // This method should only be called when there is a dest_site_instance.
  DCHECK(dest_site_instance_);

  // When a request has a destination SiteInstance (e.g., reload or session
  // history navigation) but it changes during the navigation (e.g., due to
  // redirect or error page), it's important not to remember privileges or
  // attacker-controlled state from the original entry.

  // Reset bindings (e.g., since error pages for WebUI URLs don't get them).
  bindings_ = FrameNavigationEntry::kInvalidBindings;

  // Reset any existing PageState with a non-empty, clean PageState, so that old
  // attacker-controlled state is not pulled into the new process.
  if (commit_params_->page_state.IsValid())
    commit_params_->page_state = blink::PageState::CreateFromURL(GetURL());

  // Any previously computed origin to commit is no longer valid (e.g., an
  // opaque origin for an error page).
  if (commit_params_->origin_to_commit)
    commit_params_->origin_to_commit.reset();

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
      render_frame_host_->GetNavigationClientFromInterfaceProvider();
  HandleInterfaceDisconnection(
      &commit_navigation_client_,
      base::BindOnce(&NavigationRequest::OnRendererAbortedNavigation,
                     base::Unretained(this)));
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
  return policy_container_navigation_bundle_->InitiatorPolicies();
}

const PolicyContainerPolicies& NavigationRequest::GetPolicyContainerPolicies()
    const {
  DCHECK_GE(state_, READY_TO_COMMIT);

  return policy_container_navigation_bundle_->FinalPolicies();
}

blink::mojom::PolicyContainerPtr
NavigationRequest::CreatePolicyContainerForBlink() {
  DCHECK_GE(state_, READY_TO_COMMIT);

  return policy_container_navigation_bundle_->CreatePolicyContainerForBlink();
}

scoped_refptr<PolicyContainerHost>
NavigationRequest::TakePolicyContainerHost() {
  DCHECK_GE(state_, READY_TO_COMMIT);

  // Move the host out of the data member, then reset the member. This ensures
  // we do not use the bundle after we moved its contents.
  scoped_refptr<PolicyContainerHost> host =
      std::move(*policy_container_navigation_bundle_).TakePolicyContainerHost();
  policy_container_navigation_bundle_ = absl::nullopt;

  return host;
}

void NavigationRequest::CreateCoepReporter(
    StoragePartition* storage_partition) {
  DCHECK(!isolation_info_for_subresources_.IsEmpty());

  coep_reporter_ = std::make_unique<CrossOriginEmbedderPolicyReporter>(
      storage_partition, common_params_->url,
      cross_origin_embedder_policy_.reporting_endpoint,
      cross_origin_embedder_policy_.report_only_reporting_endpoint,
      isolation_info_for_subresources_.network_isolation_key());
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
    const net::NetworkIsolationKey& network_isolation_key,
    network::mojom::URLResponseHeadPtr response_head) {
  ScopedNavigationRequestCrashKeys crash_keys(this);

  // Sanity check - this can only be set at commit time.
  DCHECK(!auth_challenge_info_);

  DCHECK(response_head);
  DCHECK(response_head->parsed_headers);
  response_head_ = std::move(response_head);
  ssl_info_ = response_head_->ssl_info;

  // Reset the page state as it can no longer be used at commit time since the
  // navigation was redirected.
  commit_params_->page_state = blink::PageState();

#if defined(OS_ANDROID)
  base::WeakPtr<NavigationRequest> this_ptr(weak_factory_.GetWeakPtr());

  bool should_override_url_loading = false;
  if (!GetContentClient()->browser()->ShouldOverrideUrlLoading(
          frame_tree_node_->frame_tree_node_id(), browser_initiated_,
          redirect_info.new_url, redirect_info.new_method,
          // Redirects are always not counted as from user gesture.
          false, true, frame_tree_node_->IsMainFrame(),
          common_params_->transition, &should_override_url_loading)) {
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
    frame_tree_node_->ResetNavigationRequest(false);
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
        false /* skip_throttles */, absl::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // For renderer-initiated navigations we need to check if the source has
  // access to the URL. Browser-initiated navigations only rely on the
  // |CanRedirectToURL| test above.
  if (!browser_initiated_ && GetSourceSiteInstance() &&
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
        false /* skip_throttles */, absl::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  const url::Origin origin = GetOriginForURLLoaderFactoryUnchecked(this);
  const absl::optional<network::mojom::BlockedByResponseReason>
      coop_requires_blocking = coop_status_.EnforceCOOP(
          coop_status_.RetrieveCOOPFromResponse(response_head_.get(), origin),
          origin, network_isolation_key);
  if (coop_requires_blocking) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(*coop_requires_blocking),
        false /* skip_throttles */, absl::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  const absl::optional<network::mojom::BlockedByResponseReason>
      coep_requires_blocking = EnforceCOEP();
  if (coep_requires_blocking) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(*coep_requires_blocking),
        false /* skip_throttles */, absl::nullopt /* error_page_content */,
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
  UpdateNavigationHandleTimingsOnResponseReceived(is_first_response);

  // Mark time for the Navigation Timing API.
  if (commit_params_->navigation_timing->redirect_start.is_null()) {
    commit_params_->navigation_timing->redirect_start =
        commit_params_->navigation_timing->fetch_start;
  }
  commit_params_->navigation_timing->redirect_end = base::TimeTicks::Now();
  commit_params_->navigation_timing->fetch_start = base::TimeTicks::Now();

  commit_params_->redirect_response.push_back(response_head_.Clone());
  commit_params_->redirect_infos.push_back(redirect_info);

  // On redirects, the initial origin_to_commit is no longer correct, so it
  // must be cleared to avoid sending incorrect value to the renderer process.
  if (commit_params_->origin_to_commit)
    commit_params_->origin_to_commit.reset();

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
        absl::nullopt /*error_page_content*/, false /*collapse_frame*/);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  if (CheckCredentialedSubresource() ==
          CredentialedSubresourceCheckResult::BLOCK_REQUEST ||
      CheckLegacyProtocolInSubresource() ==
          LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        false /*skip_throttles*/, absl::nullopt /*error_page_content*/,
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
          this);
  speculative_site_instance_ =
      site_instance->HasProcess() ? site_instance : nullptr;

  // If the new site instance doesn't yet have a process, then tell the
  // SpareRenderProcessHostManager so it can decide whether to start warming up
  // the spare at this time (note that the actual behavior depends on
  // RenderProcessHostImpl::IsSpareProcessKeptAtAllTimes).
  if (!site_instance->HasProcess()) {
    RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
        site_instance->GetBrowserContext());
  }

  // Re-evaluate the PreviewsState, but do not update the URLLoader. The
  // URLLoader PreviewsState is considered immutable after the URLLoader is
  // created.
  common_params_->previews_state =
      GetContentClient()->browser()->DetermineAllowedPreviews(
          common_params_->previews_state, this, common_params_->url);

  // Check what the process of the SiteInstance is. It will be passed to the
  // NavigationHandle, and informed to expect a navigation to the redirected
  // URL.
  // Note: calling GetProcess on the SiteInstance can lead to the creation of a
  // new process if it doesn't have one. In this case, it should only be called
  // on a SiteInstance that already has a process.
  RenderProcessHost* expected_process =
      site_instance->HasProcess() ? site_instance->GetProcess() : nullptr;

  WebExposedIsolationInfo web_exposed_isolation_info =
      frame_tree_node_->render_manager()->GetWebExposedIsolationInfo(this);
  WillRedirectRequest(common_params_->referrer->url, web_exposed_isolation_info,
                      expected_process);
}

void NavigationRequest::CheckForIsolationOptIn(const GURL& url) {
  if (!IsOptInIsolationRequested())
    return;

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  url::Origin origin = url::Origin::Create(url);
  auto* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  if (policy->UpdateOriginIsolationOptInListIfNecessary(browser_context,
                                                        origin)) {
    // This is a new request for isolating |origin|. Do a global walk of session
    // history to find any existing instances of |origin|, so that those
    // existing BrowsingInstances can avoid isolating it (which could break
    // cross-frame scripting). Only new BrowsingInstances and ones that have not
    // seen |origin| before will isolate it.
    // We don't always have a value for render_frame_host_ at this point, so we
    // map the global-walk call onto NavigatorDelegate to get it into
    // WebContents. We definitely need to do the global walk prior to deciding
    // on the render_frame_host_ to commit to.
    // We must exclude ourselves from the global walk otherwise we may mark our
    // origin as non-opt-in before it gets the change to register itself as
    // opted-in.
    frame_tree_node_->navigator()
        .GetDelegate()
        ->RegisterExistingOriginToPreventOptInIsolation(
            origin, this /* navigation_request_to_exclude */);
  }
}

void NavigationRequest::AddSameProcessOriginAgentClusterOptInIfNecessary(
    const IsolationContext& isolation_context,
    const GURL& url) {
  // If site isolation isn't disabled and OriginAgentCluster is allowed to use
  // process isolation, then no need to add the opt-in here; it will be handled
  // when the origin's SiteInstance is created.
  if (SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled() ||
      !IsOptInIsolationRequested()) {
    return;
  }

  // Since site isolation is disabled, we can't rely on the newly created
  // SiteInstance to add the origin as OAC, so we do it manually here.
  url::Origin origin = url::Origin::Create(url);
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (policy->ShouldOriginGetOptInIsolation(isolation_context, origin,
                                            true /* is_requested */)) {
    policy->AddIsolatedOriginForBrowsingInstance(
        isolation_context, origin, true /* is_origin_keyed */,
        ChildProcessSecurityPolicy::IsolatedOriginSource::WEB_TRIGGERED);
  }
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
  // TODO(https://crbug.com/888079): Use the computed origin here just to be
  // safe.
  return origin == url::Origin::Create(GetURL());
}

bool NavigationRequest::IsOptInIsolationRequested() {
  if (!response())
    return false;

  // Do not attempt isolation if the feature is not enabled.
  if (!SiteIsolationPolicy::IsOriginAgentClusterEnabled())
    return false;

  return response_head_->parsed_headers->origin_agent_cluster;
}

void NavigationRequest::DetermineOriginAgentClusterEndResult(
    bool is_requested) {
  DCHECK_EQ(state_, WILL_PROCESS_RESPONSE);

  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  // This cannot simply calculate an origin from the committing URL, as Android
  // WebView allows embedders to use loadDataWithBaseURL() to commit a data: URL
  // with an arbitrary base URL.
  const url::Origin origin =
      NavigationRequest::IsLoadDataWithBaseURL(*common_params_)
          ? url::Origin::Create(common_params_->base_url_for_data_url)
          : url::Origin::Create(common_params_->url);
  const IsolationContext& isolation_context =
      render_frame_host_->GetSiteInstance()->GetIsolationContext();
  const bool got_isolated = policy->ShouldOriginGetOptInIsolation(
      isolation_context, origin, is_requested);

  if (is_requested) {
    origin_agent_cluster_end_result_ =
        got_isolated ? OriginAgentClusterEndResult::kRequestedAndOriginKeyed
                     : OriginAgentClusterEndResult::kRequestedButNotOriginKeyed;
  } else {
    origin_agent_cluster_end_result_ =
        got_isolated
            ? OriginAgentClusterEndResult::kNotRequestedButOriginKeyed
            : OriginAgentClusterEndResult::kNotRequestedAndNotOriginKeyed;
  }

  // This needs to be computed separately from origin.opaque() because, per
  // https://crbug.com/1041376, we don't have a notion of the true origin yet.
  const bool is_opaque_origin_because_sandbox =
      (sandbox_flags_to_commit_.value() &
       network::mojom::WebSandboxFlags::kOrigin) ==
      network::mojom::WebSandboxFlags::kOrigin;

  // The origin_agent_cluster navigation commit parameter communicates to the
  // renderer about origin-keying, so it should be true for opaque origin
  // cases (e.g., for data: URLs). origin_agent_cluster_end_result_ shouldn't be
  // modified since it's used for warnings and use counters, i.e. things that
  // don't apply to this sort of "automatic" origin-keying.
  commit_params_->origin_agent_cluster =
      is_opaque_origin_because_sandbox || origin.opaque() ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kRequestedAndOriginKeyed ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kNotRequestedButOriginKeyed;
}

void NavigationRequest::ProcessOriginAgentClusterEndResult() {
  if (!HasCommitted() || IsErrorPage() || IsSameDocument())
    return;

  if (origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kRequestedAndOriginKeyed ||
      origin_agent_cluster_end_result_ ==
          OriginAgentClusterEndResult::kRequestedButNotOriginKeyed)
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        render_frame_host_,
        blink::mojom::WebFeature::kOriginAgentClusterHeader);

  const url::Origin origin = url::Origin::Create(GetURL());

  if (origin_agent_cluster_end_result_ ==
      OriginAgentClusterEndResult::kRequestedButNotOriginKeyed)
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "The page requested an origin-keyed agent cluster using the "
            "Origin-Agent-Cluster header, but could not be origin-keyed since "
            "the origin '%s' had previously been placed in a site-keyed agent "
            "cluster. Update your headers to uniformly request origin-keying "
            "for all pages on the origin.",
            origin.Serialize().c_str()));

  if (origin_agent_cluster_end_result_ ==
      OriginAgentClusterEndResult::kNotRequestedButOriginKeyed)
    render_frame_host_->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf(
            "The page did not request an origin-keyed agent cluster, but was "
            "put in one anyway because the origin '%s' had previously been "
            "placed in an origin-keyed agent cluster. Update your headers to "
            "uniformly request origin-keying for all pages on the origin.",
            origin.Serialize().c_str()));
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

  // Check the COOP header value. All same-origin values are considered to be
  // an implicit hint for site isolation.
  bool should_header_value_trigger_isolation = false;
  switch (coop_status_.current_coop().value) {
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOrigin:
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep:
    case network::mojom::CrossOriginOpenerPolicyValue::kSameOriginAllowPopups:
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
    // Note: use process_lock_url() to check isolation on the actual site
    // rather than the effective URL in the case of hosted apps.
    bool is_already_isolated_due_to_coop =
        ChildProcessSecurityPolicyImpl::GetInstance()->IsIsolatedSiteFromSource(
            url::Origin::Create(site_info_.process_lock_url()),
            ChildProcessSecurityPolicy::IsolatedOriginSource::WEB_TRIGGERED);
    return is_already_isolated_due_to_coop;
  }
  return true;
}

UrlInfo NavigationRequest::GetUrlInfo() {
  // Compute the isolation request flags.  Note that multiple requests could be
  // active simultaneously for the same navigation.
  uint32_t isolation_flags = UrlInfo::OriginIsolationRequest::kNone;

  if (IsOptInIsolationRequested())
    isolation_flags |= UrlInfo::OriginIsolationRequest::kOriginAgentCluster;

  if (ShouldRequestSiteIsolationForCOOP())
    isolation_flags |= UrlInfo::OriginIsolationRequest::kCOOP;

  auto isolation_request =
      static_cast<UrlInfo::OriginIsolationRequest>(isolation_flags);

  // TODO(crbug.com/1172042): Remove WebBundle-specific code here.
  if (GetWebBundleURL().is_valid()) {
    return UrlInfo(
        GetURL(), isolation_request,
        url::Origin::Resolve(GetURL(), url::Origin::Create(GetWebBundleURL())));
  }

  return UrlInfo(GetURL(), isolation_request);
}

const GURL& NavigationRequest::GetOriginalRequestURL() {
  // If the navigation resulted in an error or this is a loadData navigation we
  // should return the URL used to commit, even if the navigation went through
  // redirects. This is to preserve the previous behavior where we use the
  // edirect chain from the renderer to get the original request URL. When we
  // commit an error page or a loadDataWithBaseURL/loadDataAsStringWithBaseUrl
  // navigation, the redirect chain in the renderer only contains the commit
  // URL.
  if (net_error_ != net::OK ||
      NavigationRequest::IsLoadDataWithBaseURL(*common_params_)) {
    return GetURL();
  }

  // Otherwise, return the first URL in the redirect chain. If the navigation
  // is started by a client redirect, this will be the URL of the document that
  // started the redirect. Otherwise, this will be the first destination URL
  // of the navigation, before any server redirects.
  // TODO(https://crbug.com/1176636): Reconsider the behavior with client
  // redirects, as all script-initiated navigations are considered client
  // redirects, which means the client redirect might not always trigger
  // immediately (or at all, if the navigation depends on user interaction)
  // if we decide to do a reload with the original URL.
  DCHECK(!redirect_chain_.empty());
  return redirect_chain_[0];
}

GURL NavigationRequest::GetWebBundleURL() {
  if (!begin_params_->web_bundle_token)
    return GURL();
  return begin_params_->web_bundle_token->bundle_url;
}

std::unique_ptr<SubresourceWebBundleNavigationInfo>
NavigationRequest::GetSubresourceWebBundleNavigationInfo() {
  if (!begin_params_->web_bundle_token)
    return nullptr;
  return std::make_unique<SubresourceWebBundleNavigationInfo>(
      begin_params_->web_bundle_token->bundle_url,
      begin_params_->web_bundle_token->token,
      begin_params_->web_bundle_token->render_process_id);
}

void NavigationRequest::OnResponseStarted(
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    GlobalRequestID request_id,
    bool is_download,
    blink::NavigationDownloadPolicy download_policy,
    net::NetworkIsolationKey network_isolation_key,
    absl::optional<SubresourceLoaderParams> subresource_loader_params,
    EarlyHints early_hints) {
  ScopedNavigationRequestCrashKeys crash_keys(this);

  // The |loader_|'s job is finished. It must not call the NavigationRequest
  // anymore from now.
  loader_.reset();
  if (is_download)
    RecordDownloadUseCountersPrePolicyCheck(download_policy);
  is_download_ = is_download && download_policy.IsDownloadAllowed();
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
  was_early_hints_preload_link_header_received_ =
      early_hints.was_preload_link_header_received;
  early_hints_manager_ = std::move(early_hints.manager);

  if (IsServedFromBackForwardCache()) {
    response_head_ =
        rfh_restored_from_back_forward_cache_->last_response_head()->Clone();
  }

  bool is_mhtml_archive = response_head_->mime_type == "multipart/related" ||
                          response_head_->mime_type == "message/rfc822";
  if (is_mhtml_archive)
    is_mhtml_or_subframe_ = true;

  if (CheckCSPEmbeddedEnforcement() ==
      CSPEmbeddedEnforcementResult::BLOCK_RESPONSE) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_CSP),
        true /* skip_throttles */, absl::nullopt /* error_page_content*/,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  {
    const url::Origin origin = GetOriginForURLLoaderFactoryUnchecked(this);
    // TODO(pmeuleman) Move the enforcement of COOP to after
    // ComputePoliciesToCommit and use the origin from
    // GetOriginForURLLoaderFactory.
    const absl::optional<network::mojom::BlockedByResponseReason>
        coop_requires_blocking = coop_status_.EnforceCOOP(
            coop_status_.RetrieveCOOPFromResponse(response_head_.get(), origin),
            origin, network_isolation_key);
    policy_container_navigation_bundle_->SetCrossOriginOpenerPolicy(
        coop_status_.current_coop());
    if (coop_requires_blocking) {
      // TODO(https://crbug.com/1172169): Investigate what must be done in case
      // of a download.
      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(*coop_requires_blocking),
          false /* skip_throttles */, absl::nullopt /* error_page_content */,
          false /* collapse_frame */);
      // DO NOT ADD CODE after this. The previous call to
      // OnRequestFailedInternal has destroyed the NavigationRequest.
      return;
    }
  }

  ComputePoliciesToCommit();

  // The navigation may have encountered a header that requests isolation for
  // the url's origin. Before we pick the renderer, make sure we update the
  // origin-isolation opt-ins appropriately.
  CheckForIsolationOptIn(GetURL());

  // Check if the response should be sent to a renderer.
  response_should_be_rendered_ =
      !is_download &&
      (!response_head_->headers.get() ||
       (response_head_->headers->response_code() != 204 &&
        response_head_->headers->response_code() != 205 &&
        !ShouldRenderFallbackContentForResponse(*response_head_->headers)));

  // Response that will not commit should be marked as aborted in the
  // NavigationHandle.
  if (!response_should_be_rendered_)
    net_error_ = net::ERR_ABORTED;

  // Update the AppCache params of the commit params.
  commit_params_->appcache_host_id =
      appcache_handle_
          ? absl::make_optional(appcache_handle_->appcache_host_id())
          : absl::nullopt;

  const bool is_first_response = commit_params_->redirects.empty();
  UpdateNavigationHandleTimingsOnResponseReceived(is_first_response);

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
  if (commit_params_->was_activated == mojom::WasActivatedOption::kUnknown) {
    commit_params_->was_activated = mojom::WasActivatedOption::kNo;

    if (!browser_initiated_ &&
        (frame_tree_node_->HasStickyUserActivation() ||
         frame_tree_node_->has_received_user_gesture_before_nav()) &&
        ShouldPropagateUserActivation(
            frame_tree_node_->current_origin(),
            url::Origin::Create(common_params_->url))) {
      commit_params_->was_activated = mojom::WasActivatedOption::kYes;
      // TODO(805871): the next check is relying on sanitized_referrer_ but
      // should ideally use a more reliable source for the originating URL when
      // the navigation is renderer initiated.
    } else if (((common_params_->has_user_gesture && !browser_initiated_) ||
                common_params_->started_from_context_menu) &&
               ShouldPropagateUserActivation(
                   url::Origin::Create(sanitized_referrer_->url),
                   url::Origin::Create(common_params_->url))) {
      commit_params_->was_activated = mojom::WasActivatedOption::kYes;
    }
  }

  // MHTML document can't be framed into non-MHTML document (and vice versa).
  // The full page must load from the MHTML archive or none of it.
  if (is_mhtml_archive && !IsInMainFrame() && response_should_be_rendered_) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_RESPONSE),
        false /* skip_throttles */, absl::nullopt /* error_page_contnet */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  const absl::optional<network::mojom::BlockedByResponseReason>
      coep_requires_blocking = EnforceCOEP();
  if (coep_requires_blocking) {
    // TODO(https://crbug.com/1172169): Investigate what must be done in case of
    // a download.
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(*coep_requires_blocking),
        false /* skip_throttles */, absl::nullopt /* error_page_content */,
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to
    // OnRequestFailedInternal has destroyed the NavigationRequest.
    return;
  }

  auto cross_origin_embedder_policy =
      response_head_->parsed_headers->cross_origin_embedder_policy;
  const auto& url = common_params_->url;
  if (network::IsUrlPotentiallyTrustworthy(url)) {
    // https://mikewest.github.io/corpp/#process-navigation-response
    if (auto* const parent = GetParentFrame()) {
      const auto& parent_coep = parent->cross_origin_embedder_policy();
      CrossOriginEmbedderPolicyReporter* parent_coep_reporter =
          parent->coep_reporter();

      // Some special URLs not loaded using the network are inheriting the
      // Cross-Origin-Embedder-Policy header from their parent.
      // TODO(https://crbug.com/1153648) Add COEP into the PolicyContainer and
      // remove this fragile inheritance mechanism.
      const bool inherit_coep_from_parent =
          url.SchemeIsBlob() || url.SchemeIs(url::kDataScheme) ||
          GetContentClient()
              ->browser()
              ->ShouldInheritCrossOriginEmbedderPolicyImplicitly(url);
      if (inherit_coep_from_parent)
        cross_origin_embedder_policy.value = parent_coep.value;

      if (CoepBlockIframe(parent_coep.report_only_value,
                          cross_origin_embedder_policy.value)) {
        if (parent_coep_reporter) {
          parent_coep_reporter->QueueNavigationReport(redirect_chain_[0],
                                                      /*report_only=*/true);
        }
      }

      if (CoepBlockIframe(parent_coep.value,
                          cross_origin_embedder_policy.value)) {
        if (parent_coep_reporter) {
          parent_coep_reporter->QueueNavigationReport(redirect_chain_[0],
                                                      /*report_only=*/false);
        }
        // TODO(https://crbug.com/1172169): Investigate what must be done in
        // case of a download.
        OnRequestFailedInternal(network::URLLoaderCompletionStatus(
                                    network::mojom::BlockedByResponseReason::
                                        kCoepFrameResourceNeedsCoepHeader),
                                false /* skip_throttles */,
                                absl::nullopt /* error_page_content */,
                                false /* collapse_frame */);
        // DO NOT ADD CODE after this. The previous call to
        // OnRequestFailedInternal has destroyed the NavigationRequest.
        return;
      }
    }
  } else {
    cross_origin_embedder_policy = network::CrossOriginEmbedderPolicy();
  }

  // Select an appropriate renderer to commit the navigation.
  if (IsServedFromBackForwardCache()) {
    // If the current navigation is being restarted, it should not try to make
    // any further progress.
    DCHECK(!restarting_back_forward_cached_navigation_);

    NavigationControllerImpl* controller = GetNavigationController();
    render_frame_host_ = controller->GetBackForwardCache()
                             .GetEntry(nav_entry_id_)
                             ->render_frame_host.get();
    // The only time GetEntry can return nullptr here, is if the document was
    // evicted from the BackForwardCache since this navigation started.
    //
    // If the document was evicted, the navigation should have been re-issued
    // (deleting the URL loader and eventually this NavigationRequest), so we
    // should never reach this point without the document still present in the
    // BackForwardCache.
    CHECK(render_frame_host_);
  } else if (IsPrerenderedPageActivation()) {
    // Prerendering requires changing pages starting at the root node.
    DCHECK(IsInMainFrame());

    render_frame_host_ =
        GetPrerenderHostRegistry().GetRenderFrameHostForReservedHost(
            prerender_frame_tree_node_id_);
    // TODO(https://crbug.com/1181712): Handle the cases when the prerender is
    // cancelled and RFH is destroyed while NavigationRequest is alive.
  } else if (response_should_be_rendered_) {
    render_frame_host_ =
        frame_tree_node_->render_manager()->GetFrameHostForNavigation(this);

    // Update the associated SiteInstance type, which could have changed
    // due to redirects during navigation.
    set_associated_site_instance_type(
        render_frame_host_ ==
                frame_tree_node_->render_manager()->current_frame_host()
            ? AssociatedSiteInstanceType::CURRENT
            : AssociatedSiteInstanceType::SPECULATIVE);

    if (!Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
            render_frame_host_, GetUrlInfo(),
            /* is_renderer_initiated_check */ false)) {
      CHECK(false);
    }
  } else {
    render_frame_host_ = nullptr;
  }
  if (!render_frame_host_)
    DCHECK(!response_should_be_rendered_);

  if (render_frame_host_)
    DetermineOriginAgentClusterEndResult(IsOptInIsolationRequested());

  cross_origin_embedder_policy_ = cross_origin_embedder_policy;

  if (!browser_initiated_ && render_frame_host_ &&
      render_frame_host_ != frame_tree_node_->current_frame_host()) {
    // Allow the embedder to cancel the cross-process commit if needed.
    // TODO(clamy): Rename ShouldTransferNavigation.
    if (!frame_tree_node_->navigator().GetDelegate()->ShouldTransferNavigation(
            frame_tree_node_->IsMainFrame())) {
      net_error_ = net::ERR_ABORTED;
      frame_tree_node_->ResetNavigationRequest(false);
      return;
    }
  }

  // This must be set before DetermineCommittedPreviews is called.
  proxy_server_ = response_head_->proxy_server;

  // Update the previews state of the request.
  common_params_->previews_state =
      GetContentClient()->browser()->DetermineCommittedPreviews(
          common_params_->previews_state, this, response_head_->headers.get());

  // Store the URLLoaderClient endpoints until checks have been processed.
  url_loader_client_endpoints_ = std::move(url_loader_client_endpoints);

  subresource_loader_params_ = std::move(subresource_loader_params);

  // Since we've made the final pick for the RenderFrameHost above, the picked
  // RenderFrameHost's process should be considered "tainted" for future
  // process reuse decisions. That is, a site requiring a dedicated process
  // should not reuse this process, unless it's same-site with the URL we're
  // committing.  An exception is for URLs that do not "use up" the
  // SiteInstance, such as about:blank or chrome-native://.
  //
  // Note that although NavigationThrottles could still cancel the navigation
  // as part of WillProcessResponse below, we must update the process here,
  // since otherwise there could be a race if a NavigationThrottle defers the
  // navigation, and in the meantime another navigation reads the incorrect
  // IsUnused() value from the same process when making a process reuse
  // decision.
  if (render_frame_host_ &&
      SiteInstanceImpl::ShouldAssignSiteForURL(common_params_->url)) {
    render_frame_host_->GetProcess()->SetIsUsed();

    // For sites that require a dedicated process, set the site URL now if it
    // hasn't been set already. This will lock the process to that site, which
    // will prevent other sites from incorrectly reusing this process. See
    // https://crbug.com/738634.
    SiteInstanceImpl* instance = render_frame_host_->GetSiteInstance();
    const IsolationContext& isolation_context = instance->GetIsolationContext();
    auto site_info = SiteInfo::Create(isolation_context, GetUrlInfo(),
                                      instance->GetWebExposedIsolationInfo());
    if (!instance->HasSite() &&
        site_info.RequiresDedicatedProcess(isolation_context)) {
      instance->ConvertToDefaultOrSetSite(GetUrlInfo());
    }
    // Now that we know the IsolationContext for the assigned SiteInstance, we
    // opt the origin into OAC here if needed. Note that this doesn't need to
    // account for loading data URLs with a base URL, because such a base URL
    // can never opt into OAC.
    // TODO(wjmaclean): Remove this call/function when same-process
    // OriginAgentCluster moves to SiteInstanceGroup, as then all OAC origins
    // will get a SiteInstance (regardless of process isolation) and tracking
    // will be handled by the existing pathway in
    // SiteInstanceImpl::SetSiteInfoInternal().
    AddSameProcessOriginAgentClusterOptInIfNecessary(isolation_context,
                                                     GetURL());

    // TODO(wjmaclean): Once this is all working, consider combining the
    // following code into the function above.
    // If this navigation request didn't opt-in to origin isolation, we need
    // to check here in case the origin has previously requested isolation and
    // should be marked as opted-out in this SiteInstance. At this point we know
    // that |render_frame_host_|'s SiteInstance has been finalized, so it's safe
    // to use it here to get the correct |IsolationContext|.
    //
    // When loading a data URL with a base URL, use the base URL to calculate
    // the origin; otherwise, `AddNonIsolatedOriginIfNeeded()` will simply do
    // nothing as a data: URL has an opaque origin.
    //
    // TODO(wjmaclean): this won't handle cases like about:blank (where it
    // inherits an origin we care about).  We plan to compute the origin
    // before commit time (https://crbug.com/888079), which may make it
    // possible to compute the right origin here.
    const url::Origin origin =
        NavigationRequest::IsLoadDataWithBaseURL(*common_params_)
            ? url::Origin::Create(common_params_->base_url_for_data_url)
            : url::Origin::Create(common_params_->url);
    ChildProcessSecurityPolicyImpl::GetInstance()->AddNonIsolatedOriginIfNeeded(
        isolation_context, origin, false /* is_global_walk_or_frame_removal */);

    // Replace the SiteInstance of the previously committed entry if it's for a
    // url that doesn't require a site assignment, since this new commit is
    // assigning an incompatible site to the previous SiteInstance. This ensures
    // the new SiteInstance can be used with the old entry if we return to it.
    // See http://crbug.com/992198 for further context.
    NavigationEntryImpl* nav_entry =
        frame_tree_node_->navigator().controller().GetLastCommittedEntry();
    if (nav_entry && !nav_entry->GetURL().IsAboutBlank() &&
        !SiteInstanceImpl::ShouldAssignSiteForURL(nav_entry->GetURL())) {
      scoped_refptr<FrameNavigationEntry> frame_entry =
          nav_entry->root_node()->frame_entry;
      scoped_refptr<SiteInstanceImpl> new_site_instance =
          base::WrapRefCounted<SiteInstanceImpl>(static_cast<SiteInstanceImpl*>(
              instance->GetRelatedSiteInstance(frame_entry->url()).get()));
      nav_entry->AddOrUpdateFrameEntry(
          frame_tree_node_, frame_entry->item_sequence_number(),
          frame_entry->document_sequence_number(), new_site_instance.get(),
          frame_entry->source_site_instance(), frame_entry->url(),
          frame_entry->committed_origin(), frame_entry->referrer(),
          frame_entry->initiator_origin(), frame_entry->redirect_chain(),
          frame_entry->page_state(), frame_entry->method(),
          frame_entry->post_id(), frame_entry->blob_url_loader_factory(),
          frame_entry->web_bundle_navigation_info()
              ? frame_entry->web_bundle_navigation_info()->Clone()
              : nullptr,
          frame_entry->subresource_web_bundle_navigation_info()
              ? frame_entry->subresource_web_bundle_navigation_info()->Clone()
              : nullptr,
          frame_entry->policy_container_policies()
              ? frame_entry->policy_container_policies()->Clone()
              : nullptr);
    }
  }

  devtools_instrumentation::OnNavigationResponseReceived(*this,
                                                         *response_head_);

  // The response code indicates that this is an error page, but we don't
  // know how to display the content.  We follow Firefox here and show our
  // own error page instead of intercepting the request as a stream or a
  // download.
  if (is_download && (response_head_->headers.get() &&
                      (response_head_->headers->response_code() / 100 != 2))) {
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_RESPONSE),
        false /* skip_throttles */, absl::nullopt /* error_page_content */,
        false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // The CSP 'navigate-to' directive needs to know whether the response is a
  // redirect or not in order to perform its checks. This is the reason why we
  // need to check the CSP both on request and response.
  net::Error net_error = CheckContentSecurityPolicy(
      was_redirected_ /* has_followed_redirect */,
      false /* url_upgraded_after_redirect */, true /* is_response_check */);
  DCHECK_NE(net_error, net::ERR_BLOCKED_BY_CLIENT);
  // TODO(https://crbug.com/1090859): Remove this once the bug has been fixed.
  if (net_error == net::ERR_BLOCKED_BY_CLIENT)
    base::debug::DumpWithoutCrashing();
  if (net_error != net::OK) {
    OnRequestFailedInternal(network::URLLoaderCompletionStatus(net_error),
                            false /* skip_throttles */,
                            absl::nullopt /* error_page_content */,
                            false /* collapse_frame */);

    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  // Check if the navigation should be allowed to proceed.
  WillProcessResponse();
}

void NavigationRequest::OnRequestFailed(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_NE(status.error_code, net::OK);

  OnRequestFailedInternal(
      status, false /* skip_throttles */,
      absl::nullopt /* error_page_content */,
      status.should_collapse_initiator /* collapse_frame */);
}

void NavigationRequest::OnRequestFailedInternal(
    const network::URLLoaderCompletionStatus& status,
    bool skip_throttles,
    const absl::optional<std::string>& error_page_content,
    bool collapse_frame) {
  CheckStateTransition(WILL_FAIL_REQUEST);
  DCHECK(!(status.error_code == net::ERR_ABORTED &&
           error_page_content.has_value()));
  ScopedNavigationRequestCrashKeys crash_keys(this);

  // The request failed, the |loader_| must not call the NavigationRequest
  // anymore from now while the error page is being loaded.
  loader_.reset();

  common_params_->previews_state = blink::PreviewsTypes::PREVIEWS_OFF;
  ssl_info_ = status.ssl_info;

  devtools_instrumentation::OnNavigationRequestFailed(*this, status);

  // TODO(https://crbug.com/757633): Check that ssl_info.has_value() if
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

  // Abandon the prerender host if the request failed for the main frame
  // navigation. The host may be already abandoned by other prerendering
  // specific cancellation code path, i.e. PrerenderNavigationThrottle did it
  // with a dedicated FinalStatus code. In such cases, following call does
  // nothing.
  if (IsInMainFrame() && frame_tree_node_->frame_tree()->is_prerendering()) {
    auto final_status = PrerenderHost::FinalStatus::kNavigationRequestFailure;
    if (net_error_ == net::Error::ERR_BLOCKED_BY_CSP) {
      final_status = PrerenderHost::FinalStatus::kNavigationRequestBlockedByCsp;
    }
    GetPrerenderHostRegistry().AbandonHostAsync(GetFrameTreeNodeId(),
                                                final_status);
    return;
  }

  if (MaybeCancelFailedNavigation())
    return;

  if (collapse_frame) {
    DCHECK(!frame_tree_node_->IsMainFrame());
    DCHECK_EQ(net::ERR_BLOCKED_BY_CLIENT, status.error_code);
    frame_tree_node_->SetCollapsed(true);
  }

  RenderFrameHostImpl* render_frame_host = nullptr;
  switch (ComputeErrorPageProcess(status.error_code)) {
    case ErrorPageProcess::kCurrentProcess:
      // There's no way to get here with a same-document navigation, it would
      // need to be on a document that was not blocked but became blocked, but
      // same document navigations don't go to the network so it wouldn't know
      // about the change.
      CHECK(!IsSameDocument());
      render_frame_host = frame_tree_node_->current_frame_host();
      break;
    case ErrorPageProcess::kIsolatedProcess:
      // In this case we are isolating the error page from the source and
      // destination process, and want it to go to a new process.
      //
      // TODO(nasko): Investigate whether GetFrameHostForNavigation can properly
      // account for clearing the expected process if it clears the speculative
      // RenderFrameHost. See https://crbug.com/793127.
      ResetExpectedProcess();
      FALLTHROUGH;
    case ErrorPageProcess::kDestinationProcess:
      // A same-document navigation would normally attempt to navigate the
      // current document, but since we will be presenting an error instead and
      // there will not be a document to navigate. We always make an error here
      // into a cross-document navigation. See https://crbug.com/1018385 and
      // https://crbug.com/1125106.
      common_params_->navigation_type =
          ConvertToCrossDocumentType(common_params_->navigation_type);
      render_frame_host =
          frame_tree_node_->render_manager()->GetFrameHostForNavigation(this);
      break;
  }
  // Sanity check that we haven't changed the RenderFrameHost picked for the
  // error page in OnRequestFailedInternal when running the WillFailRequest
  // checks.
  CHECK(!render_frame_host_ || render_frame_host_ == render_frame_host);
  render_frame_host_ = render_frame_host;

  // Update the associated SiteInstance type.
  set_associated_site_instance_type(
      render_frame_host_ ==
              frame_tree_node_->render_manager()->current_frame_host()
          ? AssociatedSiteInstanceType::CURRENT
          : AssociatedSiteInstanceType::SPECULATIVE);

  // The check for WebUI should be performed only if error page isolation is
  // enabled for this failed navigation. It is possible for subframe error page
  // to be committed in a WebUI process as shown in https://crbug.com/944086.
  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    if (!Navigator::CheckWebUIRendererDoesNotDisplayNormalURL(
            render_frame_host_, GetUrlInfo(),
            /* is_renderer_initiated_check */ false)) {
      CHECK(false);
    }
  }

  has_stale_copy_in_cache_ = status.exists_in_cache;

  if (skip_throttles) {
    // The NavigationHandle shouldn't be notified about renderer-debug URLs.
    // They will be handled by the renderer process.
    CommitErrorPage(error_page_content);
  } else {
    // Check if the navigation should be allowed to proceed.
    WillFailRequest();
  }
}

NavigationRequest::ErrorPageProcess NavigationRequest::ComputeErrorPageProcess(
    int net_error) {
  // By policy we can isolate all error pages from both the current and
  // destination processes.
  if (SiteIsolationPolicy::IsErrorPageIsolationEnabled(
          frame_tree_node_->IsMainFrame())) {
    return ErrorPageProcess::kIsolatedProcess;
  }

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
  if (net::IsRequestBlockedError(net_error) && !browser_initiated())
    return ErrorPageProcess::kCurrentProcess;
  return ErrorPageProcess::kDestinationProcess;
}

namespace {

void OnServiceWorkerAccessedThreadSafeWrapper(
    base::WeakPtr<NavigationRequest> navigation,
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&NavigationRequest::OnServiceWorkerAccessed, navigation,
                     scope, allowed));
}

}  // namespace

void NavigationRequest::OnStartChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
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

  // Use the SiteInstance of the navigating RenderFrameHost to get access to
  // the StoragePartition. Using the url of the navigation will result in a
  // wrong StoragePartition being picked when a WebView is navigating.
  DCHECK_NE(AssociatedSiteInstanceType::NONE, associated_site_instance_type_);
  RenderFrameHostImpl* navigating_frame_host =
      associated_site_instance_type_ == AssociatedSiteInstanceType::SPECULATIVE
          ? frame_tree_node_->render_manager()->speculative_frame_host()
          : frame_tree_node_->current_frame_host();
  DCHECK(navigating_frame_host);

  SetExpectedProcess(navigating_frame_host->GetProcess());

  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  StoragePartition* partition = browser_context->GetStoragePartition(
      navigating_frame_host->GetSiteInstance());
  DCHECK(partition);

  // |loader_| should not exist if the service worker handle and app cache
  // handles will be destroyed, since it holds raw pointers to them. See the
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
        base::BindRepeating(&OnServiceWorkerAccessedThreadSafeWrapper,
                            weak_factory_.GetWeakPtr()));
  }

  if (IsSchemeSupportedForAppCache(common_params_->url)) {
    if (navigating_frame_host->GetOrCreateWebPreferences()
            .application_cache_enabled) {
      auto* appcache_service =
          static_cast<ChromeAppCacheService*>(partition->GetAppCacheService());
      if (appcache_service) {
        // The final process id won't be available until
        // NavigationRequest::ReadyToCommitNavigation.
        appcache_handle_ = std::make_unique<AppCacheNavigationHandle>(
            appcache_service, ChildProcessHost::kInvalidUniqueID);
      }
    }
  }

  // Initialize the WebBundleHandle.
  if (web_bundle_handle_tracker_) {
    DCHECK(base::FeatureList::IsEnabled(features::kWebBundles) ||
           base::FeatureList::IsEnabled(features::kWebBundlesFromNetwork) ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kTrustableWebBundleFileUrl));
    web_bundle_handle_ = web_bundle_handle_tracker_->MaybeCreateWebBundleHandle(
        common_params_->url, frame_tree_node_->frame_tree_node_id());
  }
  if (!web_bundle_handle_ && web_bundle_navigation_info_) {
    DCHECK(base::FeatureList::IsEnabled(features::kWebBundles) ||
           base::FeatureList::IsEnabled(features::kWebBundlesFromNetwork) ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kTrustableWebBundleFileUrl));
    web_bundle_handle_ = WebBundleHandle::MaybeCreateForNavigationInfo(
        web_bundle_navigation_info_->Clone(),
        frame_tree_node_->frame_tree_node_id());
  }
  if (!web_bundle_handle_) {
    if (web_bundle_utils::CanLoadAsTrustableWebBundleFile(
            common_params_->url)) {
      auto source =
          WebBundleSource::MaybeCreateFromTrustedFileUrl(common_params_->url);
      // MaybeCreateFromTrustedFileUrl() returns null when the url contains an
      // invalid character.
      if (source) {
        web_bundle_handle_ = WebBundleHandle::CreateForTrustableFile(
            std::move(source), frame_tree_node_->frame_tree_node_id());
      }
    } else if (web_bundle_utils::CanLoadAsWebBundleFile(common_params_->url)) {
      web_bundle_handle_ = WebBundleHandle::CreateForFile(
          frame_tree_node_->frame_tree_node_id());
    } else if (base::FeatureList::IsEnabled(features::kWebBundlesFromNetwork)) {
      web_bundle_handle_ = WebBundleHandle::CreateForNetwork(
          browser_context, frame_tree_node_->frame_tree_node_id());
    }
  }

  // Mark the fetch_start (Navigation Timing API).
  commit_params_->navigation_timing->fetch_start = base::TimeTicks::Now();

  std::unique_ptr<NavigationUIData> navigation_ui_data;
  if (navigation_ui_data_)
    navigation_ui_data = navigation_ui_data_->Clone();

  // Give DevTools a chance to override begin params (headers, skip SW)
  // before actually loading resource.
  bool report_raw_headers = false;
  absl::optional<std::vector<net::SourceStream::SourceType>>
      devtools_accepted_stream_types;
  devtools_instrumentation::ApplyNetworkRequestOverrides(
      frame_tree_node_, begin_params_.get(), &report_raw_headers,
      &devtools_accepted_stream_types);
  devtools_instrumentation::OnNavigationRequestWillBeSent(*this);

  // Merge headers with embedder's headers.
  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);
  headers.MergeFrom(TakeModifiedRequestHeaders());
  begin_params_->headers = headers.ToString();

  // TODO(clamy): Avoid cloning the navigation params and create the
  // ResourceRequest directly here.
  std::vector<std::unique_ptr<NavigationLoaderInterceptor>> interceptor;
  if (web_bundle_handle_)
    interceptor.push_back(web_bundle_handle_->TakeInterceptor());
  net::HttpRequestHeaders cors_exempt_headers;
  std::swap(cors_exempt_headers, cors_exempt_request_headers_);

  // For subresource requests the ClientSecurityState is passed through
  // URLLoaderFactoryParams. That does not work for navigation requests
  // because they all share a common factory, so each request is tagged with
  // a ClientSecurityState to use instead.
  //
  // We currently define the client of the fetch as the parent frame, if any.
  // This is incorrect: frames can cause others in the same browsing context
  // group to navigate to pages, without being the parent. Additionally
  // there is no client security state for top-level navigations, which mainly
  // means that Private Network Access checks are skipped for such requests.
  //
  // TODO(https://crbug.com/1170335): Pass the client security state of the
  // navigation initiator to this navigation request somehow and use that
  // instead.
  //
  // TODO(https://crbug.com/1129326): Figure out the UX story for main-frame
  // navigations, then revisit the exception made in that case.
  network::mojom::ClientSecurityStatePtr client_security_state = nullptr;
  RenderFrameHostImpl* parent = GetParentFrame();
  if (parent) {
    client_security_state = parent->BuildClientSecurityState();

    // If the right feature is not enabled, disable blocking of private network
    // requests for navigation fetches.
    if (!base::FeatureList::IsEnabled(
            features::kBlockInsecurePrivateNetworkRequestsForNavigations)) {
      // Only show warnings for requests initiated from non-secure contexts.
      client_security_state->private_network_request_policy =
          client_security_state->is_web_secure_context
              ? network::mojom::PrivateNetworkRequestPolicy::kAllow
              : network::mojom::PrivateNetworkRequestPolicy::kWarn;
    }
  }

  auto loader_type = NavigationURLLoader::LoaderType::kRegular;
  if (IsPageActivation())
    loader_type = NavigationURLLoader::LoaderType::kNoop;

  loader_ = NavigationURLLoader::Create(
      browser_context, partition,
      std::make_unique<NavigationRequestInfo>(
          common_params_->Clone(), begin_params_.Clone(), GetIsolationInfo(),
          frame_tree_node_->IsMainFrame(),
          IsSecureFrame(frame_tree_node_->parent()),
          frame_tree_node_->frame_tree_node_id(), report_raw_headers,
          upgrade_if_insecure_,
          blob_url_loader_factory_ ? blob_url_loader_factory_->Clone()
                                   : nullptr,
          devtools_navigation_token(), frame_tree_node_->devtools_frame_token(),
          OriginPolicyThrottle::ShouldRequestOriginPolicy(common_params_->url),
          std::move(cors_exempt_headers), std::move(client_security_state),
          devtools_accepted_stream_types),
      std::move(navigation_ui_data), service_worker_handle_.get(),
      appcache_handle_.get(), std::move(prefetched_signed_exchange_cache_),
      this, loader_type, CreateCookieAccessObserver(),
      static_cast<StoragePartitionImpl*>(partition)
          ->CreateURLLoaderNetworkObserverForNavigationRequest(
              frame_tree_node_->frame_tree_node_id()),
      NetworkServiceDevToolsObserver::MakeSelfOwned(frame_tree_node_),
      std::move(interceptor));

  DCHECK(!render_frame_host_);
}

void NavigationRequest::OnServiceWorkerAccessed(
    const GURL& scope,
    AllowServiceWorkerResult allowed) {
  GetDelegate()->OnServiceWorkerAccessed(this, scope, allowed);
}

network::mojom::WebSandboxFlags NavigationRequest::SandboxFlagsToCommit() {
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);
  DCHECK(!IsSameDocument());
  DCHECK(!IsServedFromBackForwardCache());
  // TODO(https://crbug.com/1181763): Figure out what to do with SandboxFlags in
  // the Prerender case
  return sandbox_flags_to_commit_.value();
}

void NavigationRequest::OnRedirectChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
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
  // Removes all Client Hints from the request, that were passed on from the
  // previous one.
  for (size_t i = 0; i < blink::kClientHintsMappingsCount; ++i)
    removed_headers.push_back(blink::kClientHintsHeaderMapping[i]);

  // Add any required Client Hints to the current request.
  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    net::HttpRequestHeaders client_hints_extra_headers;
    ParseAndPersistAcceptCHForNagivation(
        commit_params_->redirects.back(),
        commit_params_->redirect_response.back()->parsed_headers,
        browser_context, client_hints_delegate, frame_tree_node_);
    AddNavigationRequestClientHintsHeaders(
        common_params_->url, &client_hints_extra_headers, browser_context,
        client_hints_delegate, is_overriding_user_agent(), frame_tree_node_,
        commit_params_->frame_policy.container_policy);
    modified_headers.MergeFrom(client_hints_extra_headers);
  }

  net::HttpRequestHeaders cors_exempt_headers;
  std::swap(cors_exempt_headers, cors_exempt_request_headers_);
  loader_->FollowRedirect(
      std::move(removed_headers), std::move(modified_headers),
      std::move(cors_exempt_headers), common_params_->previews_state);
}

void NavigationRequest::OnFailureChecksComplete(
    NavigationThrottle::ThrottleCheckResult result) {
  // This method is called as a result of getting to the end of
  // OnRequestFailedInternal(), which calls WillFailRequest(), which
  // runs the throttles, which eventually call back to this method.
  DCHECK(result.action() != NavigationThrottle::DEFER);

  // The throttle may have changed the net_error_code, so we set the
  // `net_error_` again, overriding what OnRequestFailedInternal() set.
  net::Error old_net_error = net_error_;
  net_error_ = result.net_error_code();

  // FIXME: Should we clear out |extended_error_code_| here?

  // Ensure that WillFailRequest() isn't changing the error code in a way that
  // switches the destination process for the error page - see
  // https://crbug.com/817881.
  CHECK_EQ(ComputeErrorPageProcess(old_net_error),
           ComputeErrorPageProcess(net_error_))
      << " Unsupported error code change in WillFailRequest(): from "
      << old_net_error << " to " << net_error_;

  // The new `net_error_` value may mean we want to cancel the navigation.
  if (MaybeCancelFailedNavigation())
    return;

  // The OnRequestFailedInternal() did not commit the error page as it
  // defered to WillFailRequest(), which has called through to here, and
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
          false /*skip_throttles*/, absl::nullopt /*error_page_content*/,
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
          *this, *common_params_, *commit_params_, *response(),
          std::move(response_body_), std::move(url_loader_client_endpoints_),
          base::BindOnce(&NavigationRequest::OnRequestFailedInternal,
                         weak_factory_.GetWeakPtr(),
                         network::URLLoaderCompletionStatus(net::ERR_ABORTED),
                         false /* skip_throttles */,
                         absl::nullopt /* error_page_content */,
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
    // Reset the RenderFrameHost that had been computed for the commit of the
    // navigation.
    render_frame_host_ = nullptr;

    // TODO(clamy): distinguish between CANCEL and CANCEL_AND_IGNORE.
    if (!response_should_be_rendered_) {
      // DO NOT ADD CODE after this. The previous call to OnRequestFailed has
      // destroyed the NavigationRequest.
      OnRequestFailedInternal(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED),
          true /* skip_throttles */, absl::nullopt /* error_page_content */,
          false /* collapse_frame */);
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
    // Reset the RenderFrameHost that had been computed for the commit of the
    // navigation.
    render_frame_host_ = nullptr;
    OnRequestFailedInternal(
        network::URLLoaderCompletionStatus(result.net_error_code()),
        true /* skip_throttles */, result.error_page_content(),
        false /* collapse_frame */);
    // DO NOT ADD CODE after this. The previous call to OnRequestFailedInternal
    // has destroyed the NavigationRequest.
    return;
  }

  DCHECK_EQ(result.action(), NavigationThrottle::PROCEED);

  commit_deferrer_->RegisterDeferringConditions(*this);
  commit_deferrer_->ProcessChecks();

  // DO NOT ADD CODE after this. The previous call to ProcessChecks may have
  // caused the destruction of the NavigationRequest.
}

void NavigationRequest::OnCommitDeferringConditionChecksComplete() {
  DCHECK_LT(state_, READY_TO_COMMIT);
  CommitNavigation();

  // DO NOT ADD CODE after this. The previous call to CommitNavigation
  // caused the destruction of the NavigationRequest.
}

void NavigationRequest::CommitErrorPage(
    const absl::optional<std::string>& error_page_content) {
  DCHECK(!IsSameDocument());

  UpdateCommitNavigationParamsHistory();

  // Error pages commit in an opaque origin in the renderer process. If this
  // NavigationRequest resulted in committing an error page, set
  // |origin_to_commit| to an opaque origin that has precursor information
  // consistent with the URL being requested.
  commit_params_->origin_to_commit =
      url::Origin::Create(common_params_->url).DeriveNewOpaqueOrigin();
  if (request_navigation_client_.is_bound()) {
    if (render_frame_host_ == frame_tree_node()->current_frame_host()) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    } else {
      // This navigation is cross-RenderFrameHost: the original document should
      // no longer be able to cancel it.
      IgnoreInterfaceDisconnection();
    }
  }

  is_mhtml_or_subframe_ = false;
  sandbox_flags_to_commit_.reset();
  // TODO(https://crbug.com/1158370): Apparently, error pages inherit sandbox
  // flags from their parent/opener. Document loaded from the network
  // shouldn't have any influence over Chrome's internal error page. We should
  // define our own flags, preferably the strictest ones instead.
  ComputePoliciesToCommitForError();

  // On failed navigations, the redirect chain should only contain the last URL.
  redirect_chain_.clear();
  redirect_chain_.push_back(GetURL());

  // Set `is_prerendering` here so it's accurate before sending it to the
  // renderer, as it may be out of sync with the source of truth which is the
  // frame tree state. The frame tree may have changed if activation happened
  // while this navigation is occurring in an iframe.
  // TODO(crbug.com/1189481): With MPArch, the NavigationRequest should be
  // notified when it transfers frame trees, and commit_params should be updated
  // then.
  commit_params_->is_prerendering =
      frame_tree_node_->frame_tree()->is_prerendering();

  ReadyToCommitNavigation(true /* is_error */);

  // Use a separate cache shard, and no cookies, for error pages.
  isolation_info_for_subresources_ = net::IsolationInfo::CreateTransient();
  render_frame_host_->FailedNavigation(
      this, *common_params_, *commit_params_, has_stale_copy_in_cache_,
      net_error_, extended_error_code_, error_page_content);

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
          ->controller()
          .GetBackForwardCache()
          .CanPotentiallyStorePageLater(old_frame_host);
  commit_params_->old_page_info = mojom::OldPageInfo::New();
  commit_params_->old_page_info->routing_id_for_old_main_frame =
      old_frame_host->GetRoutingID();
  auto* page_lifecycle_state_manager =
      old_frame_host->render_view_host()->GetPageLifecycleStateManager();
  commit_params_->old_page_info->new_lifecycle_state_for_old_page =
      page_lifecycle_state_manager->SetPagehideDispatchDuringNewPageCommit(
          can_store_old_page_in_bfcache /* persisted */);
}

void NavigationRequest::CommitNavigation() {
  // A navigation request should only commit once the response has been
  // processed.
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);

  if (!CoopCoepSanityCheck())
    return;

  UpdateCommitNavigationParamsHistory();
  DCHECK(NeedsUrlLoader() == !!response_head_ ||
         (was_redirected_ && common_params_->url.IsAboutBlank()));
  DCHECK(!common_params_->url.SchemeIs(url::kJavaScriptScheme));
  DCHECK(!blink::IsRendererDebugURL(common_params_->url));
  DCHECK(sandbox_flags_to_commit_);

  AddOldPageInfoToCommitParamsIfNeeded();

  // For urn: resources served from WebBundles, use the Bundle's origin.
  url::Origin origin = (common_params_->url.SchemeIs(url::kUrnScheme) &&
                        GetWebBundleURL().is_valid())
                           ? url::Origin::Create(GetWebBundleURL())
                           : GetOriginForURLLoaderFactory();
  // TODO(crbug.com/979296): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information.
  isolation_info_for_subresources_ =
      render_frame_host_->ComputeIsolationInfoForSubresourcesForPendingCommit(
          origin);
  DCHECK(!isolation_info_for_subresources_.IsEmpty());

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

  DCHECK(render_frame_host_ ==
             frame_tree_node_->render_manager()->current_frame_host() ||
         render_frame_host_ ==
             frame_tree_node_->render_manager()->speculative_frame_host());

  if (request_navigation_client_.is_bound()) {
    if (render_frame_host_ == frame_tree_node()->current_frame_host()) {
      // Reuse the request NavigationClient for commit.
      commit_navigation_client_ = std::move(request_navigation_client_);
    } else {
      // This navigation is cross-RenderFrameHost: the original document should
      // no longer be able to cancel it.
      IgnoreInterfaceDisconnection();
    }
  }

  CreateCoepReporter(render_frame_host_->GetProcess()->GetStoragePartition());
  coop_status_.UpdateReporterStoragePartition(
      render_frame_host_->GetProcess()->GetStoragePartition());

  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    absl::optional<std::vector<network::mojom::WebClientHintsType>>
        opt_in_hints_from_response;
    if (response()) {
      opt_in_hints_from_response = ParseAndPersistAcceptCHForNagivation(
          common_params_->url, response()->parsed_headers, browser_context,
          client_hints_delegate, frame_tree_node_);
    }
    commit_params_->enabled_client_hints = LookupAcceptCHForCommit(
        common_params_->url, client_hints_delegate, frame_tree_node_);

    // We may need to add hints that were parsed this time in case they were
    // not permitted to persist in legacy accept-ch-lifetime mode.
    if (opt_in_hints_from_response) {
      for (auto hint : opt_in_hints_from_response.value())
        commit_params_->enabled_client_hints.push_back(hint);
    }
  }

  // Generate a UKM source and track it on NavigationRequest. This will be
  // passed down to the blink::Document to be created, if any, and used for UKM
  // source creation when navigation has successfully committed.
  commit_params_->document_ukm_source_id = ukm::UkmRecorder::GetNewSourceID();

  blink::mojom::ServiceWorkerContainerInfoForClientPtr
      service_worker_container_info;
  if (service_worker_handle_) {
    DCHECK(coep_reporter());
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        reporter_remote;
    coep_reporter()->Clone(reporter_remote.InitWithNewPipeAndPassReceiver());
    // Notify the service worker navigation handle that navigation commit is
    // about to go.
    service_worker_handle_->OnBeginNavigationCommit(
        render_frame_host_->GetProcess()->GetID(),
        render_frame_host_->GetRoutingID(), cross_origin_embedder_policy_,
        std::move(reporter_remote), &service_worker_container_info,
        commit_params_->document_ukm_source_id);
  }

  if (web_bundle_handle_) {
    // Check whether the page was served from a web bundle.
    if (web_bundle_handle_->navigation_info()) {
      // If the page was served from a web bundle, sets
      // |web_bundle_navigation_info_| which will be passed to
      // the FrameNavigationEntry of the navigation, and will be used for
      // history navigations.
      web_bundle_navigation_info_ =
          web_bundle_handle_->navigation_info()->Clone();
    } else {
      // If the page was not served from a web bundle, clears
      // |web_bundle_handle_| not to pass it to |render_frame_host_|.
      web_bundle_handle_.reset();
    }
  }

  // Set `is_prerendering` here so it's accurate before sending it to the
  // renderer, as it may be out of sync with the source of truth which is the
  // frame tree state. The frame tree may have changed if activation happened
  // while this navigation is occurring in an iframe.
  // TODO(crbug.com/1189481): With MPArch, the NavigationRequest should be
  // notified when it transfers frame trees, and commit_params should be updated
  // then.
  commit_params_->is_prerendering =
      frame_tree_node_->frame_tree()->is_prerendering();

  if (!IsSameDocument())
    GetNavigationController()->PopulateAppHistoryEntryVectors(this);

  auto common_params = common_params_->Clone();
  auto commit_params = commit_params_.Clone();
  auto response_head = response_head_.Clone();
  if (subresource_loader_params_ &&
      !subresource_loader_params_->prefetched_signed_exchanges.empty()) {
    commit_params->prefetched_signed_exchanges =
        std::move(subresource_loader_params_->prefetched_signed_exchanges);
  }

  render_frame_host_->CommitNavigation(
      this, std::move(common_params), std::move(commit_params),
      std::move(response_head), std::move(response_body_),
      std::move(url_loader_client_endpoints_), is_view_source_,
      std::move(subresource_loader_params_), std::move(subresource_overrides_),
      std::move(service_worker_container_info), devtools_navigation_token_,
      std::move(web_bundle_handle_));
  UpdateNavigationHandleTimingsOnCommitSent();

  // Give SpareRenderProcessHostManager a heads-up about the most recently used
  // BrowserContext.  This is mostly needed to make sure the spare is warmed-up
  // if it wasn't done in RenderProcessHostImpl::GetProcessHostForSiteInstance.
  RenderProcessHostImpl::NotifySpareManagerAboutRecentlyUsedBrowserContext(
      render_frame_host_->GetSiteInstance()->GetBrowserContext());

  SendDeferredConsoleMessages();
}

void NavigationRequest::CommitPageActivation() {
  // An activation is either for the back-forward cache or prerendering. They
  // are mutually exclusive.
  DCHECK_NE(IsServedFromBackForwardCache(), IsPrerenderedPageActivation());

  NavigationControllerImpl* controller = GetNavigationController();
  std::unique_ptr<BackForwardCacheImpl::Entry> activated_entry;

  if (IsServedFromBackForwardCache()) {
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
    // The only time activated_entry can be nullptr here, is if the
    // document was evicted from the BackForwardCache since this navigation
    // started.
    //
    // If the document was evicted, it should have posted a task to re-issue
    // the navigation - ensure that this happened.
    DCHECK(activated_entry || restarting_back_forward_cached_navigation_);
    if (!activated_entry)
      return;
  } else {
    activated_entry = GetPrerenderHostRegistry().ActivateReservedHost(
        prerender_frame_tree_node_id_, *this);

    // TODO(https://crbug.com/1181712): Determine the best way to handle
    // navigation when prerendering is cancelled during activation. This
    // includes the case where a navigation can be restarted.
    if (!activated_entry) {
      // TODO(https://crbug.com/1126305): Record the final status for activation
      // failure.
      NOTIMPLEMENTED()
          << "The prerendered page was cancelled during activation";
      return;
    }

    // The prerender page might have navigated.
    // TODO(https://crbug.com/1181712): Ensure that the tests that navigate
    // MPArch activation flow do not crash. This is a hack to unblock the basic
    // MPArch activation flow for now. There are probably other parameters which
    // are out of sync, and we need to carefully think through how we can
    // activate a RenderFrameHost whose URL doesn't match the one that was
    // initially passed to NavigationRequest (or disallow subsequent navigations
    // in the main frame of the prerender frame tree).
    common_params_->url =
        activated_entry->render_frame_host->GetLastCommittedURL();
  }

  base::WeakPtr<NavigationRequest> weak_self(weak_factory_.GetWeakPtr());
  ReadyToCommitNavigation(false /* is_error */);
  // The call above might block on showing a user dialog. The interaction of
  // the user with this dialog might result in the WebContents owning this
  // NavigationRequest to be destroyed. Return if this is the case.
  if (!weak_self)
    return;

  // Move the BackForwardCacheImpl::Entry into RenderFrameHostManager, in
  // preparation for committing. This entry may be either restored from the
  // backforward cache or a prerender activation.
  if (IsServedFromBackForwardCache()) {
    frame_tree_node_->render_manager()->RestoreFromBackForwardCache(
        std::move(activated_entry));
  } else {
    frame_tree_node_->render_manager()->ActivatePrerender(
        std::move(activated_entry));
  }

  // Commit the page activation. This includes committing the RenderFrameHost
  // and restoring extra state, such as proxies, etc.
  // Note that this will delete the NavigationRequest.
  GetRenderFrameHost()->DidCommitPageActivation(
      this, IsPrerenderedPageActivation()
                ? MakeDidCommitProvisionalLoadParamsForPrerenderActivation()
                : MakeDidCommitProvisionalLoadParamsForBFCacheRestore());
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

void NavigationRequest::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(host->GetID(), expected_render_process_host_id_);
  ResetExpectedProcess();
}

void NavigationRequest::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {}

void NavigationRequest::UpdateNavigationHandleTimingsOnResponseReceived(
    bool is_first_response) {
  base::TimeTicks loader_callback_time = base::TimeTicks::Now();

  if (is_first_response) {
    DCHECK(navigation_handle_timing_.first_request_start_time.is_null());
    DCHECK(navigation_handle_timing_.first_response_start_time.is_null());
    DCHECK(navigation_handle_timing_.first_loader_callback_time.is_null());
    navigation_handle_timing_.first_request_start_time =
        response_head_->load_timing.send_start;
    navigation_handle_timing_.first_response_start_time =
        response_head_->load_timing.receive_headers_start;
    navigation_handle_timing_.first_loader_callback_time = loader_callback_time;
  }

  navigation_handle_timing_.final_request_start_time =
      response_head_->load_timing.send_start;
  navigation_handle_timing_.final_response_start_time =
      response_head_->load_timing.receive_headers_start;
  navigation_handle_timing_.final_non_informational_response_start_time =
      response_head_->load_timing.receive_non_informational_headers_start;
  navigation_handle_timing_.final_loader_callback_time = loader_callback_time;

  // 103 Early Hints experiment (https://crbug.com/1093693).
  if (is_first_response) {
    DCHECK(
        navigation_handle_timing_.early_hints_for_first_request_time.is_null());
    navigation_handle_timing_.early_hints_for_first_request_time =
        response_head_->load_timing.first_early_hints_time;
  }
  navigation_handle_timing_.early_hints_for_final_request_time =
      response_head_->load_timing.first_early_hints_time;

  // |navigation_commit_sent_time| will be updated by
  // UpdateNavigationHandleTimingsOnCommitSent() later.
  DCHECK(navigation_handle_timing_.navigation_commit_sent_time.is_null());
}

void NavigationRequest::UpdateNavigationHandleTimingsOnCommitSent() {
  DCHECK(navigation_handle_timing_.navigation_commit_sent_time.is_null());
  navigation_handle_timing_.navigation_commit_sent_time =
      base::TimeTicks::Now();
}

void NavigationRequest::UpdateSiteInfo(
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    RenderProcessHost* post_redirect_process) {
  int post_redirect_process_id = post_redirect_process
                                     ? post_redirect_process->GetID()
                                     : ChildProcessHost::kInvalidUniqueID;

  SiteInfo new_site_info =
      GetSiteInfoForCommonParamsURL(web_exposed_isolation_info);
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
    bool is_response_check,
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
  } else if (common_params_->url.SchemeIs(url::kUrnScheme) &&
             begin_params_->web_bundle_token.has_value()) {
    // When navigating to a urn:uuid resource in a web bundle, we check the
    // bundle URL instead of the urn:uuid URL.
    url = begin_params_->web_bundle_token->bundle_url;
  } else {
    url = common_params_->url;
  }
  return context->IsAllowedByCsp(
      policies, directive, url, commit_params_->original_url,
      has_followed_redirect, is_response_check, common_params_->source_location,
      disposition, begin_params_->is_form_submission);
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
            url_upgraded_after_redirect, is_response_check, disposition)) {
      // net::ERR_ABORTED is used instead of net::ERR_BLOCKED_BY_CSP. This is
      // a better user experience as the user is not presented with an error
      // page. However if other CSP directives like frame-src are violated, it
      // may be appropriate for them to use ERR_BLOCKED_BY_CSP so this can be
      // overridden by the checks below.
      error = net::ERR_ABORTED;
    }

    if (base::FeatureList::IsEnabled(
            features::kExperimentalContentSecurityPolicyFeatures)) {
      // [navigate-to]
      if (!IsAllowedByCSPDirective(
              initiator_policies->content_security_policies, &initiator_context,
              network::mojom::CSPDirectiveName::NavigateTo,
              has_followed_redirect, url_upgraded_after_redirect,
              is_response_check, disposition)) {
        // net::ERR_ABORTED is used instead of net::ERR_BLOCKED_BY_CSP. This is
        // a better user experience as the user is not presented with an error
        // page. However if other CSP directives life frame-src are violated, it
        // may be appropriate for them to use ERR_BLOCKED_BY_CSP so this can be
        // overridden by the checks below.
        error = net::ERR_ABORTED;
      }

      // [prefetch-src]
      if (blink::features::IsPrerender2Enabled() &&
          frame_tree_node_->frame_tree()->is_prerendering()) {
        if (!IsAllowedByCSPDirective(
                initiator_policies->content_security_policies,
                &initiator_context,
                network::mojom::CSPDirectiveName::PrefetchSrc,
                has_followed_redirect, url_upgraded_after_redirect,
                is_response_check, disposition)) {
          error = net::ERR_BLOCKED_BY_CSP;
        }
      }
    }
  }

  // [frame-src]
  if (parent_policies &&
      !IsAllowedByCSPDirective(
          parent_policies->content_security_policies, &parent_context,
          network::mojom::CSPDirectiveName::FrameSrc, has_followed_redirect,
          url_upgraded_after_redirect, is_response_check, disposition)) {
    error = net::ERR_BLOCKED_BY_CSP;
  }

  return error;
}

net::Error NavigationRequest::CheckContentSecurityPolicy(
    bool has_followed_redirect,
    bool url_upgraded_after_redirect,
    bool is_response_check) {
  DCHECK(policy_container_navigation_bundle_.has_value());
  if (common_params_->url.SchemeIs(url::kAboutScheme))
    return net::OK;

  if (common_params_->should_check_main_world_csp ==
      network::mojom::CSPDisposition::DO_NOT_CHECK) {
    return net::OK;
  }

  RenderFrameHostImpl* parent = frame_tree_node()->parent();
  const PolicyContainerPolicies* parent_policies =
      policy_container_navigation_bundle_->ParentPolicies();
  DCHECK(!parent == !parent_policies);
  if (!parent &&
      frame_tree_node()
          ->current_frame_host()
          ->GetRenderViewHost()
          ->GetDelegate()
          ->IsPortal() &&
      frame_tree_node()->render_manager()->GetOuterDelegateNode()) {
    parent = frame_tree_node()
                 ->render_manager()
                 ->GetOuterDelegateNode()
                 ->current_frame_host()
                 ->GetParent();
    // TODO(antoniosartori): If we want to keep checking frame-src for portals,
    // consider storing a snapshot of the parent policies in the
    // `policy_container_navigation_bundle_` at the beginning of the navigation.
    parent_policies = &parent->policy_container_host()->policies();
  }

  const PolicyContainerPolicies* initiator_policies =
      policy_container_navigation_bundle_->InitiatorPolicies();

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
  RenderFrameHostImpl* initiator_rfh =
      GetInitiatorFrameToken().has_value()
          ? RenderFrameHostImpl::FromFrameToken(
                GetInitiatorProcessID(), GetInitiatorFrameToken().value())
          : nullptr;
  if (initiator_rfh && initiator_rfh->commit_navigation_sent_counter() !=
                           initiator_commit_navigation_sent_counter_) {
    // If the initiator frame has navigated away in between, we use a no-op
    // `initiator_csp_context`, so that we won't trigger
    // securitypolicyviolation events in the wrong document.
    initiator_rfh = nullptr;
  }
  RenderFrameHostCSPContext initiator_context(initiator_rfh);

  net::Error report_only_csp_status = CheckCSPDirectives(
      parent_context, parent_policies, initiator_context, initiator_policies,
      has_followed_redirect, url_upgraded_after_redirect, is_response_check,
      network::CSPContext::CHECK_REPORT_ONLY_CSP);

  // upgrade-insecure-requests is handled in the network code for redirects,
  // only do the upgrade here if this is not a redirect.
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
  if (frame_tree_node_->IsMainFrame())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // URLs with no embedded credentials should load correctly.
  if (!common_params_->url.has_username() &&
      !common_params_->url.has_password())
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;

  // Relative URLs on top-level pages that were loaded with embedded credentials
  // should load correctly.
  RenderFrameHostImpl* parent = frame_tree_node_->parent();
  DCHECK(parent);
  const GURL& parent_url = parent->GetLastCommittedURL();
  if (url::Origin::Create(parent_url)
          .IsSameOriginWith(url::Origin::Create(common_params_->url)) &&
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
  if (!base::FeatureList::IsEnabled(features::kBlockCredentialedSubresources))
    return CredentialedSubresourceCheckResult::ALLOW_REQUEST;
  return CredentialedSubresourceCheckResult::BLOCK_REQUEST;
}

NavigationRequest::LegacyProtocolInSubresourceCheckResult
NavigationRequest::CheckLegacyProtocolInSubresource() const {
  // It only applies to subframes.
  if (frame_tree_node_->IsMainFrame())
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  if (!ShouldTreatURLSchemeAsLegacy(common_params_->url))
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  RenderFrameHostImpl* parent = frame_tree_node_->parent();
  DCHECK(parent);
  const GURL& parent_url = parent->GetLastCommittedURL();
  if (ShouldTreatURLSchemeAsLegacy(parent_url))
    return LegacyProtocolInSubresourceCheckResult::ALLOW_REQUEST;

  // Warn the user about the request being blocked.
  const char* console_message =
      "Subresource requests using legacy protocols (like `ftp:`) are blocked. "
      "Please deliver web-accessible resources over modern protocols like "
      "HTTPS. See https://www.chromestatus.com/feature/5709390967472128 for "
      "details.";
  parent->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kWarning,
                              console_message);

  return LegacyProtocolInSubresourceCheckResult::BLOCK_REQUEST;
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

  // TODO(arthursonzogni): Disallow navigations to about:srcdoc initiated from a
  // different frame or from a different window.

  // TODO(arthursonzogni): Disallow browser initiated navigations to
  // about:srcdoc, except session history navigations.

  return AboutSrcDocCheckResult::ALLOW_REQUEST;
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
  // We enforce CSPEE only for frames, not for portals.
  if (IsInMainFrame())
    return CSPEmbeddedEnforcementResult::ALLOW_RESPONSE;

  if (IsSameDocument() || IsServedFromBackForwardCache())
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
    policy_container_navigation_bundle_->AddContentSecurityPolicy(
        required_csp_->Clone());
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
      GetRedirectChain().front().GetOrigin().spec();
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

void NavigationRequest::UpdateCommitNavigationParamsHistory() {
  NavigationController& navigation_controller =
      frame_tree_node_->navigator().controller();
  commit_params_->current_history_list_offset =
      navigation_controller.GetCurrentEntryIndex();
  commit_params_->current_history_list_length =
      navigation_controller.GetEntryCount();
}

void NavigationRequest::RendererAbortedNavigationForTesting() {
  OnRendererAbortedNavigation();
}

void NavigationRequest::OnRendererAbortedNavigation() {
  if (IsWaitingToCommit()) {
    // If the NavigationRequest has already reached READY_TO_COMMIT,
    // `render_frame_host_` owns `this`. Cache any needed state in stack
    // variables to avoid a use-after-free.
    FrameTreeNode* frame_tree_node = frame_tree_node_;
    render_frame_host_->NavigationRequestCancelled(this);
    // Ensure that the speculative RFH, if any, is also cleaned up. In theory,
    // `ResetNavigationRequest()` should handle this; however, it early-returns
    // if there is no navigation request associated with the FrameTreeNode.
    // Changing it to no longer early return breaks a bunch of other code that
    // runs in `CommitPendingIfNecessary()` that expects `DidStopLoading()`
    // won't be called if `FrameTreeNode::navigation_request()` is null...
    frame_tree_node->render_manager()->MaybeCleanUpNavigation();
  } else {
    frame_tree_node_->navigator().CancelNavigation(frame_tree_node_);
  }

  // Do not add code after this, NavigationRequest has been destroyed.
}

void NavigationRequest::HandleInterfaceDisconnection(
    mojo::AssociatedRemote<mojom::NavigationClient>* navigation_client,
    base::OnceClosure error_handler) {
  navigation_client->set_disconnect_handler(std::move(error_handler));
}

void NavigationRequest::IgnoreInterfaceDisconnection() {
  return request_navigation_client_.set_disconnect_handler(base::DoNothing());
}

void NavigationRequest::IgnoreCommitInterfaceDisconnection() {
  return commit_navigation_client_.set_disconnect_handler(base::DoNothing());
}

bool NavigationRequest::IsSameDocument() {
  return NavigationTypeUtils::IsSameDocument(common_params_->navigation_type);
}

int NavigationRequest::EstimateHistoryOffset() {
  if (common_params_->should_replace_current_entry)
    return 0;

  NavigationController& controller = frame_tree_node_->navigator().controller();

  int current_index = controller.GetLastCommittedEntryIndex();
  int pending_index = controller.GetPendingEntryIndex();

  // +1 for non history navigation.
  if (current_index == -1 || pending_index == -1)
    return 1;

  return pending_index - current_index;
}

void NavigationRequest::RecordDownloadUseCountersPrePolicyCheck(
    blink::NavigationDownloadPolicy download_policy) {
  RenderFrameHost* rfh = frame_tree_node_->current_frame_host();
  GetContentClient()->browser()->LogWebFeatureForCurrentPage(
      rfh, blink::mojom::WebFeature::kDownloadPrePolicyCheck);

  // Log UseCounters for opener navigations.
  if (download_policy.IsType(
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
  if (download_policy.IsType(blink::NavigationDownloadType::kSandbox)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInSandbox);
  }

  // Log UseCounters for download without user activation.
  if (download_policy.IsType(blink::NavigationDownloadType::kNoGesture)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadWithoutUserGesture);
  }

  // Log UseCounters for download in ad frame without user activation.
  if (download_policy.IsType(
          blink::NavigationDownloadType::kAdFrameNoGesture)) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        rfh, blink::mojom::WebFeature::kDownloadInAdFrameWithoutUserGesture);
  }

  // Log UseCounters for download in ad frame.
  if (download_policy.IsType(blink::NavigationDownloadType::kAdFrame)) {
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
    case NavigationThrottleRunner::Event::WillStartRequest:
      OnWillStartRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillRedirectRequest:
      OnWillRedirectRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillFailRequest:
      OnWillFailRequestProcessed(result);
      return;
    case NavigationThrottleRunner::Event::WillProcessResponse:
      OnWillProcessResponseProcessed(result);
      return;
    default:
      NOTREACHED();
  }
  NOTREACHED();
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

NavigatorDelegate* NavigationRequest::GetDelegate() const {
  return frame_tree_node()->navigator().GetDelegate();
}

void NavigationRequest::Resume(NavigationThrottle* resuming_throttle) {
  DCHECK(resuming_throttle);
  EnterChildTraceEvent("Resume", this);
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
  throttle_runner_->AddThrottle(std::move(navigation_throttle));
}
bool NavigationRequest::IsDeferredForTesting() {
  return throttle_runner_->GetDeferringThrottle() != nullptr;
}

bool NavigationRequest::IsMhtmlOrSubframe() {
  DCHECK(state_ >= WILL_PROCESS_RESPONSE ||
         state_ == WILL_START_REQUEST && !NeedsUrlLoader());

  return is_mhtml_or_subframe_;
}

bool NavigationRequest::IsForMhtmlSubframe() const {
  return frame_tree_node_->parent() && frame_tree_node_->frame_tree()
                                           ->root()
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
  DCHECK(result.action() != NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE ||
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
    default:
      NOTREACHED();
  }
  // DO NOT ADD CODE AFTER THIS, as the NavigationRequest might have been
  // deleted by the previous calls.
}

void NavigationRequest::WillStartRequest() {
  EnterChildTraceEvent("WillStartRequest", this);
  DCHECK_EQ(state_, WILL_START_REQUEST);

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

  throttle_runner_->RegisterNavigationThrottles();

  // If the content/ embedder did not pass the NavigationUIData at the beginning
  // of the navigation, ask for it now.
  if (!navigation_ui_data_) {
    navigation_ui_data_ = GetDelegate()->GetNavigationUIData(this);
  }

  processing_navigation_throttle_ = true;

  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillStartRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillRedirectRequest(
    const GURL& new_referrer_url,
    const WebExposedIsolationInfo& web_exposed_isolation_info,
    RenderProcessHost* post_redirect_process) {
  EnterChildTraceEvent("WillRedirectRequest", this, "url",
                       common_params_->url.possibly_invalid_spec());
  UpdateStateFollowingRedirect(new_referrer_url);
  UpdateSiteInfo(web_exposed_isolation_info, post_redirect_process);

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
      NavigationThrottleRunner::Event::WillRedirectRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillFailRequest() {
  EnterChildTraceEvent("WillFailRequest", this);

  SetState(WILL_FAIL_REQUEST);
  processing_navigation_throttle_ = true;

  // Notify each throttle of the request.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillFailRequest);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationRequest::WillProcessResponse() {
  EnterChildTraceEvent("WillProcessResponse", this);
  DCHECK_EQ(state_, WILL_PROCESS_RESPONSE);

  processing_navigation_throttle_ = true;

  // Notify each throttle of the response.
  throttle_runner_->ProcessNavigationEvent(
      NavigationThrottleRunner::Event::WillProcessResponse);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
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
    const GURL& previous_main_frame_url,
    NavigationType navigation_type) {
  common_params_->url = params.url;
  did_replace_entry_ = did_replace_entry;
  should_update_history_ = params.should_update_history;
  // A same document navigation with the same url, and no user-gesture is
  // typically the result of 'history.replaceState().' As the page is
  // controlling this, the user doesn't really think of this as a navigation
  // and it doesn't make sense to log this in history. Logging this in history
  // would lead to lots of visits to a particular page, which impacts the
  // visit count.
  if (should_update_history_ && IsSameDocument() && !HasUserGesture() &&
      params.url == previous_main_frame_url) {
    should_update_history_ = false;
  }
  previous_main_frame_url_ = previous_main_frame_url;
  navigation_type_ = navigation_type;

  if (net_error_ != net::OK) {
    EnterChildTraceEvent("DidCommitNavigation: error page", this);
    SetState(DID_COMMIT_ERROR_PAGE);
  } else {
    EnterChildTraceEvent("DidCommitNavigation", this);
    SetState(DID_COMMIT);
  }

  StopCommitTimeout();

  // Switching BrowsingInstance because of COOP or top-level cross browsing
  // instance navigation resets the name of the frame. The renderer already
  // knows locally about it because we sent an empty name at frame creation
  // time. The renderer has now committed the page and we can safely enforce the
  // empty name on the browser side.
  bool should_clear_browsing_instance_name =
      coop_status().require_browsing_instance_swap() ||
      (commit_params().is_cross_site_cross_browsing_context_group &&
       base::FeatureList::IsEnabled(
           features::kClearCrossSiteCrossBrowsingContextGroupWindowName));

  if (should_clear_browsing_instance_name) {
    std::string name, unique_name;
    // The "swap" only affect main frames, that have an empty unique name.
    DCHECK(frame_tree_node_->unique_name().empty());
    frame_tree_node_->SetFrameName(name, unique_name);
  }

  // Record metrics for the time it took to commit the navigation if it was to
  // another document without error.
  if (!IsSameDocument() && state_ != DID_COMMIT_ERROR_PAGE) {
    ui::PageTransition transition = common_params_->transition;
    absl::optional<bool> is_background =
        render_frame_host_->GetProcess()->IsProcessBackgrounded();

    RecordStartToCommitMetrics(
        common_params_->navigation_start, transition, ready_to_commit_time_,
        is_background, is_same_process_, frame_tree_node_->IsMainFrame());
  }

  DCHECK(!frame_tree_node_->IsMainFrame() || navigation_entry_committed)
      << "Only subframe navigations can get here without changing the "
      << "NavigationEntry";
  subframe_entry_committed_ = navigation_entry_committed;

  // For successful navigations, ensure the frame owner element is no longer
  // collapsed as a result of a prior navigation.
  if (state_ != DID_COMMIT_ERROR_PAGE && !frame_tree_node()->IsMainFrame()) {
    // The last committed load in collapsed frames will be an error page with
    // |kUnreachableWebDataURL|. Same-document navigation should not be
    // possible.
    DCHECK(!IsSameDocument() || !frame_tree_node()->is_collapsed());
    frame_tree_node()->SetCollapsed(false);
  }

  if (service_worker_handle_) {
    // Notify the service worker navigation handle that the navigation finished
    // committing.
    service_worker_handle_->OnEndNavigationCommit();
  }
}

SiteInfo NavigationRequest::GetSiteInfoForCommonParamsURL(
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  // TODO(alexmos): Using |starting_site_instance_|'s IsolationContext may not
  // be correct for cross-BrowsingInstance redirects.
  return SiteInfo::Create(starting_site_instance_->GetIsolationContext(),
                          GetUrlInfo(), web_exposed_isolation_info);
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

#if defined(OS_ANDROID)
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
  HandleInterfaceDisconnection(
      &request_navigation_client_,
      base::BindOnce(&NavigationRequest::OnRendererAbortedNavigation,
                     base::Unretained(this)));
}

bool NavigationRequest::NeedsUrlLoader() {
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

  ContentBrowserClient* client = GetContentClient()->browser();
  BrowserContext* context =
      frame_tree_node_->navigator().controller().GetBrowserContext();

  url::Origin origin = GetOriginForURLLoaderFactory();
  if (client->ShouldAllowInsecurePrivateNetworkRequests(context, origin)) {
    // The content browser client decided to make an exception for this URL.
    private_network_request_policy_ =
        network::mojom::PrivateNetworkRequestPolicy::kAllow;
    return;
  }

  const PolicyContainerPolicies& policies =
      policy_container_navigation_bundle_->FinalPolicies();

  // Requests initiated from secure contexts are never blocked; depending
  // on a feature flag, we show a warning in DevTools.
  if (policies.is_web_secure_context) {
    private_network_request_policy_ =
        base::FeatureList::IsEnabled(
            features::kWarnAboutSecurePrivateNetworkRequests)
            ? network::mojom::PrivateNetworkRequestPolicy::kWarn
            : network::mojom::PrivateNetworkRequestPolicy::kAllow;
    return;
  }

  // Requests from non-secure contexts in the unknown address space are allowed.
  if (policies.ip_address_space == network::mojom::IPAddressSpace::kUnknown) {
    private_network_request_policy_ =
        network::mojom::PrivateNetworkRequestPolicy::kAllow;
    return;
  }

  // Requests from non-secure contexts are only blocked if the feature is
  // enabled, otherwise we simply show a warning in DevTools.
  private_network_request_policy_ =
      base::FeatureList::IsEnabled(
          features::kBlockInsecurePrivateNetworkRequests)
          ? network::mojom::PrivateNetworkRequestPolicy::kBlock
          : network::mojom::PrivateNetworkRequestPolicy::kWarn;
}

void NavigationRequest::ReadyToCommitNavigation(bool is_error) {
  EnterChildTraceEvent("ReadyToCommitNavigation", this);

  // We may come back to here asynchronously, and the renderer may be destroyed
  // in the meantime. Renderer-initiated navigations listen to mojo
  // disconnection from the renderer NavigationClient; but browser-initiated
  // navigations do not, so we must look explicitly. We should not proceed and
  // claim "ReadyToCommitNavigation" to the delegate if the renderer is gone.
  if (!render_frame_host_->IsRenderFrameLive()) {
    OnRendererAbortedNavigation();
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
  frame_tree_node_->TransferNavigationRequestOwnership(render_frame_host_);

  // When a speculative RenderFrameHost reaches ReadyToCommitNavigation, the
  // browser process has asked the renderer to commit the navigation and is
  // waiting for confirmation of the commit. Update the LifecycleStateImpl to
  // kPendingCommit as RenderFrameHost isn't considered speculative anymore and
  // was chosen to commit as this navigation's final RenderFrameHost.
  if (render_frame_host_->lifecycle_state() ==
      RenderFrameHostImpl::LifecycleStateImpl::kSpeculative) {
    // Only cross-RenderFrameHost navigations create speculative
    // RenderFrameHosts whereas SameDocument, BackForwardCache and
    // PrerenderedActivation navigations don't.
    DCHECK(!IsSameDocument() && !IsPageActivation());
    render_frame_host_->SetLifecycleStateToPendingCommit();
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

  if (appcache_handle_) {
    DCHECK(appcache_handle_->host());
    appcache_handle_->host()->SetProcessId(
        render_frame_host_->GetProcess()->GetID());
  }

  RenderFrameHostImpl* previous_render_frame_host =
      frame_tree_node_->current_frame_host();

  // Record metrics for the time it takes to get to this state from the
  // beginning of the navigation.
  if (!IsSameDocument() && !is_error) {
    is_same_process_ = render_frame_host_->GetProcess()->GetID() ==
                       previous_render_frame_host->GetProcess()->GetID();

    RecordReadyToCommitMetrics(previous_render_frame_host, render_frame_host_,
                               *common_params_.get(), ready_to_commit_time_,
                               origin_agent_cluster_end_result_);
  }

  // TODO(https://crbug.com/888079) Take sandbox into account.
  same_origin_ = (previous_render_frame_host->GetLastCommittedOrigin() ==
                  GetOriginForURLLoaderFactory());

  SetExpectedProcess(render_frame_host_->GetProcess());

  if (!IsSameDocument()) {
#if DCHECK_IS_ON()
    DCHECK(is_safe_to_delete_);
    base::AutoReset<bool> resetter(&is_safe_to_delete_, false);
#endif
    GetDelegate()->ReadyToCommitNavigation(this);
  }

  if (ready_to_commit_callback_for_testing_)
    std::move(ready_to_commit_callback_for_testing_).Run();
}

std::unique_ptr<AppCacheNavigationHandle>
NavigationRequest::TakeAppCacheHandle() {
  return std::move(appcache_handle_);
}

bool NavigationRequest::IsWaitingToCommit() {
  return state_ == READY_TO_COMMIT;
}

bool NavigationRequest::WasEarlyHintsPreloadLinkHeaderReceived() {
  return was_early_hints_preload_link_header_received_;
}

// static
bool NavigationRequest::IsLoadDataWithBaseURL(const GURL& url,
                                              const GURL& base_url) {
  return url.SchemeIs(url::kDataScheme) && !base_url.is_empty();
}

// static
bool NavigationRequest::IsLoadDataWithBaseURL(
    const mojom::CommonNavigationParams& common_params) {
  return IsLoadDataWithBaseURL(common_params.url,
                               common_params.base_url_for_data_url);
}

// static
bool NavigationRequest::IsLoadDataWithBaseURLAndUnreachableURL(
    bool is_main_frame,
    const mojom::CommonNavigationParams& common_params,
    const absl::optional<std::string>& data_url_as_string) {
  if (!is_main_frame || !IsLoadDataWithBaseURL(common_params))
    return false;

  // On main frame loadDataURLWithBaseURL navigations, history_url_for_data_url
  // is saved in WebNavigationParams' unreachable_url in the renderer and sent
  // back to the browser, unless the base_url is invalid and data_url_as_string
  // is not used. See https://crbug.com/522567 and handling of data: URLs in
  // RenderFrameImpl::CommitNavigation() for more details.
  const bool has_history_url_for_data_url =
      !common_params.history_url_for_data_url.is_empty();
  const bool has_non_empty_data_url_as_string =
      data_url_as_string.has_value() && !data_url_as_string.value().empty();
  if (has_history_url_for_data_url) {
    // history_url_for_data_url must only be set if we originally set
    // base_url_for_data_url or data_url_as_string.
    DCHECK(!common_params.base_url_for_data_url.is_empty() ||
           has_non_empty_data_url_as_string);
  }

  return (common_params.base_url_for_data_url.is_valid() ||
          has_non_empty_data_url_as_string) &&
         has_history_url_for_data_url;
}

url::Origin NavigationRequest::GetOriginForURLLoaderFactory() {
  // The origin to commit is not known until we get the final network response.
  DCHECK_GE(state_, WILL_PROCESS_RESPONSE);

  if (IsSameDocument() || IsPageActivation())
    return GetRenderFrameHost()->GetLastCommittedOrigin();

  // Calculate an approximation of the origin. The sandbox/csp are ignored.
  url::Origin origin = GetOriginForURLLoaderFactoryUnchecked(this);

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
  bool use_opaque_origin = (sandbox_flags_to_commit_.value() &
                            network::mojom::WebSandboxFlags::kOrigin) ==
                           network::mojom::WebSandboxFlags::kOrigin;
  // TODO(https://crbug.com/1158370): Move special-casing error pages into
  // ComputeSandboxFlagsToCommit (and renderer-side origin calculations) so that
  // the most strict sandbox flags are applied.
  if (GetNetErrorCode() != net::OK)
    use_opaque_origin = true;
  if (use_opaque_origin)
    origin = origin.DeriveNewOpaqueOrigin();

  // MHTML documents should commit as an opaque origin. They should not be able
  // to make network request on behalf of the real origin.
  DCHECK(!IsMhtmlOrSubframe() || origin.opaque());

  // https://crbug.com/1041376) of the origin that will be committed because of
  // |this| NavigationRequest.

  // Note that GetRenderFrameHost() only allows to retrieve the RenderFrameHost
  // once it has been set for this navigation.  This will happens either at
  // WillProcessResponse time for regular navigations or at WillFailRequest time
  // for error pages.
  // Check that |origin| is allowed to be accessed from the process that is the
  // target of this navigation.
  if (GetRenderFrameHost()->ShouldBypassSecurityChecksForErrorPage(this))
    return origin;

  // MHTML iframes can load documents from any origin, no matter the current
  // policy of the process being used. This is because the content is loaded
  // from the MHTML archive within the process. There are no data loaded from
  // the network.
  if (IsForMhtmlSubframe())
    return origin;

  int process_id = GetRenderFrameHost()->GetProcess()->GetID();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  CHECK(policy->CanAccessDataForOrigin(process_id, origin));
  return origin;
}

void NavigationRequest::WriteIntoTrace(perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("navigation_id", navigation_id_);
  dict.Add("has_committed", HasCommitted());
  dict.Add("is_error_page", IsErrorPage());
  dict.Add("net_error", net_error_);
  dict.Add("url", common_params_->url);
  dict.Add("frame_tree_node", frame_tree_node_);
  dict.Add("browser_initiated", browser_initiated_);
  dict.Add("from_begin_navigation", from_begin_navigation_);
  dict.Add("is_for_commit", is_for_commit_);
  dict.Add("reload_type", reload_type_);
  dict.Add("state", state_);
  dict.Add("navigation_type", common_params_->navigation_type);

  if (IsServedFromBackForwardCache()) {
    dict.Add("served_from_bfcache", true);
    dict.Add("rfh_restored_from_bfcache",
             rfh_restored_from_back_forward_cache_);
  }

  if (state_ >= WILL_PROCESS_RESPONSE)
    dict.Add("render_frame_host", GetRenderFrameHost());
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
  PingNetworkService(base::BindOnce(
      [](base::Time start_time) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Navigation.CommitTimeout.NetworkServicePingTime",
            base::Time::Now() - start_time);
      },
      base::Time::Now()));
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.CommitTimeout.NetworkServiceAvailability",
      GetNetworkServiceAvailability());
  base::TimeDelta last_crash_time = GetTimeSinceLastNetworkServiceCrash();
  if (!last_crash_time.is_zero()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Navigation.CommitTimeout.NetworkServiceLastCrashTime",
        last_crash_time);
  }
  UMA_HISTOGRAM_BOOLEAN("Navigation.CommitTimeout.IsRendererProcessReady",
                        GetRenderFrameHost()->GetProcess()->IsReady());
  UMA_HISTOGRAM_ENUMERATION("Navigation.CommitTimeout.Scheme",
                            GetScheme(common_params_->url));
  UMA_HISTOGRAM_BOOLEAN("Navigation.CommitTimeout.IsMainFrame",
                        frame_tree_node_->IsMainFrame());
  base::UmaHistogramSparse("Navigation.CommitTimeout.ErrorCode", -net_error_);
  render_process_blocked_state_changed_subscription_ = {};
  GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsUnresponsive(
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

const net::HttpResponseHeaders* NavigationRequest::GetResponseHeaders() {
  return response_head_.get() ? response_head_->headers.get() : nullptr;
}

mojom::DidCommitProvisionalLoadParamsPtr
NavigationRequest::MakeDidCommitProvisionalLoadParamsForActivation() {
  // Use the DidCommitProvisionalLoadParams last used to commit the frame being
  // restored as a starting point.
  mojom::DidCommitProvisionalLoadParamsPtr params =
      render_frame_host_->TakeLastCommitParams();

  // Params must have been set when the RFH being restored from the cache last
  // navigated.
  CHECK(params);

  DCHECK_EQ(params->http_status_code, net::HTTP_OK);
  DCHECK_EQ(params->url_is_unreachable, false);

  params->intended_as_new_entry = commit_params().intended_as_new_entry;
  params->should_replace_current_entry =
      common_params().should_replace_current_entry;
  DCHECK_EQ(params->post_id, -1);
  params->navigation_token = commit_params().navigation_token;
  DCHECK_EQ(params->url, common_params().url);
  params->should_update_history = true;
  params->gesture = common_params().has_user_gesture ? NavigationGestureUser
                                                     : NavigationGestureAuto;
  DCHECK_EQ(params->method, common_params().method);
  params->item_sequence_number = frame_entry_item_sequence_number_;
  params->document_sequence_number = frame_entry_document_sequence_number_;
  params->transition = common_params().transition;
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
  DCHECK_EQ(params->origin, commit_params().origin_to_commit.value());
  params->page_state = commit_params().page_state;
  return params;
}

mojom::DidCommitProvisionalLoadParamsPtr
NavigationRequest::MakeDidCommitProvisionalLoadParamsForPrerenderActivation() {
  // Start with the provisional load parameters shared between all page
  // activation types.
  mojom::DidCommitProvisionalLoadParamsPtr params =
      MakeDidCommitProvisionalLoadParamsForActivation();
  // TODO(https://crbug.com/1179428): Investigate when a new entry should
  // replace an old one when prerendering a page.
  params->did_create_new_entry = true;
  // Unlike bfcache restore, Prerendering makes a new navigation entry, so it
  // doesn't need to set params->page_state..
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

bool NavigationRequest::HasCommitted() {
  return state_ == DID_COMMIT || state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationRequest::IsErrorPage() {
  return state_ == DID_COMMIT_ERROR_PAGE;
}

bool NavigationRequest::DidEncounterError() const {
  return net_error_ != net::OK;
}

net::HttpResponseInfo::ConnectionInfo NavigationRequest::GetConnectionInfo() {
  return response() ? response()->connection_info
                    : net::HttpResponseInfo::ConnectionInfo();
}

bool NavigationRequest::IsInMainFrame() {
  return frame_tree_node()->IsMainFrame();
}

RenderFrameHostImpl* NavigationRequest::GetParentFrame() {
  return IsInMainFrame() ? nullptr : frame_tree_node()->parent();
}

bool NavigationRequest::IsInPrimaryMainFrame() {
  return IsInMainFrame() &&
         frame_tree_node()->frame_tree()->type() == FrameTree::Type::kPrimary;
}

bool NavigationRequest::IsPrerenderedPageActivation() {
  CHECK_GE(state_, WILL_START_REQUEST);
  return prerender_frame_tree_node_id_ != RenderFrameHost::kNoFrameTreeNodeId;
}

int NavigationRequest::GetFrameTreeNodeId() {
  return frame_tree_node()->frame_tree_node_id();
}

bool NavigationRequest::WasResponseCached() {
  return response() && response()->was_fetched_via_cache;
}

bool NavigationRequest::HasPrefetchedAlternativeSubresourceSignedExchange() {
  return !commit_params_->prefetched_signed_exchanges.empty();
}

int64_t NavigationRequest::GetNavigationId() {
  return navigation_id_;
}

ukm::SourceId NavigationRequest::GetNextPageUkmSourceId() {
  // If the navigation is restoring from back-forward cache, the UKM id
  // will get restored, too.
  if (rfh_restored_from_back_forward_cache_)
    return rfh_restored_from_back_forward_cache_->GetPageUkmSourceId();

  // If this is the same document or a child frame navigation the UKM id will
  // not change from it.
  if (IsSameDocument() || !IsInMainFrame())
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
  return !browser_initiated_;
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
  return common_params().transition;
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
RenderFrameHostImpl* NavigationRequest::GetRenderFrameHost() {
  // Only allow the RenderFrameHost to be retrieved once it has been set for
  // this navigation. This will happens either at WillProcessResponse time for
  // regular navigations or at WillFailRequest time for error pages.
  CHECK_GE(state_, WILL_PROCESS_RESPONSE)
      << "This accessor should only be called after a RenderFrameHost has "
         "been picked for this navigation.";
  static_assert(WILL_FAIL_REQUEST > WILL_PROCESS_RESPONSE,
                "WillFailRequest state should come after WillProcessResponse");
  return render_frame_host_;
}

const net::HttpRequestHeaders& NavigationRequest::GetRequestHeaders() {
  if (!request_headers_) {
    request_headers_.emplace();
    request_headers_->AddHeadersFromString(begin_params_->headers);
  }
  return *request_headers_;
}

const absl::optional<net::SSLInfo>& NavigationRequest::GetSSLInfo() {
  return ssl_info_;
}

const absl::optional<net::AuthChallengeInfo>&
NavigationRequest::GetAuthChallengeInfo() {
  return auth_challenge_info_;
}

net::ResolveErrorInfo NavigationRequest::GetResolveErrorInfo() {
  return resolve_error_info_;
}

net::IsolationInfo NavigationRequest::GetIsolationInfo() {
  if (isolation_info_)
    return isolation_info_.value();

  // TODO(crbug.com/979296): Consider changing this code to copy an origin
  // instead of creating one from a URL which lacks opacity information.
  return frame_tree_node_->current_frame_host()
      ->ComputeIsolationInfoForNavigation(common_params_->url);
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

const GURL& NavigationRequest::GetPreviousMainFrameURL() {
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

ReloadType NavigationRequest::GetReloadType() {
  return reload_type_;
}

RestoreType NavigationRequest::GetRestoreType() {
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

const absl::optional<blink::Impression>& NavigationRequest::GetImpression() {
  return begin_params().impression;
}

const absl::optional<blink::LocalFrameToken>&
NavigationRequest::GetInitiatorFrameToken() {
  return initiator_frame_token_;
}

int NavigationRequest::GetInitiatorProcessID() {
  return initiator_process_id_;
}

const absl::optional<url::Origin>& NavigationRequest::GetInitiatorOrigin() {
  return common_params().initiator_origin;
}

const std::vector<std::string>& NavigationRequest::GetDnsAliases() {
  static const base::NoDestructor<std::vector<std::string>> emptyvector_result;
  return response_head_ ? response_head_->dns_aliases : *emptyvector_result;
}

bool NavigationRequest::IsSameProcess() {
  return is_same_process_;
}

NavigationEntry* NavigationRequest::GetNavigationEntry() {
  if (nav_entry_id_ == 0)
    return nullptr;

  NavigationControllerImpl* controller = GetNavigationController();
  NavigationEntry* entry = controller->GetEntryWithUniqueID(nav_entry_id_);
  if (entry)
    return entry;
  return (controller->GetPendingEntry() &&
          controller->GetPendingEntry()->GetUniqueID() == nav_entry_id_)
             ? controller->GetPendingEntry()
             : nullptr;
}

int NavigationRequest::GetNavigationEntryOffset() {
  return navigation_entry_offset_;
}

const net::ProxyServer& NavigationRequest::GetProxyServer() {
  return proxy_server_;
}

GlobalFrameRoutingId NavigationRequest::GetPreviousRenderFrameHostId() {
  return previous_render_frame_host_id_;
}

bool NavigationRequest::IsServedFromBackForwardCache() {
  const NavigationRequest& request = *this;
  return request.IsServedFromBackForwardCache();
}

void NavigationRequest::SetIsOverridingUserAgent(bool override_ua) {
  // Only add specific headers when creating a NavigationRequest before the
  // network request is made, not at commit time.
  if (is_for_commit_)
    return;

  // This code assumes it is only called from DidStartNavigation().
  DCHECK(!ua_change_requires_reload_);

  commit_params_->is_overriding_user_agent = override_ua;
  // The new document, created by this navigation, will be honoring the new
  // value. It will be reflected into its NavigationEntry's when committing the
  // new document at DidCommitNavigation time.

  net::HttpRequestHeaders headers;
  headers.AddHeadersFromString(begin_params_->headers);
  auto user_agent_override = GetUserAgentOverride();
  headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                    user_agent_override.empty()
                        ? GetContentClient()->browser()->GetUserAgent()
                        : user_agent_override);
  BrowserContext* browser_context =
      frame_tree_node_->navigator().controller().GetBrowserContext();
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (client_hints_delegate) {
    UpdateNavigationRequestClientUaHeaders(
        common_params_->url, client_hints_delegate, is_overriding_user_agent(),
        frame_tree_node_, &headers);
  }
  begin_params_->headers = headers.ToString();
  // |request_headers_| comes from |begin_params_|. Clear |request_headers_| now
  // so that if |request_headers_| are needed, they will be updated.
  request_headers_.reset();
}

void NavigationRequest::SetSilentlyIgnoreErrors() {
  silently_ignore_errors_ = true;
}

// static
NavigationRequest* NavigationRequest::From(NavigationHandle* handle) {
  return static_cast<NavigationRequest*>(handle);
}

// static
ReloadType NavigationRequest::NavigationTypeToReloadType(
    mojom::NavigationType type) {
  if (type == mojom::NavigationType::RELOAD)
    return ReloadType::NORMAL;
  if (type == mojom::NavigationType::RELOAD_BYPASSING_CACHE)
    return ReloadType::BYPASSING_CACHE;
  if (type == mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL)
    return ReloadType::ORIGINAL_REQUEST_URL;
  return ReloadType::NONE;
}

bool NavigationRequest::IsNavigationStarted() const {
  return is_navigation_started_;
}

bool NavigationRequest::RequiresInitiatorBasedSourceSiteInstance() const {
  const bool is_data_or_about =
      common_params_->url.SchemeIs(url::kDataScheme) ||
      common_params_->url.IsAboutBlank();

  const bool has_valid_initiator =
      common_params_->initiator_origin &&
      common_params_->initiator_origin->GetTupleOrPrecursorTupleIfOpaque()
          .IsValid();

  return is_data_or_about && has_valid_initiator && !dest_site_instance_;
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

void NavigationRequest::RestartBackForwardCachedNavigation() {
  TRACE_EVENT0("navigation",
               "NavigationRequest::RestartBackForwardCachedNavigation");
  CHECK(IsServedFromBackForwardCache());
  restarting_back_forward_cached_navigation_ = true;
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NavigationRequest::RestartBackForwardCachedNavigationImpl,
                     weak_factory_.GetWeakPtr()));
  // Delete the loader to ensure that it does not try to commit current
  // navigation before the task above deletes it.
  loader_.reset();
}

void NavigationRequest::RestartBackForwardCachedNavigationImpl() {
  TRACE_EVENT0("navigation",
               "NavigationRequest::RestartBackForwardCachedNavigationImpl");
  RenderFrameHostImpl* rfh = rfh_restored_from_back_forward_cache();
  CHECK(rfh);
  CHECK_EQ(rfh->frame_tree_node()->navigation_request(), this);

  NavigationControllerImpl* controller = GetNavigationController();
  int nav_index = controller->GetEntryIndexWithUniqueID(nav_entry_id());

  // If the NavigationEntry was deleted, do not do anything.
  if (nav_index == -1)
    return;

  controller->GoToIndex(nav_index);
}

void NavigationRequest::ForceEnableOriginTrials(
    const std::vector<std::string>& trials) {
  DCHECK(!HasCommitted());
  commit_params_->force_enabled_origin_trials = trials;
}

absl::optional<network::mojom::BlockedByResponseReason>
NavigationRequest::EnforceCOEP() {
  // https://html.spec.whatwg.org/#check-a-navigation-response's-adherence-to-its-embedder-policy
  auto* parent_frame = GetParentFrame();
  if (!parent_frame) {
    return absl::nullopt;
  }
  const auto& url = common_params_->url;
  // Some special URLs not loaded using the network are inheriting the
  // Cross-Origin-Embedder-Policy header from their parent.
  if (url.SchemeIsBlob() || url.SchemeIs(url::kDataScheme)) {
    return absl::nullopt;
  }
  return network::CrossOriginResourcePolicy::IsNavigationBlocked(
      url, redirect_chain_[0], parent_frame->GetLastCommittedOrigin(),
      *response_head_, parent_frame->GetLastCommittedOrigin(),
      request_destination(), parent_frame->cross_origin_embedder_policy(),
      parent_frame->coep_reporter());
}

bool NavigationRequest::CoopCoepSanityCheck() {
  network::mojom::CrossOriginOpenerPolicyValue coop_value =
      IsInMainFrame() ? coop_status_.current_coop().value
                      : render_frame_host_->GetMainFrame()
                            ->cross_origin_opener_policy()
                            .value;
  if (coop_value ==
          network::mojom::CrossOriginOpenerPolicyValue::kSameOriginPlusCoep &&
      !CompatibleWithCrossOriginIsolated(cross_origin_embedder_policy_)) {
    NOTREACHED();
    base::debug::DumpWithoutCrashing();
    return false;
  }
  return true;
}

std::unique_ptr<PeakGpuMemoryTracker>
NavigationRequest::TakePeakGpuMemoryTracker() {
  return std::move(loading_mem_tracker_);
}

std::unique_ptr<NavigationEarlyHintsManager>
NavigationRequest::TakeEarlyHintsManager() {
  return std::move(early_hints_manager_);
}

network::mojom::ClientSecurityStatePtr
NavigationRequest::BuildClientSecurityState() {
  auto client_security_state = network::mojom::ClientSecurityState::New();

  const PolicyContainerPolicies& policies =
      policy_container_navigation_bundle_->FinalPolicies();
  client_security_state->is_web_secure_context = policies.is_web_secure_context;
  client_security_state->ip_address_space = policies.ip_address_space;

  client_security_state->cross_origin_embedder_policy =
      cross_origin_embedder_policy_;
  client_security_state->private_network_request_policy =
      private_network_request_policy_;

  return client_security_state;
}

std::string NavigationRequest::GetUserAgentOverride() {
  return is_overriding_user_agent() ? frame_tree_node_->navigator()
                                          .GetDelegate()
                                          ->GetUserAgentOverride()
                                          .ua_string_override
                                    : std::string();
}

NavigationControllerImpl* NavigationRequest::GetNavigationController() {
  return &frame_tree_node_->navigator().controller();
}

PrerenderHostRegistry& NavigationRequest::GetPrerenderHostRegistry() {
  return *frame_tree_node_->current_frame_host()
              ->delegate()
              ->GetPrerenderHostRegistry();
}

mojo::PendingRemote<network::mojom::CookieAccessObserver>
NavigationRequest::CreateCookieAccessObserver() {
  mojo::PendingRemote<network::mojom::CookieAccessObserver> remote;
  cookie_observers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void NavigationRequest::OnCookiesAccessed(
    network::mojom::CookieAccessDetailsPtr details) {
  // TODO(721329): We should not send information to the current frame about
  // (potentially unrelated) ongoing navigation, but at the moment we don't
  // have another way to add messages to DevTools console.
  EmitCookieWarningsAndMetrics(frame_tree_node()->current_frame_host(),
                               details);

  CookieAccessDetails allowed;
  CookieAccessDetails blocked;
  SplitCookiesIntoAllowedAndBlocked(details, &allowed, &blocked);
  if (!allowed.cookie_list.empty())
    GetDelegate()->OnCookiesAccessed(this, allowed);
  if (!blocked.cookie_list.empty())
    GetDelegate()->OnCookiesAccessed(this, blocked);
}

void NavigationRequest::Clone(
    mojo::PendingReceiver<network::mojom::CookieAccessObserver> observer) {
  cookie_observers_.Add(this, std::move(observer));
}

std::vector<mojo::PendingReceiver<network::mojom::CookieAccessObserver>>
NavigationRequest::TakeCookieObservers() {
  return cookie_observers_.TakeReceivers();
}

void NavigationRequest::ComputePoliciesToCommit() {
  policy_container_navigation_bundle_->SetIPAddressSpace(
      CalculateIPAddressSpace(common_params_->url, response_head_.get()));

  // Use the unchecked / non-sandboxed origin to calculate potential
  // trustworthiness. Indeed, the potential trustworthiness check should apply
  // to the origin of the creation URL, prior to opaquification.
  policy_container_navigation_bundle_->SetIsOriginPotentiallyTrustworthy(
      network::IsOriginPotentiallyTrustworthy(
          GetOriginForURLLoaderFactoryUnchecked(this)));
  policy_container_navigation_bundle_->ComputePolicies(common_params_->url);

  ComputeSandboxFlagsToCommit(/*for_error=*/false);
}

void NavigationRequest::ComputePoliciesToCommitForError() {
  policy_container_navigation_bundle_->ComputePoliciesForError();
  ComputeSandboxFlagsToCommit(/*for_error=*/true);
}

void NavigationRequest::ComputeSandboxFlagsToCommit(bool for_error) {
  DCHECK(commit_params_);
  DCHECK(!HasCommitted());
  DCHECK(!sandbox_flags_to_commit_);

  // Inherit sandbox from the frame.
  sandbox_flags_to_commit_ = commit_params_->frame_policy.sandbox_flags;

  // The document can also restrict sandbox further, via its CSP.
  const PolicyContainerPolicies& policies_to_commit =
      policy_container_navigation_bundle_->FinalPolicies();
  for (const auto& csp : policies_to_commit.content_security_policies)
    *sandbox_flags_to_commit_ |= csp->sandbox;

  if (!for_error && response_head_ &&
      !devtools_instrumentation::ShouldBypassCSP(*this)) {
    for (const auto& csp :
         response_head_->parsed_headers->content_security_policy) {
      *sandbox_flags_to_commit_ |= csp->sandbox;
    }
  }

  // The URL of a document loaded from a MHTML archive is controlled by the
  // Content-Location header. This can be set to an arbitrary URL. This is
  // potentially dangerous. For this reason we force the document to be
  // sandboxed, providing exceptions only for creating new windows. This
  // includes disallowing javascript and using an opaque origin.
  if (IsMhtmlOrSubframe()) {
    *sandbox_flags_to_commit_ |= ~network::mojom::WebSandboxFlags::kPopups &
                                 ~network::mojom::WebSandboxFlags::
                                     kPropagatesToAuxiliaryBrowsingContexts;
  }
  commit_params_->sandbox_flags = *sandbox_flags_to_commit_;
}

void NavigationRequest::CheckStateTransition(NavigationState state) const {
#if DCHECK_IS_ON()
  // See
  // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/navigation-request-navigation-state.png
  // clang-format off
  static const base::NoDestructor<StateTransitions<NavigationState>>
      transitions(StateTransitions<NavigationState>({
          {NOT_STARTED, {
              WAITING_FOR_RENDERER_RESPONSE,
              WILL_START_NAVIGATION,
              WILL_START_REQUEST,
          }},
          {WAITING_FOR_RENDERER_RESPONSE, {
              WILL_START_NAVIGATION,
          }},
          {WILL_START_NAVIGATION, {
              WILL_START_REQUEST,
              WILL_FAIL_REQUEST,
          }},
          {WILL_START_REQUEST, {
              WILL_REDIRECT_REQUEST,
              WILL_PROCESS_RESPONSE,
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
  // TODO(crbug.com/774663): Maybe take `ThrottleCheckResult::action()` into
  // account as well.
  // If the request was canceled by the user, do not show an error page.
  if (net::ERR_ABORTED == net_error_ ||
      // Some embedders suppress error pages to allow custom error handling.
      silently_ignore_errors_ ||
      // <webview> guests suppress net::ERR_BLOCKED_BY_CLIENT.
      (net::ERR_BLOCKED_BY_CLIENT == net_error_ &&
       silently_ignore_blocked_by_client_)) {
    frame_tree_node_->ResetNavigationRequest(false);
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
      blink::mojom::FrameOwnerElementType::kObject) {
    RenderFallbackContentForObjectTag();
    frame_tree_node_->ResetNavigationRequest(false);
    return true;
  }

  return false;
}

bool NavigationRequest::ShouldRenderFallbackContentForResponse(
    const net::HttpResponseHeaders& http_headers) const {
  return frame_tree_node()->frame_owner_element_type() ==
             blink::mojom::FrameOwnerElementType::kObject &&
         !network::cors::IsOkStatus(http_headers.response_code());
}

void NavigationRequest::RenderFallbackContentForObjectTag() {
  // https://whatwg.org/C/iframe-embed-object.html#the-object-element:fallback-content-5:
  // Fallback content is represented by the children of the <object> tag, so it
  // will be rendered in the process of the parent's document.
  DCHECK_EQ(blink::mojom::FrameOwnerElementType::kObject,
            frame_tree_node_->frame_owner_element_type());
  if (RenderFrameProxyHost* proxy =
          frame_tree_node_->render_manager()->GetProxyToParent()) {
    proxy->GetAssociatedRemoteFrame()->RenderFallbackContent();
  } else {
    frame_tree_node_->current_frame_host()
        ->GetAssociatedLocalFrame()
        ->RenderFallbackContent();
  }
}

bool NavigationRequest::IsServedFromBackForwardCache() const {
  return rfh_restored_from_back_forward_cache_ != nullptr;
}

bool NavigationRequest::IsPageActivation() const {
  return const_cast<NavigationRequest*>(this)->IsPrerenderedPageActivation() ||
         IsServedFromBackForwardCache();
}

std::unique_ptr<NavigationEntryImpl>
NavigationRequest::TakePrerenderNavigationEntry() {
  return std::move(prerender_navigation_entry_);
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
    // TODO(https://crbug.com/721329): We should have a way of sending console
    // messaged to devtools without going through the renderer.
    render_frame_host_->AddMessageToConsole(message.level,
                                            std::move(message.message));
  }
  console_messages_.clear();
}

}  // namespace content
