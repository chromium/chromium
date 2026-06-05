// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include "base/check_is_test.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prefetch/assert_prefetch_container_observer.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_container_observer.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_isolated_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/proxy_lookup_client_impl.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/url_request/redirect_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/headers_matcher.h"
#include "services/network/public/cpp/request_header_to_enum.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace content {
namespace {

PrefetchStatus PrefetchStatusFromIneligibleReason(
    PreloadingEligibility eligibility) {
  switch (eligibility) {
    case PreloadingEligibility::kBatterySaverEnabled:
      return PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled;
    case PreloadingEligibility::kDataSaverEnabled:
      return PrefetchStatus::kPrefetchIneligibleDataSaverEnabled;
    case PreloadingEligibility::kExistingProxy:
      return PrefetchStatus::kPrefetchIneligibleExistingProxy;
    case PreloadingEligibility::kHostIsNonUnique:
      return PrefetchStatus::kPrefetchIneligibleHostIsNonUnique;
    case PreloadingEligibility::kNonDefaultStoragePartition:
      return PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition;
    case PreloadingEligibility::kPrefetchProxyNotAvailable:
      return PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable;
    case PreloadingEligibility::kPreloadingDisabled:
      return PrefetchStatus::kPrefetchIneligiblePreloadingDisabled;
    case PreloadingEligibility::kRetryAfter:
      return PrefetchStatus::kPrefetchIneligibleRetryAfter;
    case PreloadingEligibility::kSameSiteCrossOriginPrefetchRequiredProxy:
      return PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy;
    case PreloadingEligibility::kSchemeIsNotHttps:
      return PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps;
    case PreloadingEligibility::kUserHasCookies:
      return PrefetchStatus::kPrefetchIneligibleUserHasCookies;
    case PreloadingEligibility::kUserHasServiceWorker:
      return PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker;
    case PreloadingEligibility::kUserHasServiceWorkerNoFetchHandler:
      return PrefetchStatus::
          kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler;
    case PreloadingEligibility::kRedirectFromServiceWorker:
      return PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker;
    case PreloadingEligibility::kRedirectToServiceWorker:
      return PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker;
    case PreloadingEligibility::kBlockedByConnectionAllowlist:
      return PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist;
    case PreloadingEligibility::kEligible:
    default:
      // Other ineligible cases are not used in `PrefetchService`.
      NOTREACHED();
  }
}

std::optional<PreloadingTriggeringOutcome> TriggeringOutcomeFromStatus(
    PrefetchStatus prefetch_status) {
  switch (prefetch_status) {
    case PrefetchStatus::kPrefetchNotFinishedInTime:
      return PreloadingTriggeringOutcome::kRunning;
    case PrefetchStatus::kPrefetchSuccessful:
      return PreloadingTriggeringOutcome::kReady;
    case PrefetchStatus::kPrefetchResponseUsed:
      return PreloadingTriggeringOutcome::kSuccess;
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler:
    case PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchIneligibleExistingProxy:
    case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
    case PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved:
    case PrefetchStatus::kPrefetchCancelledOnUserNavigation:
      return PreloadingTriggeringOutcome::kFailure;
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchNotStarted:
      return std::nullopt;
  }
  return std::nullopt;
}

// Returns true if SetPrefetchStatus(|status|) can be called after a prefetch
// has already been marked as failed. We ignore such status updates
// as they may end up overwriting the initial failure reason.
bool StatusUpdateIsPossibleAfterFailure(PrefetchStatus status) {
  switch (status) {
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
    case PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved:
    case PrefetchStatus::kPrefetchCancelledOnUserNavigation: {
      CHECK(TriggeringOutcomeFromStatus(status) ==
            PreloadingTriggeringOutcome::kFailure);
      return true;
    }
    case PrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchStatus::kPrefetchSuccessful:
    case PrefetchStatus::kPrefetchResponseUsed:
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler:
    case PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchIneligibleExistingProxy:
    case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
      return false;
  }
}

void RecordPrefetchProxyPrefetchMainframeNetError(int net_error) {
  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Mainframe.NetError",
                           std::abs(net_error));
}

void RecordPrefetchProxyPrefetchMainframeBodyLength(int64_t body_length) {
  UMA_HISTOGRAM_COUNTS_10M("PrefetchProxy.Prefetch.Mainframe.BodyLength",
                           body_length);
}

bool CalculateIsLikelyAheadOfPrerender(
    const PreloadPipelineInfoImpl& preload_pipeline_info) {
  if (!features::UsePrefetchPrerenderIntegration()) {
    return false;
  }

  switch (preload_pipeline_info.planned_max_preloading_type()) {
    case PreloadingType::kPrefetch:
      return false;
    case PreloadingType::kPrerender:
    case PreloadingType::kPrerenderUntilScript:
      return true;
    case PreloadingType::kUnspecified:
    case PreloadingType::kPreconnect:
    case PreloadingType::kNoStatePrefetch:
      NOTREACHED();
  }
}

PrefetchContainer::PrefetchResponseCompletedCallbackForTesting&
GetPrefetchResponseCompletedCallbackForTesting() {
  static base::NoDestructor<
      PrefetchContainer::PrefetchResponseCompletedCallbackForTesting>
      prefetch_response_completed_callback_for_testing;
  return *prefetch_response_completed_callback_for_testing;
}

void RecordPrefetchProxyPrefetchMainframeTotalTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::Time start = head->request_time;
  base::Time end = head->response_time;

  if (start.is_null() || end.is_null()) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES("PrefetchProxy.Prefetch.Mainframe.TotalTime",
                             end - start, base::Milliseconds(10),
                             base::Seconds(30), 100);
}

void RecordPrefetchProxyPrefetchMainframeConnectTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::TimeTicks start = head->load_timing.connect_timing.connect_start;
  base::TimeTicks end = head->load_timing.connect_timing.connect_end;

  if (start.is_null() || end.is_null()) {
    return;
  }

  UMA_HISTOGRAM_TIMES("PrefetchProxy.Prefetch.Mainframe.ConnectTime",
                      end - start);
}

void RecordPrefetchProxyPrefetchMainframeRespCode(int response_code) {
  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Mainframe.RespCode",
                           response_code);
}

// TODO(crbug.com/517847437): Consider migrating
// `network::ResourceRequest::TrustedParams::EqualsForTesting` to non-test.
// Currently this is used for `DUMP_WILL_BE_CHECK()` below (not strictly a test
// but test-ish).
bool OptionalTrustedParamsEqualsForTesting(  // IN-TEST
    const std::optional<network::ResourceRequest::TrustedParams>& lhs,
    const std::optional<network::ResourceRequest::TrustedParams>& rhs) {
  return (!lhs && !rhs) ||
         (lhs && rhs && lhs->EqualsForTesting(*rhs));  // IN-TEST
}

// Validates PrePrefetch `ResourceRequest`, to confirm that we correctly
// construct it. See
// https://docs.google.com/document/d/12cjL04kEjtLs5hSthgg8o_UK-LeS_RcF992Z2vCp7Vk/edit?usp=sharing
// for illustrating #A/#B+/#C.
//
// If any of the validation fails, we might want to:
// - Fix the usage of OMTPrefetch (i.e. WebView API caller side; not all WebView
//   prefetch parameters, APIs, and use cases are supported, as OMTPrefetch is
//   an experimental feature), or
// - Extend the supported usage of OMTPrefetch or fix bugs in OMTPrefetch
//   (i.e. fix the prefetch side).
enum class ValidateResourceRequestMode {
  // (#A): `resource_request_for_validation` is the `ResourceRequest` that would
  // be created for Prefetch, if the PrePrefetch wouldn't have been performed.
  //
  // (#C) and (#A) should be mostly the same, with some expected differences
  // (see comments and logic in the method body below):
  // - `devtools_request_id`
  // - `content::GetCorsExemptRequestedWithHeaderName()` header
  // - Timing differences, e.g. Client Hints values are changed between
  //   PrePrefetch and its consumption.
  //   (Note: not excluded from comparison)
  kOnRequestConstruction,

  // (#B+): `resource_request_for_validation` is the `ResourceRequest` after the
  // PrePrefetch `ResourceRequest` (#C) is promoted and goes through
  // `WillCreateURLLoaderFactory` etc.
  //
  // (#C) and (#B+) should be exactly the same.
  kAfterWillCreateURLLoaderFactory,
};

std::string_view GetHistogramName(ValidateResourceRequestMode mode) {
  switch (mode) {
    case ValidateResourceRequestMode::kOnRequestConstruction:
      return "OnRequestConstruction";
    case ValidateResourceRequestMode::kAfterWillCreateURLLoaderFactory:
      return "AfterWillCreateURLLoaderFactory";
  }
}

