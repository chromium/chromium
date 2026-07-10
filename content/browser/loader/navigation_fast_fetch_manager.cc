// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_fast_fetch_manager.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

using EligibilityReason = NavigationFastFetchManager::EligibilityReason;

EligibilityReason CalculateEligibilityReason(NavigationRequest& request) {
  if (request.IsSameDocument()) {
    return EligibilityReason::kSameDocument;
  }
  if (request.frame_tree_node()->frame_tree().is_prerendering()) {
    return EligibilityReason::kIsPrerender;
  }
  if (request.IsPrerenderedPageActivation()) {
    return EligibilityReason::kIsPrerenderActivation;
  }
  if (!request.IsInOutermostMainFrame()) {
    return EligibilityReason::kNotInOutermostMainFrame;
  }
  if (request.common_params().method != net::HttpRequestHeaders::kGetMethod) {
    return EligibilityReason::kNotGetMethod;
  }
  if (!request.GetURL().SchemeIs(url::kHttpsScheme)) {
    return EligibilityReason::kNotHttpsScheme;
  }
  if (request.IsReload()) {
    return EligibilityReason::kIsReload;
  }
  if (request.IsHistory()) {
    return EligibilityReason::kIsHistory;
  }
  if (DevToolsAgentHost::IsDebuggerAttached(request.GetWebContents())) {
    return EligibilityReason::kDevToolsAttached;
  }
  if (request.HasPrefetchedSignedExchange()) {
    return EligibilityReason::kHasSignedExchange;
  }

  // StoragePartition check.
  BrowserContext* browser_context =
      request.GetWebContents()->GetBrowserContext();
  StoragePartitionConfig storage_partition_config;
  SiteInstanceImpl* current_instance =
      request.frame_tree_node()->current_frame_host()->GetSiteInstance();
  if (current_instance->IsFixedStoragePartition()) {
    storage_partition_config =
        current_instance->GetSecurityPrincipal().GetStoragePartitionConfig();
  } else {
    storage_partition_config = SiteInfo::GetStoragePartitionConfigForUrl(
        browser_context, request.GetURL());
  }
  StoragePartition* storage_partition =
      browser_context->GetStoragePartition(storage_partition_config);
  // ServiceWorker check.
  ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();
  auto key = blink::StorageKey::CreateFirstParty(
      url::Origin::Create(request.GetURL()));
  if (service_worker_context->MaybeHasRegistrationForStorageKey(key)) {
    // Note: MaybeHasRegistrationForStorageKey can return true if it's not sure.
    return EligibilityReason::kHasServiceWorker;
  }

  // TODO(crbug.com/529425553): Check if there is on-going prefetch.

  return EligibilityReason::kEligible;
}

}  // namespace

// static
std::unique_ptr<NavigationFastFetchManager> NavigationFastFetchManager::Create(
    NavigationRequest& request) {
  base::ElapsedTimer eligibility_timer;

  EligibilityReason reason = CalculateEligibilityReason(request);
  base::UmaHistogramMicrosecondsTimes(
      "Navigation.Experimental.FastFetch.EligibilityCheckDuration",
      eligibility_timer.Elapsed());
  return base::WrapUnique(new NavigationFastFetchManager(reason));
}

NavigationFastFetchManager::NavigationFastFetchManager(
    EligibilityReason eligibility_reason)
    : eligibility_reason_(eligibility_reason),
      eligibility_check_time_(base::TimeTicks::Now()) {}

NavigationFastFetchManager::~NavigationFastFetchManager() {
  if (should_record_eligibility_reason_) {
    base::UmaHistogramEnumeration(
        "Navigation.Experimental.FastFetch.EligibilityReason",
        eligibility_reason_);
    if (eligibility_reason_ == EligibilityReason::kEligible &&
        !outcome_recorded_) {
      RecordOutcome(NavigationOutcome::kCancelled);
    }
  }
}

void NavigationFastFetchManager::OnCommitNavigation(
    NavigationRequest& request) {
  if (eligibility_reason_ != EligibilityReason::kEligible) {
    return;
  }

  RecordOutcome(NavigationOutcome::kCommitted);

  // TODO(crbug.com/529425553): Check if request headers have been modified by
  // throttles or other factors.

  static constexpr char kOpportunityTimeNameLoaderStart[] =
      "Navigation.Experimental.FastFetch.OpportunityTime.LoaderStart";
  static constexpr char kOpportunityTimeNameFetchStart[] =
      "Navigation.Experimental.FastFetch.OpportunityTime.FetchStart";

  CHECK(!eligibility_check_time_.is_null());

  base::TimeTicks loader_start_time =
      request.GetNavigationHandleTiming().loader_start_time;
  if (!loader_start_time.is_null() &&
      loader_start_time >= eligibility_check_time_) {
    base::UmaHistogramTimes(kOpportunityTimeNameLoaderStart,
                            loader_start_time - eligibility_check_time_);
  }

  std::optional<base::TimeTicks> fetch_start_time =
      request.GetNavigationHandleTiming().first_fetch_start_time;
  if (fetch_start_time.has_value() &&
      *fetch_start_time >= eligibility_check_time_) {
    base::UmaHistogramTimes(kOpportunityTimeNameFetchStart,
                            *fetch_start_time - eligibility_check_time_);
  }
}

void NavigationFastFetchManager::OnRequestFailed(
    NavigationRequest& request,
    const network::URLLoaderCompletionStatus& status,
    bool skip_throttles) {
  if (eligibility_reason_ != EligibilityReason::kEligible) {
    return;
  }

  RecordOutcome(NavigationOutcome::kFailed);

  base::UmaHistogramSparse("Navigation.Experimental.FastFetch.FailedNetError",
                           -status.error_code);
  base::UmaHistogramBoolean("Navigation.Experimental.FastFetch.SkipThrottles",
                            skip_throttles);
}

void NavigationFastFetchManager::RecordOutcome(NavigationOutcome outcome) {
  CHECK(!outcome_recorded_);
  outcome_recorded_ = true;
  base::UmaHistogramEnumeration(
      "Navigation.Experimental.FastFetch.NavigationOutcome", outcome);
}

}  // namespace content