NOINLINE void ValidateResourceRequestForPrePrefetch(
    // (#C): `resource_request_for_pre_prefetch` is the `ResourceRequest`
    // created for PrePrefetch.
    const network::ResourceRequest& resource_request_for_pre_prefetch,
    const network::ResourceRequest& resource_request_for_validation,
    ValidateResourceRequestMode mode) {
  auto headers_mismatches = network::MatchHttpRequestHeaders(
      resource_request_for_pre_prefetch.headers,
      resource_request_for_validation.headers,
      network::MatchHttpRequestHeadersValueOption::kEquals);

  auto should_ignore_cors_exempt_header = [mode](
                                              const std::string& lowered_key) {
    switch (mode) {
      case ValidateResourceRequestMode::kOnRequestConstruction:
        if (base::EqualsCaseInsensitiveASCII(
                lowered_key, content::GetCorsExemptRequestedWithHeaderName())) {
          // `content::GetCorsExemptRequestedWithHeaderName()` can be added to
          // `resource_request_for_pre_prefetch` (see
          // `GetAwPrefetchHeadersOnNonUIThread()`), while it's not (yet) added
          // to `resource_request_for_validation` at this time, so ignore the
          // mismatch.
          return true;
        }
        return false;
      case ValidateResourceRequestMode::kAfterWillCreateURLLoaderFactory:
        // `content::GetCorsExemptRequestedWithHeaderName()` should be already
        // added (if needed) through `WillCreateURLLoaderFactory` interceptors
        // before reaching this point, so check the header to match.
        return false;
    }
  };
  auto cors_exempt_headers_mismatches = network::MatchHttpRequestHeaders(
      resource_request_for_pre_prefetch.cors_exempt_headers,
      resource_request_for_validation.cors_exempt_headers,
      network::MatchHttpRequestHeadersValueOption::kEquals,
      should_ignore_cors_exempt_header);

  constexpr std::string_view histogram_base_name =
      "Prefetch.PrePrefetchRequestValidation.";

  if (!headers_mismatches.empty() || !cors_exempt_headers_mismatches.empty()) {
    // Confirm that the header mismatch logic and excluded header list is
    // correct, i.e. the should-be-matching PrePrefetch scenarios in tests
    // passes the validation here. When we'll add tests for non-matching
    // PrePrefetch scenarios, we have to reconsider this.
    //
    // We don't crash production builds and we collect metrics instead, because
    // the mismatch rate might be too high to collect crash reports, even still
    // they are relatively rare.
    CHECK_IS_NOT_TEST();
  }

  base::UmaHistogramBoolean(
      base::StrCat({histogram_base_name, GetHistogramName(mode),
                    ".Headers.HasMismatch"}),
      !headers_mismatches.empty());
  for (const auto& mismatch : headers_mismatches) {
    network::LogLowerCaseRequestHeaderToUma(
        base::StrCat({histogram_base_name, GetHistogramName(mode),
                      ".Headers.Mismatched"}),
        mismatch.lowered_key);
  }

  base::UmaHistogramBoolean(
      base::StrCat({histogram_base_name, GetHistogramName(mode),
                    ".CorsExemptHeaders.HasMismatch"}),
      !cors_exempt_headers_mismatches.empty());
  for (const auto& mismatch : cors_exempt_headers_mismatches) {
    network::LogLowerCaseRequestHeaderToUma(
        base::StrCat({histogram_base_name, GetHistogramName(mode),
                      ".CorsExemptHeaders.Mismatched"}),
        mismatch.lowered_key);
  }

  // Migrated from `ResourceRequest::EqualsForTesting`, except for headers
  // and some other fields (commented below).
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.method,
                        resource_request_for_validation.method);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.url,
                        resource_request_for_validation.url);
  DUMP_WILL_BE_CHECK(
      resource_request_for_pre_prefetch.site_for_cookies.IsEquivalent(
          resource_request_for_validation.site_for_cookies));
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.update_first_party_url_on_redirect,
      resource_request_for_validation.update_first_party_url_on_redirect);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.request_initiator ==
                     resource_request_for_validation.request_initiator);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.isolated_world_origin ==
                     resource_request_for_validation.isolated_world_origin);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.referrer,
                        resource_request_for_validation.referrer);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.referrer_policy,
                        resource_request_for_validation.referrer_policy);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.load_flags,
                        resource_request_for_validation.load_flags);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.resource_type,
                        resource_request_for_validation.resource_type);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.priority,
                        resource_request_for_validation.priority);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.priority_incremental,
                        resource_request_for_validation.priority_incremental);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.devtools_stack_id ==
                     resource_request_for_validation.devtools_stack_id);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.cors_preflight_policy,
                        resource_request_for_validation.cors_preflight_policy);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.originated_from_service_worker,
      resource_request_for_validation.originated_from_service_worker);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.skip_service_worker,
                        resource_request_for_validation.skip_service_worker);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.mode,
                        resource_request_for_validation.mode);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.required_ip_address_space,
      resource_request_for_validation.required_ip_address_space);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.credentials_mode,
                        resource_request_for_validation.credentials_mode);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.redirect_mode,
                        resource_request_for_validation.redirect_mode);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.fetch_integrity,
                        resource_request_for_validation.fetch_integrity);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.expected_public_keys ==
                     resource_request_for_validation.expected_public_keys);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.destination,
                        resource_request_for_validation.destination);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.request_body,
                        resource_request_for_validation.request_body);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.keepalive,
                        resource_request_for_validation.keepalive);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.shared_storage_writable_eligible,
      resource_request_for_validation.shared_storage_writable_eligible);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.has_user_gesture,
                        resource_request_for_validation.has_user_gesture);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.enable_load_timing,
                        resource_request_for_validation.enable_load_timing);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.enable_upload_progress,
      resource_request_for_validation.enable_upload_progress);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.do_not_prompt_for_login,
      resource_request_for_validation.do_not_prompt_for_login);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.is_outermost_main_frame,
      resource_request_for_validation.is_outermost_main_frame);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.transition_type,
                        resource_request_for_validation.transition_type);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.is_reload_navigation,
                        resource_request_for_validation.is_reload_navigation);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.previews_state,
                        resource_request_for_validation.previews_state);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.upgrade_if_insecure,
                        resource_request_for_validation.upgrade_if_insecure);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.is_revalidating,
                        resource_request_for_validation.is_revalidating);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.throttling_profile_id ==
                     resource_request_for_validation.throttling_profile_id);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.fetch_window_id ==
                     resource_request_for_validation.fetch_window_id);
  if (mode != ValidateResourceRequestMode::kOnRequestConstruction) {
    // For `kOnRequestConstruction`, `resource_request_for_validation` has its
    // fresh random `devtools_request_id` token.
    DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.devtools_request_id ==
                       resource_request_for_validation.devtools_request_id);
  }
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.is_fetch_like_api,
                        resource_request_for_validation.is_fetch_like_api);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.is_fetch_later_api,
                        resource_request_for_validation.is_fetch_later_api);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.is_favicon,
                        resource_request_for_validation.is_favicon);
  DUMP_WILL_BE_CHECK(
      resource_request_for_pre_prefetch.recursive_prefetch_token ==
      resource_request_for_validation.recursive_prefetch_token);
  DUMP_WILL_BE_CHECK(OptionalTrustedParamsEqualsForTesting(  // IN-TEST
      resource_request_for_pre_prefetch.trusted_params,
      resource_request_for_validation.trusted_params));
  DUMP_WILL_BE_CHECK(
      resource_request_for_pre_prefetch.devtools_accepted_stream_types ==
      resource_request_for_validation.devtools_accepted_stream_types);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.trust_token_params ==
                     resource_request_for_validation.trust_token_params);
  // `web_bundle_token_params` isn't set for prefetch.
  DUMP_WILL_BE_CHECK(
      !resource_request_for_pre_prefetch.web_bundle_token_params);
  DUMP_WILL_BE_CHECK(!resource_request_for_validation.web_bundle_token_params);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.net_log_create_info ==
                     resource_request_for_validation.net_log_create_info);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.net_log_reference_info ==
                     resource_request_for_validation.net_log_reference_info);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.shared_dictionary_writer_enabled,
      resource_request_for_validation.shared_dictionary_writer_enabled);
  DUMP_WILL_BE_CHECK_EQ(resource_request_for_pre_prefetch.socket_tag,
                        resource_request_for_validation.socket_tag);
  DUMP_WILL_BE_CHECK_EQ(
      resource_request_for_pre_prefetch.allows_device_bound_sessions,
      resource_request_for_validation.allows_device_bound_sessions);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.permissions_policy ==
                     resource_request_for_validation.permissions_policy);
  DUMP_WILL_BE_CHECK(resource_request_for_pre_prefetch.fetch_retry_options ==
                     resource_request_for_validation.fetch_retry_options);
}

}  // namespace

// static
std::unique_ptr<PrefetchContainer> PrefetchContainer::Create(
    base::PassKey<PrefetchService>,
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    std::unique_ptr<PrePrefetchContainer> pre_prefetch_container) {
  return std::make_unique<PrefetchContainer>(base::PassKey<PrefetchContainer>(),
                                             std::move(prefetch_request),
                                             std::move(pre_prefetch_container));
}

// static
std::unique_ptr<PrefetchContainer> PrefetchContainer::CreateForTesting(
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    std::unique_ptr<PrePrefetchContainer> pre_prefetch_container) {
  return std::make_unique<PrefetchContainer>(base::PassKey<PrefetchContainer>(),
                                             std::move(prefetch_request),
                                             std::move(pre_prefetch_container));
}

PrefetchContainer::PrefetchContainer(
    base::PassKey<PrefetchContainer>,
    std::unique_ptr<const PrefetchRequest> prefetch_request,
    std::unique_ptr<PrePrefetchContainer> pre_prefetch_container)
    : request_(std::move(prefetch_request)),
      is_constructed_from_pre_prefetch_(pre_prefetch_container != nullptr),
      container_id_for_testing_(base::UnguessableToken::Create().ToString()) {
  CHECK(request_);
  TRACE_EVENT_BEGIN("loading", "PrefetchContainer::LoadState::kNotStarted",
                    request().preload_pipeline_info().GetTrack());

  // `PrefetchContainer` is always added to `PrefetchService` upon construction
  // in non-test code.
  prefetch_container_metrics_.time_added_to_prefetch_service =
      base::TimeTicks::Now();
  prefetch_container_metrics_.is_constructed_from_pre_prefetch =
      is_constructed_from_pre_prefetch_;

  if (pre_prefetch_container) {
    CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
    pre_prefetch_loader_ = pre_prefetch_container->TakePendingURLLoaderOnUI();
    pre_prefetch_loader_client_receiver_ =
        pre_prefetch_container->TakePendingURLLoaderClientReceiverOnUI();

    resource_request_for_pre_prefetch_ =
        pre_prefetch_container->TakeResourceRequestOnUI();
  }

  is_likely_ahead_of_prerender_ =
      CalculateIsLikelyAheadOfPrerender(request().preload_pipeline_info());

  AddRedirectHop(GetURL());

  // Disallow prefetching ServiceWorker-controlled responses for isolated
  // network contexts.
  if (!features::IsPrefetchServiceWorkerEnabled(request().browser_context()) ||
      request().IsIsolatedNetworkContextRequired(GetURL())) {
    service_worker_state_ = PrefetchServiceWorkerState::kDisallowed;
  }

  assert_observer_ = std::make_unique<AssertPrefetchContainerObserver>(*this);

  if (auto* browser_initiator_info = request().GetBrowserInitiatorInfo()) {
    if (auto* request_status_listener_observer =
            browser_initiator_info->request_status_listener_observer()) {
      AddObserver(request_status_listener_observer);
    }
  }
}

PrefetchContainer::~PrefetchContainer() {
  DVLOG(1) << *this << "::dtor";

  // `PrefetchContainer` destruction is disallowed during observer notification.
  DUMP_WILL_BE_CHECK(!during_observer_notification_);

  is_in_dtor_ = true;

  // Ideally, this method should be called just before dtor.
  // https://chromium-review.googlesource.com/c/chromium/src/+/5657659/comments/0cfb14c0_3050963e
  //
  // TODO(crbug.com/356314759): Do it.
  NotifyObservers(&PrefetchContainerObserver::OnWillBeDestroyed);

  CancelStreamingURLLoaderIfNotServing();

  MaybeRecordPrefetchStatusToUMA(
      prefetch_status_.value_or(PrefetchStatus::kPrefetchNotStarted));
  RecordPrefetchDurationHistogram();
  RecordPrefetchMatchMissedToPrefetchStartedHistogram();
  RecordPrefetchContainerServedCountHistogram();

  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    ukm_source_id = renderer_initiator_info->ukm_source_id();
  }
  ukm::builders::PrefetchProxy_PrefetchedResource builder(ukm_source_id);
  builder.SetResourceType(/*mainframe*/ 1);
  builder.SetStatus(static_cast<int>(
      prefetch_status_.value_or(PrefetchStatus::kPrefetchNotStarted)));
  builder.SetLinkClicked(served_count_ > 0);

  if (GetNonRedirectResponseReader()) {
    GetNonRedirectResponseReader()->RecordOnPrefetchContainerDestroyed(
        base::PassKey<PrefetchContainer>(), builder);
  }

  if (probe_result_) {
    builder.SetISPFilteringStatus(static_cast<int>(probe_result_.value()));
  }

  // TODO(crbug.com/40215782): Get the navigation start time and set the
  // NavigationStartToFetchStartMs field of the PrefetchProxy.PrefetchedResource
  // UKM event.

  builder.Record(ukm::UkmRecorder::Get());

  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    if (renderer_initiator_info->prefetch_document_manager()) {
      renderer_initiator_info->prefetch_document_manager()->OnWillBeDestroyed(
          *this);
    }
  }

  TRACE_EVENT_END("loading", request_->preload_pipeline_info().GetTrack());

  if (auto* browser_initiator_info = request().GetBrowserInitiatorInfo()) {
    if (auto* request_status_listener_observer =
            browser_initiator_info->request_status_listener_observer()) {
      RemoveObserver(request_status_listener_observer);
    }
  }

  // Destroy `assert_observer_` before `WeakPtr`s are invalidated to allow it
  // call `RemoveObserver()`.
  assert_observer_.reset();
}

PrefetchServingHandle PrefetchContainer::CreateServingHandle() {
  return PrefetchServingHandle(GetWeakPtr(), 0);
}

std::unique_ptr<const PrefetchServingHandle>
PrefetchContainer::CreateConstServingHandle() const {
  // `GetMutableWeakPtr()` (which is a kind of a const-to-non-const cast) is
  // used here but the effect of the const cast is minimized by returning
  // `std::unique_ptr<const PrefetchServingHandle>`, as
  // `const PrefetchServingHandle` doesn't use its
  // non-const `PrefetchContainer` reference.
  return std::make_unique<const PrefetchServingHandle>(GetMutableWeakPtr(), 0);
}

const std::vector<std::unique_ptr<PrefetchSingleRedirectHop>>&
PrefetchContainer::redirect_chain(base::PassKey<PrefetchServingHandle>) const {
  return redirect_chain_;
}

void PrefetchContainer::SetProbeResult(base::PassKey<PrefetchServingHandle>,
                                       PrefetchProbeResult probe_result) {
  probe_result_ = probe_result;
}

std::optional<PreloadingTriggeringOutcome>
PrefetchContainer::TriggeringOutcomeFromStatusForServingHandle(
    base::PassKey<PrefetchServingHandle>,
    PrefetchStatus prefetch_status) {
  return TriggeringOutcomeFromStatus(prefetch_status);
}

// Please follow go/preloading-dashboard-updates if a new outcome enum or a
// failure reason enum is added.
void PrefetchContainer::SetTriggeringOutcomeAndFailureReasonFromStatus(
    PrefetchStatus new_prefetch_status) {
  std::optional<PrefetchStatus> old_prefetch_status = prefetch_status_;
  if (old_prefetch_status &&
      old_prefetch_status.value() == PrefetchStatus::kPrefetchResponseUsed) {
    // Skip this update if the triggering outcome has already been updated
    // to kSuccess.
    return;
  }

  if (old_prefetch_status &&
      (TriggeringOutcomeFromStatus(old_prefetch_status.value()) ==
       PreloadingTriggeringOutcome::kFailure)) {
    if (StatusUpdateIsPossibleAfterFailure(new_prefetch_status)) {
      // Note that `StatusUpdateIsPossibleAfterFailure()` implies that
      // the new status is a failure.
      CHECK(TriggeringOutcomeFromStatus(new_prefetch_status) ==
            PreloadingTriggeringOutcome::kFailure);

      // Skip this update since if the triggering outcome has already been
      // updated to kFailure, we don't need to overwrite it.
      return;
    } else {
      SCOPED_CRASH_KEY_NUMBER("PrefetchContainer", "prefetch_status_from",
                              static_cast<int>(old_prefetch_status.value()));
      SCOPED_CRASH_KEY_NUMBER("PrefetchContainer", "prefetch_status_to",
                              static_cast<int>(new_prefetch_status));
      NOTREACHED() << "PrefetchStatus illegal transition: "
                      "(old_prefetch_status, new_prefetch_status) = ("
                   << static_cast<int>(old_prefetch_status.value()) << ", "
                   << static_cast<int>(new_prefetch_status) << ")";
    }
  }

  // We record the prefetch status to UMA if it's a failure, or if the prefetch
  // response is being used. For other statuses, there may be more updates in
  // the future, so we only record them in the destructor.
  // Note: The prefetch may have an updated failure status in the future
  // (for example: if the triggering speculation candidate for a failed prefetch
  // is removed), but the original failure is more pertinent for metrics
  // purposes.
  if (TriggeringOutcomeFromStatus(new_prefetch_status) ==
          PreloadingTriggeringOutcome::kFailure ||
      new_prefetch_status == PrefetchStatus::kPrefetchResponseUsed) {
    MaybeRecordPrefetchStatusToUMA(new_prefetch_status);
  }

  if (request().attempt()) {
    switch (new_prefetch_status) {
      case PrefetchStatus::kPrefetchNotFinishedInTime:
        request().attempt()->SetTriggeringOutcome(
            PreloadingTriggeringOutcome::kRunning);
        break;
      case PrefetchStatus::kPrefetchSuccessful:
        // A successful prefetch means the response is ready to be used for the
        // next navigation.
        request().attempt()->SetTriggeringOutcome(
            PreloadingTriggeringOutcome::kReady);
        break;
      case PrefetchStatus::kPrefetchResponseUsed:
        if (old_prefetch_status && old_prefetch_status.value() !=
                                       PrefetchStatus::kPrefetchSuccessful) {
          // If the new prefetch status is |kPrefetchResponseUsed| or
          // |kPrefetchUsedNoProbe| but the previous status is not
          // |kPrefetchSuccessful|, then temporarily update the triggering
          // outcome to |kReady| to ensure valid triggering outcome state
          // transitions. This can occur in cases where the prefetch is served
          // before the body is fully received.
          request().attempt()->SetTriggeringOutcome(
              PreloadingTriggeringOutcome::kReady);
        }
        request().attempt()->SetTriggeringOutcome(
            PreloadingTriggeringOutcome::kSuccess);
        break;
      // A decoy is considered eligible because a network request is made for
      // it. It is considered as a failure as the final response is never
      // served.
      case PrefetchStatus::kPrefetchIsPrivacyDecoy:
      case PrefetchStatus::kPrefetchFailedNetError:
      case PrefetchStatus::kPrefetchFailedNon2XX:
      case PrefetchStatus::kPrefetchFailedMIMENotSupported:
      case PrefetchStatus::kPrefetchFailedInvalidRedirect:
      case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
      case PrefetchStatus::kPrefetchNotUsedProbeFailed:
      case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
      // TODO(adithyas): This would report 'eviction' as a failure even though
      // the initial prefetch succeeded, consider introducing a different
      // PreloadingTriggerOutcome for eviction.
      case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
      case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
      case PrefetchStatus::kPrefetchIsStale:
      case PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved:
      case PrefetchStatus::kPrefetchCancelledOnUserNavigation:
        request().attempt()->SetFailureReason(
            ToPreloadingFailureReason(new_prefetch_status));
        break;
      case PrefetchStatus::kPrefetchHeldback:
      case PrefetchStatus::kPrefetchNotStarted:
        // `kPrefetchNotStarted` is set in
        // `PrefetchService::OnGotEligibilityForNonRedirect()` when the
        // container is pushed onto the prefetch queue, which occurs before the
        // holdback status is determined in
        // `PrefetchService::StartSinglePrefetch`. After the container is queued
        // and before it is sent for prefetch, the only status change is when
        // the container is popped from the queue but heldback. This is covered
        // by attempt's holdback status. For these two reasons this
        // PrefetchStatus does not fire a `SetTriggeringOutcome`.
        break;
      case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
      case PrefetchStatus::
          kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler:
      case PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker:
      case PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker:
      case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
      case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
      case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
      case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
      case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
      case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
      case PrefetchStatus::kPrefetchIneligibleExistingProxy:
      case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
      case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
      case PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
      case PrefetchStatus::kPrefetchIneligibleBlockedByConnectionAllowlist:
        NOTIMPLEMENTED();
    }
  }
}

void PrefetchContainer::SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
    PrefetchStatus prefetch_status) {
  prefetch_status_ = prefetch_status;
  request().preload_pipeline_info().SetPrefetchStatus(prefetch_status);
  for (auto& preload_pipeline_info : inherited_preload_pipeline_infos_) {
    preload_pipeline_info->SetPrefetchStatus(prefetch_status);
  }

  // Currently DevTools only supports when the prefetch is initiated by
  // renderer.
  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    std::optional<PreloadingTriggeringOutcome> preloading_trigger_outcome =
        TriggeringOutcomeFromStatus(prefetch_status);
    if (renderer_initiator_info->devtools_navigation_token().has_value() &&
        preloading_trigger_outcome.has_value()) {
      devtools_instrumentation::DidUpdatePrefetchStatus(
          FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost()),
          renderer_initiator_info->devtools_navigation_token().value(),
          GetURL(), request().preload_pipeline_info().id(),
          preloading_trigger_outcome.value(), prefetch_status,
          GetDevtoolsRequestId());
    }
  }
}

void PrefetchContainer::SetPrefetchMatchMissedTimeForMetrics(
    base::TimeTicks time) {
  CHECK(!time_prefetch_match_missed_);
  time_prefetch_match_missed_ = time;
}

void PrefetchContainer::SetPrefetchStatus(PrefetchStatus prefetch_status) {
  // The concept of `PreloadingAttempt`'s `PreloadingTriggeringOutcome` is to
  // record the outcomes of started triggers. Therefore, this should
  // only be called once prefetching has actually started, and not for
  // ineligible or eligibled but not started triggers (e.g., holdback triggers,
  // triggers waiting on a queue).
  switch (GetLoadState()) {
    case LoadState::kStarted:
    case LoadState::kDeterminedHead:
    case LoadState::kFailedDeterminedHead:
    case LoadState::kCompleted:
    case LoadState::kFailed:
      SetTriggeringOutcomeAndFailureReasonFromStatus(prefetch_status);
      break;
    case LoadState::kNotStarted:
    case LoadState::kEligible:
    case LoadState::kFailedIneligible:
    case LoadState::kFailedHeldback:
      break;
  }
  SetPrefetchStatusWithoutUpdatingTriggeringOutcome(prefetch_status);
}

PrefetchStatus PrefetchContainer::GetPrefetchStatus() const {
  DCHECK(prefetch_status_);
  return prefetch_status_.value();
}

PrefetchIsolatedNetworkContext* PrefetchContainer::CreateIsolatedNetworkContext(
    mojo::Remote<network::mojom::NetworkContext> isolated_network_context) {
  CHECK(!isolated_network_context_);
  isolated_network_context_ = std::make_unique<PrefetchIsolatedNetworkContext>(
      std::move(isolated_network_context), request());
  return isolated_network_context_.get();
}

PrefetchIsolatedNetworkContext* PrefetchContainer::GetIsolatedNetworkContext()
    const {
  return isolated_network_context_.get();
}

bool PrefetchContainer::IsConstructedFromPrePrefetch() const {
  return is_constructed_from_pre_prefetch_;
}

bool PrefetchContainer::ExistsValidPrePrefetch() const {
  return pre_prefetch_loader_ && pre_prefetch_loader_client_receiver_;
}

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchContainer::CreatePrePrefetchURLLoaderFactory() {
  CHECK(ExistsValidPrePrefetch());
  // PrePrefetch URLLoader Factory should be used only for the initial request.
  // Please see also the comment at
  // `PrefetchService::GetURLLoaderFactoryForCurrentPrefetch()`.
  CHECK_EQ(redirect_chain_.size(), 1u);
  scoped_refptr<network::SharedURLLoaderFactory>
      pre_prefetch_url_loader_factory = base::MakeRefCounted<
          network::SingleRequestURLLoaderFactory>(base::BindOnce(
          [](mojo::PendingRemote<network::mojom::URLLoader> pre_prefetch_loader,
             mojo::PendingReceiver<network::mojom::URLLoaderClient>
                 pre_prefetch_client_receiver,
             base::WeakPtr<PrefetchContainer> prefetch_container,
             const network::ResourceRequest& resource_request,
             mojo::PendingReceiver<network::mojom::URLLoader> receiver,
             mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
            mojo::FusePipes(std::move(receiver),
                            std::move(pre_prefetch_loader));
            mojo::FusePipes(std::move(pre_prefetch_client_receiver),
                            std::move(client));
            if (prefetch_container) {
              // We can serve navigation even after its `PrefetchContainer` is
              // gone. In such cases, skip the validation here, as it should be
              // rare and we don't expect the validation can fail specifically
              // for such cases.
              CHECK(prefetch_container->GetResourceRequest());
              ValidateResourceRequestForPrePrefetch(
                  resource_request, *prefetch_container->GetResourceRequest(),
                  ValidateResourceRequestMode::
                      kAfterWillCreateURLLoaderFactory);
            }
          },
          std::move(pre_prefetch_loader_),
          std::move(pre_prefetch_loader_client_receiver_), GetWeakPtr()));

  // Currently `feature::kPrefetchOffTheMainThread` doesn't support the
  // request w/ isolated context.
  return CreatePrefetchURLLoaderFactory(
      request()
          .browser_context()
          ->GetDefaultStoragePartition()
          ->GetNetworkContext(),
      request(), std::move(pre_prefetch_url_loader_factory));
}

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchContainer::GetOrCreateDefaultNetworkContextURLLoaderFactory() {
  CHECK(!IsIsolatedNetworkContextRequiredForCurrentPrefetch());
  if (!default_network_context_url_loader_factory_) {
    // The corresponding `CreatePrefetchURLLoaderFactory()` call is inside
    // `PrefetchIsolatedNetworkContext`.
    default_network_context_url_loader_factory_ =
        CreatePrefetchURLLoaderFactory(request()
                                           .browser_context()
                                           ->GetDefaultStoragePartition()
                                           ->GetNetworkContext(),
                                       request());
  }
  CHECK(default_network_context_url_loader_factory_);
  return default_network_context_url_loader_factory_;
}

void PrefetchContainer::CloseIdleConnections() {
  if (isolated_network_context_) {
    isolated_network_context_->CloseIdleConnections();
  }
}

void PrefetchContainer::SetLoadState(LoadState new_load_state) {
  TRACE_EVENT_END("loading", request_->preload_pipeline_info().GetTrack());

  CHECK(!is_in_dtor_);

  // `LoadState` transitions are disallowed during observer notification.
  DUMP_WILL_BE_CHECK(!during_observer_notification_);

  {
    using T = PrefetchContainerLoadState;
    static const base::NoDestructor<base::StateTransitions<T>> transitions(
        base::StateTransitions<T>({
            {T::kNotStarted, {T::kEligible, T::kFailedIneligible}},
            {T::kEligible, {T::kStarted, T::kFailedHeldback}},
            {T::kFailedIneligible, {}},
            {T::kFailedHeldback, {}},
            {T::kStarted, {T::kDeterminedHead, T::kFailedDeterminedHead}},
            {T::kDeterminedHead, {T::kCompleted, T::kFailed}},
            {T::kFailedDeterminedHead, {T::kFailed}},
            {T::kCompleted, {}},
            {T::kFailed, {}},
        }));
    CHECK_STATE_TRANSITION(transitions, load_state_, new_load_state);
  }

  // Tracing and debugging
  switch (new_load_state) {
    case LoadState::kNotStarted:
      NOTREACHED();
    case LoadState::kEligible:
      TRACE_EVENT_BEGIN("loading", "PrefetchContainer::LoadState::kEligible",
                        request_->preload_pipeline_info().GetTrack());
      break;
    case LoadState::kFailedIneligible:
      TRACE_EVENT_BEGIN("loading",
                        "PrefetchContainer::LoadState::kFailedIneligible",
                        request_->preload_pipeline_info().GetTrack());
      break;
    case LoadState::kStarted:
      TRACE_EVENT_BEGIN("loading", "PrefetchContainer::LoadState::kStarted",
                        request_->preload_pipeline_info().GetTrack());
      break;
    case LoadState::kFailedHeldback:
      TRACE_EVENT_BEGIN("loading",
                        "PrefetchContainer::LoadState::kFailedHeldback",
                        request_->preload_pipeline_info().GetTrack());
      break;
    case LoadState::kDeterminedHead:
      TRACE_EVENT_BEGIN("loading",
                        "PrefetchContainer::LoadState::kDeterminedHead",
                        request_->preload_pipeline_info().GetTrack());
      break;
    case LoadState::kFailedDeterminedHead:
      TRACE_EVENT_BEGIN("loading",
                        "PrefetchContainer::LoadState::kFailedDeterminedHead",
                        request_->preload_pipeline_info().GetTrack());
      break;
    case LoadState::kCompleted:
      TRACE_EVENT_BEGIN("loading", "PrefetchContainer::LoadState::kCompleted",
                        request_->preload_pipeline_info().GetTrack());
      break;
    case LoadState::kFailed:
      TRACE_EVENT_BEGIN("loading", "PrefetchContainer::LoadState::kFailed",
                        request_->preload_pipeline_info().GetTrack());
      break;
  }
  DVLOG(1) << (*this) << " LoadState " << load_state_ << " -> "
           << new_load_state;

  load_state_ = new_load_state;
}

PrefetchContainer::LoadState PrefetchContainer::GetLoadState() const {
  return load_state_;
}

void PrefetchContainer::OnEligibilityCheckComplete(
    PreloadingEligibility eligibility) {
  TRACE_EVENT("loading", "PrefetchContainer::OnEligibilityCheckComplete",
              request_->preload_pipeline_info().GetFlow());

  if (IsDecoy()) {
    eligibility = PreloadingEligibility::kEligible;
  }

  request().preload_pipeline_info().SetPrefetchEligibility(eligibility);
  for (auto& preload_pipeline_info : inherited_preload_pipeline_infos_) {
    preload_pipeline_info->SetPrefetchEligibility(eligibility);
  }

  bool is_eligible = eligibility == PreloadingEligibility::kEligible;

  if (redirect_chain_.size() == 1) {
    // This case is for just the URL that was originally requested to be
    // prefetched.

    CHECK(!initial_eligibility_);
    initial_eligibility_ = eligibility;

    if (is_eligible) {
      SetLoadState(LoadState::kEligible);
      if (!IsDecoy()) {
        SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);
      }
    } else {
      SetLoadState(LoadState::kFailedIneligible);
      PrefetchStatus new_prefetch_status =
          PrefetchStatusFromIneligibleReason(eligibility);
      MaybeRecordPrefetchStatusToUMA(new_prefetch_status);
      SetPrefetchStatusWithoutUpdatingTriggeringOutcome(new_prefetch_status);
    }

    if (request().attempt()) {
      // Please follow go/preloading-dashboard-updates if a new eligibility is
      // added.
      request().attempt()->SetEligibility(eligibility);
    }

    prefetch_container_metrics_.time_initial_eligibility_got =
        base::TimeTicks::Now();

    if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
      if (renderer_initiator_info->prefetch_document_manager()) {
        renderer_initiator_info->prefetch_document_manager()
            ->OnGotInitialEligibility(*this);
      }
    }

    NotifyObservers(&PrefetchContainerObserver::OnGotInitialEligibility);
  } else {
    // This case is for any URLs from redirects.
    if (!is_eligible) {
      SetPrefetchStatus(PrefetchStatus::kPrefetchFailedIneligibleRedirect);
    }
  }

  if (is_eligible && !IsDecoy()) {
    GetCurrentSingleRedirectHopToPrefetch().RegisterCookieListener();
  }
}

void PrefetchContainer::UpdateResourceRequest(
    const net::RedirectInfo& redirect_info,
    const network::HttpRequestHeadersUpdateParams& headers_update_params) {
  CHECK(resource_request_);

  // TODO(jbroman): We have several places that invoke
  // `net::RedirectUtil::UpdateHttpRequest` and then need to do very similar
  // work afterward. Ideally we would deduplicate these more.
  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_->url, resource_request_->method, redirect_info,
      headers_update_params.removed_headers,
      headers_update_params.modified_headers, &resource_request_->headers,
      &should_clear_upload);
  CHECK(!should_clear_upload);

  for (const std::string& name : headers_update_params.removed_headers) {
    resource_request_->cors_exempt_headers.RemoveHeader(name);
  }
  resource_request_->cors_exempt_headers.MergeFrom(
      headers_update_params.modified_cors_exempt_headers);

  resource_request_->UpdateOnRedirect(redirect_info);
  UpdateVariationsHeaderForPrefetch(*resource_request_, request());
}

void PrefetchContainer::AddRedirectHop(const GURL& url) {
  redirect_chain_.push_back(std::make_unique<PrefetchSingleRedirectHop>(
      *this, url, request().preload_pipeline_info().GetFlow()));
}

void PrefetchContainer::MarkCrossSiteContaminated() {
  is_cross_site_contaminated_ = true;
}

void PrefetchContainer::PauseAllCookieListeners() {
  // TODO(crbug.com/377440445): Consider whether we actually need to
  // pause/resume all single prefetch's cookie listener during each single
  // prefetch's isolated cookie copy.
  for (const auto& single_redirect_hop : redirect_chain_) {
    single_redirect_hop->PauseCookieListener();
  }
}

void PrefetchContainer::ResumeAllCookieListeners() {
  // TODO(crbug.com/377440445): Consider whether we actually need to
  // pause/resume all single prefetch's cookie listener during each single
  // prefetch's isolated cookie copy.
  for (const auto& single_redirect_hop : redirect_chain_) {
    single_redirect_hop->ResumeCookieListener();
  }
}

void PrefetchContainer::SetStreamingURLLoader(
    base::WeakPtr<PrefetchStreamingURLLoader> streaming_loader) {
  // The previous streaming loader (if any) should be already deleted or to be
  // deleted soon when the new `streaming_loader` is set here.
  CHECK(!streaming_loader_ || streaming_loader_->IsDeletionScheduledForCHECK());

  streaming_loader_ = std::move(streaming_loader);
}

base::WeakPtr<PrefetchStreamingURLLoader>
PrefetchContainer::GetStreamingURLLoader() const {
  // Streaming loaders already deleted or scheduled to be deleted shouldn't be
  // used.
  if (!streaming_loader_ || streaming_loader_->IsDeletionScheduledForCHECK()) {
    return nullptr;
  }
  return streaming_loader_;
}

bool PrefetchContainer::IsStreamingURLLoaderDeletionScheduledForTesting()
    const {
  return streaming_loader_ && streaming_loader_->IsDeletionScheduledForCHECK();
}

const PrefetchResponseReader* PrefetchContainer::GetNonRedirectResponseReader()
    const {
  CHECK(!redirect_chain_.empty());
  const PrefetchResponseReader* response_reader = nullptr;
  if (redirect_chain_.back()->response_reader().GetHead()) {
    response_reader = &redirect_chain_.back()->response_reader();
  }

  switch (GetLoadState()) {
    case LoadState::kNotStarted:
    case LoadState::kEligible:
    case LoadState::kFailedIneligible:
    case LoadState::kFailedHeldback:
    case LoadState::kStarted:
      // Either the last `PrefetchResponseReader` is for a redirect response, or
      // for a final response not yet receiving its header.
      CHECK(!response_reader);
      break;

    case LoadState::kDeterminedHead:
    case LoadState::kCompleted:
      // The final `PrefetchResponseReader` has received its response
      // successfully.
      CHECK(response_reader);
      break;

    case LoadState::kFailedDeterminedHead:
    case LoadState::kFailed:
      // `response_reader` can be null here when the prefetch has failed without
      // receiving any response head, including on failed redirects.
      break;
  }

  return response_reader;
}

const network::mojom::URLResponseHead* PrefetchContainer::GetNonRedirectHead()
    const {
  return GetNonRedirectResponseReader()
             ? GetNonRedirectResponseReader()->GetHead().get()
             : nullptr;
}

std::optional<int> PrefetchContainer::GetResponseCode() const {
  std::optional<int> response_code;
  const network::mojom::URLResponseHead* head = GetNonRedirectHead();
  if (head && head->headers) {
    response_code = head->headers->response_code();
  }

  switch (GetLoadState()) {
    case LoadState::kNotStarted:
    case LoadState::kEligible:
    case LoadState::kFailedIneligible:
    case LoadState::kFailedHeldback:
    case LoadState::kStarted:
      CHECK(!response_code);
      break;
    case LoadState::kDeterminedHead:
    case LoadState::kCompleted:
      CHECK(response_code);
      break;
    case LoadState::kFailedDeterminedHead:
    case LoadState::kFailed:
      // `response_code` can be non-null (see the comment in the header) or null
      // here.
      break;
  }

  return response_code;
}

const std::optional<network::URLLoaderCompletionStatus>&
PrefetchContainer::GetCompletionStatus() const {
  switch (GetLoadState()) {
    case LoadState::kNotStarted:
    case LoadState::kEligible:
    case LoadState::kFailedIneligible:
    case LoadState::kFailedHeldback:
    case LoadState::kStarted:
    case LoadState::kDeterminedHead:
    case LoadState::kFailedDeterminedHead:
      CHECK(!completion_status_);
      break;
    case LoadState::kCompleted:
    case LoadState::kFailed:
      CHECK(completion_status_);
      break;
  }

  return completion_status_;
}

void PrefetchContainer::CancelStreamingURLLoaderIfNotServing() {
  if (!streaming_loader_) {
    return;
  }

  // Prefetch cancellation is disallowed during observer notification.
  DUMP_WILL_BE_CHECK(!during_observer_notification_);

  streaming_loader_->CancelIfNotServing();
  streaming_loader_.reset();
}

// static
std::optional<PrefetchErrorOnResponseReceived>
PrefetchContainer::OnPrefetchResponseStarted(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    network::mojom::URLResponseHead* head) {
  if (!prefetch_container) {
    // `kPrefetchWasDecoy` is used to keep the existing behavior.
    return PrefetchErrorOnResponseReceived::kPrefetchWasDecoy;
  }
  return prefetch_container->OnPrefetchResponseStartedInternal(head);
}

std::optional<PrefetchErrorOnResponseReceived>
PrefetchContainer::OnPrefetchResponseStartedInternal(
    network::mojom::URLResponseHead* head) {
  TRACE_EVENT("loading", "PrefetchContainer::OnPrefetchResponseStartedInternal",
              "prefetch_url", GetURL().spec());
  if (IsDecoy()) {
    return PrefetchErrorOnResponseReceived::kPrefetchWasDecoy;
  }

  if (IsCrossSiteContaminated()) {
    head->is_prefetch_with_cross_site_contamination = true;
  }

  NotifyPrefetchResponseReceived(*head);

  if (!head->headers) {
    return PrefetchErrorOnResponseReceived::kFailedInvalidHeaders;
  }

  if (base::FeatureList::IsEnabled(features::kPrefetchActivationBeacon) &&
      head->parsed_headers &&
      head->parsed_headers->prefetch_activation_beacon_endpoint.has_value()) {
    const GURL& endpoint =
        *head->parsed_headers->prefetch_activation_beacon_endpoint;
    // TODO(crbug.com/499814382): Report to DevTools console when the endpoint
    // specified by the header is not in the same origin for debuggability.
    if (endpoint.is_valid() && url::Origin::Create(endpoint).IsSameOriginWith(
                                   url::Origin::Create(GetCurrentURL()))) {
      activation_beacon_url_ = endpoint;
    }
  }

  RecordPrefetchProxyPrefetchMainframeTotalTime(head);
  RecordPrefetchProxyPrefetchMainframeConnectTime(head);

  int response_code = head->headers->response_code();
  RecordPrefetchProxyPrefetchMainframeRespCode(response_code);
  if (response_code < 200 || response_code >= 300) {
    SetPrefetchStatus(PrefetchStatus::kPrefetchFailedNon2XX);
    return PrefetchErrorOnResponseReceived::kFailedNon2XX;
  }

  if (PrefetchServiceHTMLOnly() && head->mime_type != "text/html") {
    SetPrefetchStatus(PrefetchStatus::kPrefetchFailedMIMENotSupported);
    return PrefetchErrorOnResponseReceived::kFailedMIMENotSupported;
  }
  return std::nullopt;
}

void PrefetchContainer::OnDeterminedHead(bool is_successful_determined_head) {
  TRACE_EVENT("loading", "PrefetchContainer::OnDeterminedHead",
              request_->preload_pipeline_info().GetFlow());

  if (is_in_dtor_) {
    // This can be called due to the loader cancellation during the
    // `PrefetchContainer` destruction. No state changes should be made and
    // observers shouldn't be notified during destruction.
    return;
  }

  SetLoadState(is_successful_determined_head
                   ? LoadState::kDeterminedHead
                   : LoadState::kFailedDeterminedHead);

  if (GetNonRedirectHead()) {
    prefetch_container_metrics_.time_header_determined_successfully =
        base::TimeTicks::Now();

    // Propagates the header to `no_vary_search_data_` if a non-redirect
    // response header is got.
    CHECK(!no_vary_search_data_.has_value());

    // RenderFrameHostImpl will be used to display error messages in DevTools
    // console. Can be null when the prefetch is browser-initiated.
    RenderFrameHostImpl* rfhi_can_be_null = nullptr;
    if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
      rfhi_can_be_null = renderer_initiator_info->GetRenderFrameHost();
    }
    no_vary_search_data_ = no_vary_search::ProcessHead(
        *GetNonRedirectHead(), GetURL(), rfhi_can_be_null);
  }

  NotifyObservers(&PrefetchContainerObserver::OnDeterminedHead);
}

void PrefetchContainer::StartTimeoutTimerIfNeeded(
    base::OnceClosure on_timeout_callback) {
  if (request().ttl().is_positive()) {
    CHECK(!timeout_timer_);
    timeout_timer_ = std::make_unique<base::OneShotTimer>();
    timeout_timer_->Start(FROM_HERE, request().ttl(),
                          std::move(on_timeout_callback));
  }
}

// static
void PrefetchContainer::SetPrefetchResponseCompletedCallbackForTesting(
    PrefetchResponseCompletedCallbackForTesting callback) {
  GetPrefetchResponseCompletedCallbackForTesting() =  // IN-TEST
      std::move(callback);
}

void PrefetchContainer::OnPrefetchCompleteInternal() {
  DVLOG(1) << *this << "::OnPrefetchComplete";

  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.RedirectChainSize",
                           redirect_chain_.size());

  if (GetNonRedirectResponseReader()) {
    UpdatePrefetchRequestMetrics(
        GetNonRedirectResponseReader()->GetHead().get());
    UpdateServingPageMetrics();
  } else {
    DVLOG(1) << *this << "::OnPrefetchComplete:"
             << "no non redirect response reader";
  }

  if (IsDecoy()) {
    SetPrefetchStatus(PrefetchStatus::kPrefetchIsPrivacyDecoy);
    return;
  }

  // TODO(crbug.com/40250089): Call
  // `devtools_instrumentation::OnPrefetchBodyDataReceived()` with body of the
  // response.
  NotifyPrefetchRequestComplete();

  int net_error = GetCompletionStatus()->error_code;

  RecordPrefetchProxyPrefetchMainframeNetError(net_error);

  // Updates the prefetch's status if it hasn't been updated since the request
  // first started. For the prefetch to reach the network stack, it must have
  // `PrefetchStatus::kPrefetchNotStarted` or beyond.
  DCHECK(HasPrefetchStatus());
  if (GetPrefetchStatus() == PrefetchStatus::kPrefetchNotFinishedInTime) {
    SetPrefetchStatus(net_error == net::OK
                          ? PrefetchStatus::kPrefetchSuccessful
                          : PrefetchStatus::kPrefetchFailedNetError);
    UpdateServingPageMetrics();
  }

  if (net_error == net::OK) {
    prefetch_container_metrics_.time_prefetch_completed_successfully =
        base::TimeTicks::Now();
    RecordPrefetchProxyPrefetchMainframeBodyLength(
        GetCompletionStatus()->decoded_body_length);
  }

  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    if (renderer_initiator_info->prefetch_document_manager()) {
      renderer_initiator_info->prefetch_document_manager()
          ->OnPrefetchCompletedOrFailed(*this);
    }
  }
}

// TODO(https://crbug.com/432518638): We should be able to calculate
// `is_success` from the last `PrefetchResponseReader`.
// Before https://crbug.com/432518638 is fixed, we explicitly plumb them here to
// ensure the correct `PrefetchResponseReader`'s states are used.
void PrefetchContainer::OnPrefetchComplete(
    bool is_success,
    const network::URLLoaderCompletionStatus& completion_status) {
  if (is_in_dtor_) {
    // This can be called due to the loader cancellation during the
    // `PrefetchContainer` destruction. No state changes should be made and
    // observers shouldn't be notified during destruction.
    return;
  }

  TRACE_EVENT("loading", "PrefetchContainer::OnPrefetchComplete",
              request_->preload_pipeline_info().GetFlow());

  completion_status_ = completion_status;

  SetLoadState(is_success ? LoadState::kCompleted : LoadState::kFailed);
  OnPrefetchCompleteInternal();

  NotifyObservers(&PrefetchContainerObserver::OnPrefetchCompletedOrFailed);

  if (GetPrefetchResponseCompletedCallbackForTesting()) {
    GetPrefetchResponseCompletedCallbackForTesting().Run(  // IN-TEST
        GetWeakPtr());
  }
}

void PrefetchContainer::UpdatePrefetchRequestMetrics(
    const network::mojom::URLResponseHead* head) {
  DVLOG(1) << *this << "::UpdatePrefetchRequestMetrics:"
           << "head = " << head;

  if (head) {
    header_latency_ =
        head->load_timing.receive_headers_end - head->load_timing.request_start;
  }
}

PrefetchMatchResolverAction PrefetchContainer::GetMatchResolverAction() const {
  const base::TimeDelta cacheable_duration = PrefetchCacheableDuration();

  switch (load_state_) {
    case LoadState::kNotStarted:
      if (features::UsePrefetchPrerenderIntegration()) {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kWait, load_state_,
            std::nullopt);
      } else {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
            std::nullopt);
      }
    case LoadState::kEligible:
      if (features::UsePrefetchPrerenderIntegration()) {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kWait, load_state_,
            std::nullopt);
      } else {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
            std::nullopt);
      }
    case LoadState::kStarted:
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kWait, load_state_,
          std::nullopt);
    case LoadState::kDeterminedHead: {
      const bool is_expired = false;
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe, load_state_,
          is_expired);
    }
    case LoadState::kFailedDeterminedHead:
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
          std::nullopt);
    case LoadState::kCompleted: {
      CHECK(!redirect_chain_.empty());
      CHECK_EQ(redirect_chain_.back()->response_reader().load_state(),
               PrefetchResponseReader::LoadState::kCompleted);
      // This branch corresponds to the first `if` in
      // `GetServableStateInternal2()`.
      CHECK(GetNonRedirectResponseReader());
      const bool is_expired =
          !GetNonRedirectResponseReader()->Servable(cacheable_duration);
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe, load_state_,
          is_expired);
    }
    case LoadState::kFailedHeldback:
    case LoadState::kFailedIneligible:
    case LoadState::kFailed:
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
          std::nullopt);
  }
}

PrefetchSingleRedirectHop&
PrefetchContainer::GetCurrentSingleRedirectHopToPrefetch() const {
  CHECK(redirect_chain_.size() > 0);
  return *redirect_chain_[redirect_chain_.size() - 1];
}

const PrefetchSingleRedirectHop&
PrefetchContainer::GetPreviousSingleRedirectHopToPrefetch() const {
  CHECK(redirect_chain_.size() > 1);
  return *redirect_chain_[redirect_chain_.size() - 2];
}

void PrefetchContainer::SetServingPageMetrics(
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
  serving_page_metrics_container_ = serving_page_metrics_container;
}

void PrefetchContainer::UpdateServingPageMetrics() {
  if (!serving_page_metrics_container_) {
    return;
  }

  serving_page_metrics_container_->SetRequiredPrivatePrefetchProxy(
      request().prefetch_type().IsProxyRequiredWhenCrossOrigin());
  serving_page_metrics_container_->SetPrefetchHeaderLatency(
      GetPrefetchHeaderLatency());
  if (HasPrefetchStatus()) {
    serving_page_metrics_container_->SetPrefetchStatus(GetPrefetchStatus());
  }
}

void PrefetchContainer::SimulatePrefetchEligibleForTest() {
  CHECK_EQ(redirect_chain_.size(), 1u);
  OnEligibilityCheckComplete(PreloadingEligibility::kEligible);
}

void PrefetchContainer::SimulatePrefetchStartedForTest() {
  if (request().attempt()) {
    request().attempt()->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
  }
  OnPrefetchStarted();
}

// Simulates successful cases of
// `PrefetchService::OnGotEligibilityForRedirect()`.
void PrefetchContainer::SimulatePrefetchRedirectedForTest(  // IN-TEST
    const GURL& redirect_url,
    PreloadingEligibility eligibility) {
  // Add a redirect hop with dummy redirect info that should be good enough in
  // most cases.
  net::RedirectInfo redirect_info;
  redirect_info.status_code = 302;
  redirect_info.new_method = "GET";
  redirect_info.new_url = redirect_url;
  redirect_info.new_site_for_cookies =
      net::SiteForCookies::FromUrl(redirect_info.new_url);

  CHECK_GE(redirect_chain_.size(), 1u);

  AddRedirectHop(redirect_info.new_url);

  OnEligibilityCheckComplete(eligibility);

  auto headers_update_params =
      PrepareRedirectHeadersForPrefetch(redirect_info.new_url, request());
  UpdateResourceRequest(redirect_info, std::move(headers_update_params));
}

void PrefetchContainer::SimulatePrefetchFailedIneligibleForTest(
    PreloadingEligibility eligibility) {
  CHECK_NE(PreloadingEligibility::kEligible, eligibility);
  CHECK_EQ(redirect_chain_.size(), 1u);
  OnEligibilityCheckComplete(eligibility);
}

void PrefetchContainer::OnDetectedCookiesChange(
    std::optional<bool>
        is_unblock_for_cookies_changed_triggered_by_this_prefetch_container) {
  // Multiple `PrefetchMatchResolver` can wait the same `PrefetchContainer`. So,
  // `OnDetectedCookiesChange()` can be called multiple times,
  if (on_detected_cookies_change_called_) {
    return;
  }
  on_detected_cookies_change_called_ = true;

  // There are cases that `prefetch_status_` is failure but this method is
  // called. For more details, see
  // https://docs.google.com/document/d/1G48SaWbdOy1yNBT1wio2IHVuUtddF5VLFsT6BRSYPMI/edit?tab=t.hpkotaxo7tfh#heading=h.woaoy8erwx63
  //
  // To prevent crash, we don't call `SetPrefetchStatus()`.
  if (prefetch_status_ &&
      TriggeringOutcomeFromStatus(prefetch_status_.value()) ==
          PreloadingTriggeringOutcome::kFailure) {
    SCOPED_CRASH_KEY_NUMBER("PrefetchContainer", "ODCC2_from",
                            static_cast<int>(prefetch_status_.value()));
    if (is_unblock_for_cookies_changed_triggered_by_this_prefetch_container
            .has_value()) {
      SCOPED_CRASH_KEY_BOOL(
          "PrefetchContainer", "ODCC2_iufcctbtpc",
          is_unblock_for_cookies_changed_triggered_by_this_prefetch_container
              .value());
    }
    base::debug::DumpWithoutCrashing();
    return;
  }

  CHECK_NE(GetPrefetchStatus(), PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  SetPrefetchStatus(PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  UpdateServingPageMetrics();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrefetchContainer::CancelStreamingURLLoaderIfNotServing,
                     GetWeakPtr()));
}

void PrefetchContainer::OnPrefetchStarted() {
  TRACE_EVENT("loading", "PrefetchContainer::OnPrefetchStarted",
              request_->preload_pipeline_info().GetFlow());

  SetLoadState(PrefetchContainer::LoadState::kStarted);
  prefetch_container_metrics_.time_prefetch_started = base::TimeTicks::Now();

  if (IsConstructedFromPrePrefetch()) {
    CHECK(base::FeatureList::IsEnabled(features::kPrefetchOffTheMainThread));
    CHECK(resource_request_for_pre_prefetch_);

    ValidateResourceRequestForPrePrefetch(
        *resource_request_for_pre_prefetch_,
        *MakeInitialResourceRequestForPrefetch(request(), IsDecoy()),
        ValidateResourceRequestMode::kOnRequestConstruction);

    // `resource_request_for_pre_prefetch_` was constructed from a non-main
    // thread snapshot during PrePrefetch, and we promote it as a
    // `PrefetchContainer`'s `resource_request_`.
    resource_request_ = std::move(resource_request_for_pre_prefetch_);
  } else {
    resource_request_ =
        MakeInitialResourceRequestForPrefetch(request(), IsDecoy());
  }

  if (!IsDecoy()) {
    // The status is updated to be successful or failed when it finishes.
    SetPrefetchStatus(PrefetchStatus::kPrefetchNotFinishedInTime);
  }

  NotifyPrefetchRequestWillBeSent(
      /*redirect_head=*/nullptr);
}

GURL PrefetchContainer::GetCurrentURL() const {
  return GetCurrentSingleRedirectHopToPrefetch().url();
}

GURL PrefetchContainer::GetPreviousURL() const {
  return GetPreviousSingleRedirectHopToPrefetch().url();
}

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForCurrentPrefetch()
    const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToPrefetch();
  return this_prefetch.is_isolated_network_context_required();
}

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForPreviousRedirectHop()
    const {
  const PrefetchSingleRedirectHop& previous_prefetch =
      GetPreviousSingleRedirectHopToPrefetch();
  return previous_prefetch.is_isolated_network_context_required();
}

base::WeakPtr<PrefetchResponseReader>
PrefetchContainer::GetResponseReaderForCurrentPrefetch() {
  PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToPrefetch();
  return this_prefetch.response_reader().GetWeakPtr();
}

const std::string& PrefetchContainer::GetDevtoolsRequestId() const {
  static const base::NoDestructor<std::string> invalid_request_id(
      base::UnguessableToken::Create().ToString());
  return GetResourceRequest()
             ? GetResourceRequest()->devtools_request_id.value()
             : *invalid_request_id;
}

const PrefetchKey& PrefetchContainer::key() const {
  return request().key();
}

const GURL& PrefetchContainer::GetURL() const {
  return request().key().url();
}

const std::optional<net::HttpNoVarySearchData>&
PrefetchContainer::GetNoVarySearchHint() const {
  return request().no_vary_search_hint();
}

bool PrefetchContainer::IsPrefetchStale() const {
  TRACE_EVENT("loading", "PrefetchContainer::IsPrefetchStale");
  PrefetchServableState servable_state =
      GetMatchResolverAction().ToServableState();
  return servable_state == PrefetchServableState::kNotServable;
}

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchContainer& prefetch_container) {
  return ostream << "PrefetchContainer[" << &prefetch_container
                 << ", Key=" << prefetch_container.key() << "]";
}

std::ostream& operator<<(std::ostream& ostream,
                         PrefetchContainer::LoadState state) {
  switch (state) {
    case PrefetchContainer::LoadState::kNotStarted:
      return ostream << "NotStarted";
    case PrefetchContainer::LoadState::kEligible:
      return ostream << "Eligible";
    case PrefetchContainer::LoadState::kFailedIneligible:
      return ostream << "FailedIneligible";
    case PrefetchContainer::LoadState::kStarted:
      return ostream << "Started";
    case PrefetchContainer::LoadState::kDeterminedHead:
      return ostream << "DeterminedHead";
    case PrefetchContainer::LoadState::kFailedDeterminedHead:
      return ostream << "FailedDeterminedHead";
    case PrefetchContainer::LoadState::kCompleted:
      return ostream << "Completed";
    case PrefetchContainer::LoadState::kFailed:
      return ostream << "Failed";
    case PrefetchContainer::LoadState::kFailedHeldback:
      return ostream << "FailedHeldback";
  }
}

void PrefetchContainer::AddObserver(PrefetchContainerObserver* observer) {
  observers_.AddObserver(observer);
}

void PrefetchContainer::RemoveObserver(PrefetchContainerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool PrefetchContainer::IsExactMatch(const GURL& url) const {
  return url == GetURL();
}

bool PrefetchContainer::IsNoVarySearchHeaderMatch(const GURL& url) const {
  return no_vary_search_data_ &&
         no_vary_search_data_->AreEquivalent(url, GetURL());
}

bool PrefetchContainer::ShouldWaitForNoVarySearchHeader(const GURL& url) const {
  switch (GetLoadState()) {
    case LoadState::kDeterminedHead:
    case LoadState::kCompleted:
      return false;

    case LoadState::kNotStarted:
    case LoadState::kEligible:
    case LoadState::kStarted:
      if (const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint =
              request().no_vary_search_hint()) {
        return no_vary_search_hint->AreEquivalent(url, GetURL());
      }
      return false;

    case LoadState::kFailedDeterminedHead:
    case LoadState::kFailed:
    case LoadState::kFailedIneligible:
    case LoadState::kFailedHeldback:
      return false;
  }
}

void PrefetchContainer::OnUnregisterCandidate(
    const GURL& navigated_url,
    bool is_served,
    PrefetchPotentialCandidateServingResult serving_result,
    bool is_nav_prerender,
    std::optional<base::TimeDelta> blocked_duration) {
  // Note that this method can be called with `is_in_dtor_` true.
  //
  // TODO(crbug.com/356314759): Avoid calling this with `is_in_dtor_`
  // true.

  if (is_served) {
    served_count_++;

    UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.AfterClick.RedirectChainSize",
                             redirect_chain_.size());
  }

  RecordPrefetchMatchingBlockedNavigationHistogram(blocked_duration.has_value(),
                                                   is_nav_prerender);

  RecordBlockUntilHeadDurationHistogram(blocked_duration, is_served,
                                        is_nav_prerender);

  RecordPrefetchPotentialCandidateServingResultHistogram(serving_result);

  // Note that `PreloadingAttemptImpl::SetIsAccurateTriggering()` is called for
  // prefetch in
  //
  // - A. `PreloadingDataImpl::DidStartNavigation()`
  // - B. Here
  //
  // A covers prefetches that satisfy `bool(GetNonRedirectHead())` at that
  // timing. B covers almost all ones that were once potentially matching to the
  // navigation, including that was `kBlockUntilHead` state.
  //
  // Note that multiple calls are safe and set a correct value.
  //
  // Historical note: Before No-Vary-Search hint, the decision to use a
  // prefetched response was made at A. With No-Vary-Search hint the decision to
  // use an in-flight prefetched response is delayed until the headers are
  // received from the server. This happens after `DidStartNavigation()`. At
  // this point in the code we have already decided we are going to use the
  // prefetch, so we can safely call `SetIsAccurateTriggering()`.
  if (request().attempt()) {
    static_cast<PreloadingAttemptImpl*>(request().attempt())
        ->SetIsAccurateTriggering(navigated_url);
  }
}

void PrefetchContainer::MergeNewPrefetchRequest(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  // Propagate eligibility (and status) to `prefetch_request`.
  //
  // Assume we don't. (*) case is problematic.
  //
  // - If eligibility is not got, eligibility and status will be propagated by
  //   the following `OnEligibilityCheckComplete()` and
  //   `SetPrefetchStatusWithoutUpdatingTriggeringOutcome()`.
  // - If eligibility is got and ineligible, this `PrefetchContainer` is
  //   `kNotServed` and `MergeNewPrefetchRequest()` is not called.
  // - If eligibility is got and `kEligible`:
  //   - If status is not got, status will be propagated by the following
  //     `SetPrefetchStatusWithoutUpdatingTriggeringOutcome()`.
  //     - If status is eventually `kPrefetchSuccessful` or
  //       `kPrefetchResponseUsed`, `kPrefetchResponseUsed` will be propagated
  //       at the prefetch matching end.
  //     - If status is eventually failure, status is propagated, but
  //       eligibility is `kUnspecified`. (*)
  //   - If status is got and `kPrefetchSuccessful` or `kPrefetchResponseUsed`,
  //     `kPrefetchResponseUsed` will be propagated at the prefetch matching
  //     end.
  //   - If status is got and failure, this `PrefetchContainer` is `kNotServed`
  //     and `MergeNewPrefetchRequest()` is not called.
  //
  // In (*), `PrerenderHost` have to cancel prerender with eligibility
  // `kUnspecified` and status failure. It's relatively complicated condition.
  // See a test
  // `PrerendererImplBrowserTestPrefetchAhead.PrefetchMigratedPrefetchFailurePrerenderFailure`.
  //
  // To make things simple, we propagate both eligibility and status.
  scoped_refptr<PreloadPipelineInfoImpl> added_preload_pipeline_info =
      base::WrapRefCounted(&prefetch_request->preload_pipeline_info());

  added_preload_pipeline_info->SetPrefetchEligibility(
      request().preload_pipeline_info().prefetch_eligibility());
  if (auto prefetch_status =
          request().preload_pipeline_info().prefetch_status()) {
    added_preload_pipeline_info->SetPrefetchStatus(*prefetch_status);
  }

  inherited_preload_pipeline_infos_.push_back(
      std::move(added_preload_pipeline_info));

  is_likely_ahead_of_prerender_ |= CalculateIsLikelyAheadOfPrerender(
      prefetch_request->preload_pipeline_info());
}

void PrefetchContainer::NotifyPrefetchRequestWillBeSent(
    const network::mojom::URLResponseHeadPtr* redirect_head) {
  if (IsDecoy()) {
    return;
  }

  auto* renderer_initiator_info = request().GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return;
  }

  auto* rfh = renderer_initiator_info->GetRenderFrameHost();
  auto* ftn = FrameTreeNode::From(rfh);
  if (!rfh) {
    // Don't emit CDP events if the initiator document isn't alive.
    return;
  }

  if (redirect_head && *redirect_head) {
    const network::mojom::URLResponseHeadDevToolsInfoPtr info =
        network::ExtractDevToolsInfo(**redirect_head);
    const GURL url = GetPreviousURL();
    std::pair<const GURL&, const network::mojom::URLResponseHeadDevToolsInfo&>
        redirect_info{url, *info.get()};
    devtools_instrumentation::OnPrefetchRequestWillBeSent(
        *ftn, GetDevtoolsRequestId(), rfh->GetLastCommittedURL(),
        *GetResourceRequest(), std::move(redirect_info));
  } else {
    devtools_instrumentation::OnPrefetchRequestWillBeSent(
        *ftn, GetDevtoolsRequestId(), rfh->GetLastCommittedURL(),
        *GetResourceRequest(), std::nullopt);
  }
}

void PrefetchContainer::NotifyPrefetchResponseReceived(
    const network::mojom::URLResponseHead& head) {
  // Ensured by the caller
  // `PrefetchContainer::OnPrefetchResponseStartedInternal()`.
  CHECK(!IsDecoy());

  prefetch_container_metrics_.time_url_request_started =
      head.load_timing.request_start;
  prefetch_container_metrics_.time_domain_lookup_started =
      head.load_timing.connect_timing.domain_lookup_start;

  if (head.load_timing_internal_info.has_value()) {
    prefetch_container_metrics_.create_stream_delay =
        head.load_timing_internal_info->create_stream_delay;
    prefetch_container_metrics_.connected_callback_delay =
        head.load_timing_internal_info->connected_callback_delay;
    prefetch_container_metrics_.initialize_stream_delay =
        head.load_timing_internal_info->initialize_stream_delay;
  }

  // DevTools plumbing.
  auto* renderer_initiator_info = request().GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return;
  }

  auto* ftn =
      FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost());
  if (!ftn) {
    // Don't emit CDP events if the initiator document isn't alive.
    return;
  }

  devtools_instrumentation::OnPrefetchResponseReceived(
      ftn, GetDevtoolsRequestId(), GetCurrentURL(), head);
}

void PrefetchContainer::NotifyPrefetchRequestComplete() {
  // Ensured by the caller `PrefetchContainer::OnPrefetchCompleteInternal()`.
  CHECK(!IsDecoy());

  auto* renderer_initiator_info = request().GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return;
  }

  auto* ftn =
      FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost());
  if (!ftn) {
    // Don't emit CDP events if the initiator document isn't alive.
    return;
  }

  devtools_instrumentation::OnPrefetchRequestComplete(
      ftn, GetDevtoolsRequestId(), *GetCompletionStatus());
}

std::string PrefetchContainer::GetMetricsSuffix() const {
  std::optional<std::string> histogram_suffix;
  if (auto* browser_initiator_info = request().GetBrowserInitiatorInfo()) {
    histogram_suffix = browser_initiator_info->histogram_suffix();
  }
  return GetMetricsSuffixTriggerTypeAndEagerness(request().prefetch_type(),
                                                 histogram_suffix);
}

bool PrefetchContainer::HasPreloadPipelineInfoForMetrics(
    const PreloadPipelineInfo& other) const {
  if (&request().preload_pipeline_info() == &other) {
    return true;
  }

  for (const auto& preload_pipeline_info : inherited_preload_pipeline_infos_) {
    if (preload_pipeline_info.get() == &other) {
      return true;
    }
  }

  return false;
}

void PrefetchContainer::MaybeRecordPrefetchStatusToUMA(
    PrefetchStatus prefetch_status) {
  if (prefetch_status_recorded_to_uma_) {
    return;
  }

  base::UmaHistogramEnumeration("Preloading.Prefetch.PrefetchStatus",
                                prefetch_status);
  prefetch_status_recorded_to_uma_ = true;
}

void PrefetchContainer::OnServiceWorkerStateDetermined(
    PrefetchServiceWorkerState service_worker_state) {
  switch (service_worker_state_) {
    case PrefetchServiceWorkerState::kDisallowed:
      CHECK_EQ(service_worker_state, PrefetchServiceWorkerState::kDisallowed);
      break;
    case PrefetchServiceWorkerState::kAllowed:
      CHECK_NE(service_worker_state, PrefetchServiceWorkerState::kAllowed);
      service_worker_state_ = service_worker_state;
      break;
    case PrefetchServiceWorkerState::kControlled:
      NOTREACHED();
  }
}

void PrefetchContainer::RecordPrefetchDurationHistogram() {
  if (!prefetch_container_metrics_.time_added_to_prefetch_service.has_value()) {
    return;
  }

  if (!prefetch_container_metrics_.time_initial_eligibility_got.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToInitialEligibility.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_initial_eligibility_got.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  if (!prefetch_container_metrics_.time_prefetch_started.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToPrefetchStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_started.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.InitialEligibilityToPrefetchStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_started.value() -
          prefetch_container_metrics_.time_initial_eligibility_got.value());

  if (!prefetch_container_metrics_.time_url_request_started.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToURLRequestStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_url_request_started.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.PrefetchStartedToURLRequestStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_url_request_started.value() -
          prefetch_container_metrics_.time_prefetch_started.value());

  CHECK(prefetch_container_metrics_.time_domain_lookup_started.has_value());
  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToDomainLookupStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_domain_lookup_started.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());
  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.PrefetchStartedToDomainLookupStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_domain_lookup_started.value() -
          prefetch_container_metrics_.time_prefetch_started.value());

  if (prefetch_container_metrics_.create_stream_delay.has_value()) {
    base::UmaHistogramTimes(base::StrCat({
                                "Prefetch.PrefetchContainer.CreateStreamDelay.",
                                GetMetricsSuffix(),
                            }),
                            *prefetch_container_metrics_.create_stream_delay);
  }
  if (prefetch_container_metrics_.connected_callback_delay.has_value()) {
    base::UmaHistogramTimes(
        base::StrCat({
            "Prefetch.Prefetchcontainer.ConnectedCallbackDelay.",
            GetMetricsSuffix(),
        }),
        *prefetch_container_metrics_.connected_callback_delay);
  }
  if (prefetch_container_metrics_.initialize_stream_delay) {
    base::UmaHistogramTimes(
        base::StrCat({
            "Prefetch.Prefetchcontainer.InitializeStreamDelay.",
            GetMetricsSuffix(),
        }),
        *prefetch_container_metrics_.initialize_stream_delay);
  }

  if (!prefetch_container_metrics_.time_header_determined_successfully
           .has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToHeaderDeterminedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_header_determined_successfully.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer."
          "PrefetchStartedToHeaderDeterminedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_header_determined_successfully.value() -
          prefetch_container_metrics_.time_prefetch_started.value());

  if (!prefetch_container_metrics_.time_prefetch_completed_successfully
           .has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToPrefetchCompletedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_completed_successfully.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer."
          "PrefetchStartedToPrefetchCompletedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_completed_successfully.value() -
          prefetch_container_metrics_.time_prefetch_started.value());
}

void PrefetchContainer::RecordPrefetchMatchMissedToPrefetchStartedHistogram() {
  if (prefetch_container_metrics_.time_prefetch_started.has_value() &&
      time_prefetch_match_missed_.has_value()) {
    base::UmaHistogramTimes(
        base::StrCat({
            "Prefetch.PrefetchContainer.PrefetchMatchMissedToPrefetchStarted.",
            GetMetricsSuffix(),
        }),
        prefetch_container_metrics_.time_prefetch_started.value() -
            time_prefetch_match_missed_.value());
  }
}

void PrefetchContainer::RecordPrefetchMatchingBlockedNavigationHistogram(
    bool blocked_until_head,
    bool is_nav_prerender) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.",
           GetMetricsSuffix()}),
      blocked_until_head);
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.",
           is_nav_prerender ? "Prerender." : "NonPrerender.",
           GetMetricsSuffix()}),
      blocked_until_head);
}

void PrefetchContainer::RecordBlockUntilHeadDurationHistogram(
    const std::optional<base::TimeDelta>& blocked_duration,
    bool served,
    bool is_nav_prerender) {
  base::UmaHistogramTimes(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.",
                    served ? "Served." : "NotServed.", GetMetricsSuffix()}),
      blocked_duration.value_or(base::Seconds(0)));
  base::UmaHistogramTimes(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.",
                    is_nav_prerender ? "Prerender." : "NonPrerender.",
                    served ? "Served." : "NotServed.", GetMetricsSuffix()}),
      blocked_duration.value_or(base::Seconds(0)));
}

void PrefetchContainer::RecordPrefetchPotentialCandidateServingResultHistogram(
    PrefetchPotentialCandidateServingResult serving_result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Prefetch.PrefetchPotentialCandidateServingResult."
                    "PerMatchingCandidate.",
                    GetMetricsSuffix()}),
      serving_result);
}

void PrefetchContainer::RecordPrefetchContainerServedCountHistogram() {
  base::UmaHistogramCounts100(
      base::StrCat(
          {"Prefetch.PrefetchContainer.ServedCount.", GetMetricsSuffix()}),
      served_count_);
}

}  // namespace content
