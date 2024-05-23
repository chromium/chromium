// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/containers/contains.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safebrowsing_constants.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper_delegate.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "net/base/apple/url_conversions.h"

using safe_browsing::SafeBrowsingUrlCheckerImpl;
using security_interstitials::UnsafeResource;

namespace {
// Creates a PolicyDecision that cancels a navigation to show a safe browsing
// error.
web::WebStatePolicyDecider::PolicyDecision CreateSafeBrowsingErrorDecision() {
  return web::WebStatePolicyDecider::PolicyDecision::CancelAndDisplayError(
      [NSError errorWithDomain:kSafeBrowsingErrorDomain
                          code:kUnsafeResourceErrorCode
                      userInfo:nil]);
}

// Returns a canonicalized version of `url` as used by the SafeBrowsingService.
GURL GetCanonicalizedUrl(const GURL& url) {
  std::string hostname;
  std::string path;
  std::string query;
  safe_browsing::V4ProtocolManagerUtil::CanonicalizeUrl(url, &hostname, &path,
                                                        &query);

  GURL::Replacements replacements;
  if (hostname.length())
    replacements.SetHostStr(hostname);
  if (path.length())
    replacements.SetPathStr(path);
  if (query.length())
    replacements.SetQueryStr(query);
  replacements.ClearRef();

  return url.ReplaceComponents(replacements);
}

// Records a histogram tracking how often Safe Browsing delays navigations.
void RecordCheckCompletedOnResponseMetric(bool check_completed) {
  base::UmaHistogramBoolean(
      "SafeBrowsing.IOS.IsCheckCompletedOnShouldAllowResponse",
      check_completed);
}

// Records histograms tracking the amount of time that navigations are delayed
// by Safe Browsing, broken down by the type of Safe Browsing check that was
// performed. Unlike `RecordTotalDelayMetricForDelayedAllowedNavigation`, this
// should be called for all completed checks, even those that don't cause any
// delay and those that are blocked by Safe Browsing.
void RecordTotalDelay2MetricForNavigation(
    base::TimeDelta delay,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  std::string performed_check_str;
  switch (performed_check) {
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck:
      performed_check_str = ".FullUrlLookup";
      break;
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck:
      performed_check_str = ".HashPrefixDatabaseCheck";
      break;
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped:
      performed_check_str = ".SkippedCheck";
      break;
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck:
      performed_check_str = ".HashPrefixRealTimeCheck";
      break;
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kUnknown:
      NOTREACHED_IN_MIGRATION();
  }

  base::UmaHistogramTimes("SafeBrowsing.IOS.TotalDelay2" + performed_check_str,
                          delay);
}

// Records a histogram tracking the amount of time that navigations are delayed
// by Safe Browsing but ultimately allowed to proceed.
void RecordTotalDelayMetricForDelayedAllowedNavigation(
    base::TimeTicks delay_start_time,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  base::TimeDelta delay = base::TimeTicks::Now() - delay_start_time;
  base::UmaHistogramTimes("SafeBrowsing.IOS.TotalDelay", delay);
  RecordTotalDelay2MetricForNavigation(delay, performed_check);
}
}  // namespace

#pragma mark - SafeBrowsingTabHelper

SafeBrowsingTabHelper::SafeBrowsingTabHelper(web::WebState* web_state,
                                             SafeBrowsingClient* client)
    : policy_decider_(web_state, client),
      query_observer_(web_state, &policy_decider_),
      navigation_observer_(web_state, &policy_decider_) {}

SafeBrowsingTabHelper::~SafeBrowsingTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(SafeBrowsingTabHelper)

void SafeBrowsingTabHelper::SetDelegate(
    id<SafeBrowsingTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

void SafeBrowsingTabHelper::RemoveDelegate() {
  delegate_ = nil;
}

void SafeBrowsingTabHelper::OpenSafeBrowsingSettings() {
  if (delegate_) {
    [delegate_ openSafeBrowsingSettings];
  }
}

void SafeBrowsingTabHelper::ShowEnhancedSafeBrowsingInfobar() {
  if (delegate_) {
    [delegate_ showEnhancedSafeBrowsingInfobar];
  }
}

#pragma mark - SafeBrowsingTabHelper::PolicyDecider

SafeBrowsingTabHelper::PolicyDecider::PolicyDecider(web::WebState* web_state,
                                                    SafeBrowsingClient* client)
    : web::WebStatePolicyDecider(web_state),
      query_manager_(SafeBrowsingQueryManager::FromWebState(web_state)),
      client_(client) {}

SafeBrowsingTabHelper::PolicyDecider::~PolicyDecider() = default;

bool SafeBrowsingTabHelper::PolicyDecider::IsQueryStale(
    const SafeBrowsingQueryManager::Query& query) {
  const GURL& url = query.url;
  return !GetOldestPendingMainFrameQuery(url);
}

web::WebStatePolicyDecider::PolicyDecision
SafeBrowsingTabHelper::PolicyDecider::CreatePolicyDecision(
    const SafeBrowsingQueryManager::Query& query,
    const SafeBrowsingQueryManager::Result& result,
    web::WebState* web_state) {
  // Create a policy decision using the query result.
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  if (result.show_error_page) {
    policy_decision = CreateSafeBrowsingErrorDecision();
  } else if (!result.proceed) {
    policy_decision = web::WebStatePolicyDecider::PolicyDecision::Cancel();
  }

  // If an error page needs to be displayed, record the pending decision and
  // store the unsafe resource.
  if (policy_decision.ShouldDisplayError()) {
    DCHECK(result.resource);

    // Store the navigation URL for the resource.
    UnsafeResource resource = *result.resource;
    resource.navigation_url = query.url;
    SafeBrowsingUrlAllowList::FromWebState(web_state)
        ->AddPendingUnsafeNavigationDecision(resource.navigation_url,
                                             resource.threat_type);

    // Store the UnsafeResource to be fetched later to populate the error page.
    SafeBrowsingUnsafeResourceContainer* container =
        SafeBrowsingUnsafeResourceContainer::FromWebState(web_state);
    container->StoreMainFrameUnsafeResource(resource);
  }

  return policy_decision;
}

void SafeBrowsingTabHelper::PolicyDecider::HandlePolicyDecision(
    const SafeBrowsingQueryManager::Query& query,
    const web::WebStatePolicyDecider::PolicyDecision& policy_decision,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  DCHECK(!IsQueryStale(query));
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    OnMainFrameUrlSyncQueryDecided(query.url, policy_decision, performed_check);
  } else {
    OnMainFrameUrlQueryDecided(query.url, policy_decision, performed_check);
  }
}

void SafeBrowsingTabHelper::PolicyDecider::UpdateForMainFrameDocumentChange() {
  // TODO(crbug.com/41491791): Update state for async checks on document change.
}

void SafeBrowsingTabHelper::PolicyDecider::UpdateForMainFrameServerRedirect() {
  // The current `pending_main_frame_query_` is a server redirect from
  // `previous_main_frame_query_`, so add the latter to the pending redirect
  // chain. However, when a URL redirects to itself, ShouldAllowRequest may not
  // be called again, and in that case `previous_main_frame_query_` will not
  // have a new URL to add to the redirect chain.
  if (previous_main_frame_query_) {
    pending_main_frame_redirect_chain_.push_back(
        std::move(*previous_main_frame_query_));
    previous_main_frame_query_ = std::nullopt;
  }
}

#pragma mark web::WebStatePolicyDecider

void SafeBrowsingTabHelper::PolicyDecider::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  bool is_main_frame = request_info.target_frame_is_main;
  if (!is_main_frame) {
    base::UmaHistogramBoolean("SafeBrowsing.IOS.SubframeCheck.Skipped", true);
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // Allow navigations for URLs that cannot be checked by the service.
  GURL request_url = GetCanonicalizedUrl(net::GURLWithNSURL(request.URL));
  SafeBrowsingService* safe_browsing_service =
      client_->GetSafeBrowsingService();
  if (!safe_browsing_service->CanCheckUrl(request_url)) {
    return std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
  }

  // Track all pending URL queries.
  if (pending_main_frame_query_) {
    previous_main_frame_query_ = std::move(pending_main_frame_query_);
  }

  pending_main_frame_query_ = MainFrameUrlQuery(request_url);

  // If there is a pre-existing main frame unsafe resource for `request_url`
  // that haven't yet resulted in an error page, this resource can be used to
  // show the error page for the current load.  This can occur in back/forward
  // navigations to safe browsing error pages, where ShouldAllowRequest() is
  // called multiple times consecutively for the same URL.
  SafeBrowsingUnsafeResourceContainer* unsafe_resource_container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state());
  const security_interstitials::UnsafeResource* main_frame_resource =
      unsafe_resource_container->GetMainFrameUnsafeResource();
  if (main_frame_resource && main_frame_resource->url == request_url) {
    // TODO(crbug.com/40681490): This should directly return the safe browsing
    // error decision once error pages for cancelled requests are supported.
    // For now, only cancelled response errors are displayed properly.
    pending_main_frame_query_->decision = CreateSafeBrowsingErrorDecision();
    return std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
  }

  // Start the URL check.
  query_manager_->StartQuery(SafeBrowsingQueryManager::Query(
      request_url, base::SysNSStringToUTF8([request HTTPMethod])));

  // Allow all requests to continue.  If a safe browsing error is detected, the
  // navigation will be cancelled for using the response policy decision.
  std::move(callback).Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
}

void SafeBrowsingTabHelper::PolicyDecider::ShouldAllowResponse(
    NSURLResponse* response,
    web::WebStatePolicyDecider::ResponseInfo response_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  if (!response_info.for_main_frame) {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // Allow navigations for URLs that cannot be checked by the service.
  SafeBrowsingService* safe_browsing_service =
      client_->GetSafeBrowsingService();
  GURL response_url = GetCanonicalizedUrl(net::GURLWithNSURL(response.URL));
  if (!safe_browsing_service->CanCheckUrl(response_url)) {
    return std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
  }

  DCHECK(pending_main_frame_query_);
  // When there's a server redirect, a ShouldAllowRequest call sometimes
  // doesn't happen for the target of the redirection. This seems to be fixed
  // in trunk WebKit.
  if (!pending_main_frame_redirect_chain_.empty()) {
    bool matching_hosts =
        pending_main_frame_query_->url.host() == response_url.host();
    UMA_HISTOGRAM_BOOLEAN(
        "IOS.SafeBrowsing.RedirectedRequestResponseHostsMatch", matching_hosts);
  }
  // If the previous query wasn't added to a pending redirect chain, the
  // pending chain is no longer active, since DidRedirectNavigation() is
  // guaranteed to be called before ShouldAllowResponse() is called for the
  // redirection target.
  if (previous_main_frame_query_) {
    // The previous query was never added to a redirect chain, so the current
    // query is not a redirect.
    previous_main_frame_query_ = std::nullopt;
    pending_main_frame_redirect_chain_.clear();
  }

  auto decision = MainFrameRedirectChainDecision();
  if (decision) {
    RecordCheckCompletedOnResponseMetric(/*check_completed=*/true);
    std::move(callback).Run(*decision);
    pending_main_frame_redirect_chain_.clear();
  } else {
    RecordCheckCompletedOnResponseMetric(/*check_completed=*/false);
    pending_main_frame_query_->response_callback = std::move(callback);
    pending_main_frame_query_->delay_start_time = base::TimeTicks::Now();
  }
}

#pragma mark URL Check Completion Helpers

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery*
SafeBrowsingTabHelper::PolicyDecider::GetOldestPendingMainFrameQuery(
    const GURL& url) {
  for (auto& query : pending_main_frame_redirect_chain_) {
    if (query.url == url && !query.decision) {
      return &query;
    }
  }

  if (pending_main_frame_query_ && pending_main_frame_query_->url == url &&
      !pending_main_frame_query_->decision) {
    return &pending_main_frame_query_.value();
  }

  return nullptr;
}

void SafeBrowsingTabHelper::PolicyDecider::OnMainFrameUrlQueryDecided(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecision decision,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  GetOldestPendingMainFrameQuery(url)->decision = decision;

  // If ShouldAllowResponse() has already been called for this URL, and if
  // an overall decision for the redirect chain can be computed, invoke this
  // URL's callback with the overall decision.
  auto& response_callback = pending_main_frame_query_->response_callback;
  if (!response_callback.is_null()) {
    std::optional<web::WebStatePolicyDecider::PolicyDecision> overall_decision =
        MainFrameRedirectChainDecision();
    if (overall_decision) {
      if (overall_decision->ShouldAllowNavigation()) {
        RecordTotalDelayMetricForDelayedAllowedNavigation(
            pending_main_frame_query_->delay_start_time, performed_check);
      } else {
        base::TimeDelta delay = base::TimeTicks::Now() -
                                pending_main_frame_query_->delay_start_time;
        RecordTotalDelay2MetricForNavigation(delay, performed_check);
      }
      std::move(response_callback).Run(*overall_decision);
      pending_main_frame_redirect_chain_.clear();
    }
  } else {
    RecordTotalDelay2MetricForNavigation(base::TimeDelta(), performed_check);
  }

  if (decision.ShouldCancelNavigation()) {
    client_->OnMainFrameUrlQueryCancellationDecided(web_state(), url);
  }
}

void SafeBrowsingTabHelper::PolicyDecider::OnMainFrameUrlSyncQueryDecided(
    const GURL& url,
    web::WebStatePolicyDecider::PolicyDecision decision,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  GetOldestPendingMainFrameQuery(url)->decision = decision;

  // If ShouldAllowResponse() has already been called for this URL, and if
  // an overall decision for the redirect chain can be computed, invoke this
  // URL's callback with the overall decision.
  auto& response_callback = pending_main_frame_query_->response_callback;
  if (!response_callback.is_null()) {
    std::optional<web::WebStatePolicyDecider::PolicyDecision> overall_decision =
        MainFrameRedirectChainDecision();
    if (overall_decision) {
      if (overall_decision->ShouldAllowNavigation()) {
        RecordTotalDelayMetricForDelayedAllowedNavigation(
            pending_main_frame_query_->delay_start_time, performed_check);
      } else {
        base::TimeDelta delay = base::TimeTicks::Now() -
                                pending_main_frame_query_->delay_start_time;
        RecordTotalDelay2MetricForNavigation(delay, performed_check);
      }
      std::move(response_callback).Run(*overall_decision);
      pending_main_frame_redirect_chain_.clear();
    }
  } else {
    RecordTotalDelay2MetricForNavigation(base::TimeDelta(), performed_check);
  }

  if (decision.ShouldCancelNavigation()) {
    client_->OnMainFrameUrlQueryCancellationDecided(web_state(), url);
  }
}

std::optional<web::WebStatePolicyDecider::PolicyDecision>
SafeBrowsingTabHelper::PolicyDecider::MainFrameRedirectChainDecision() {
  if (pending_main_frame_query_->decision &&
      pending_main_frame_query_->decision->ShouldCancelNavigation()) {
    return pending_main_frame_query_->decision;
  }

  // If some query has received a decision to cancel the navigation or if
  // every query has received a decision to allow the navigation, there is
  // enough information to make an overall decision.
  std::optional<web::WebStatePolicyDecider::PolicyDecision> decision =
      pending_main_frame_query_->decision;
  for (auto& query : pending_main_frame_redirect_chain_) {
    if (!query.decision) {
      decision = std::nullopt;
    } else if (query.decision->ShouldCancelNavigation()) {
      decision = query.decision;
      break;
    }
  }

  return decision;
}

#pragma mark SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::MainFrameUrlQuery(
    const GURL& url)
    : url(url) {}

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::MainFrameUrlQuery(
    MainFrameUrlQuery&& query) = default;

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery&
SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::operator=(
    MainFrameUrlQuery&& other) = default;

SafeBrowsingTabHelper::PolicyDecider::MainFrameUrlQuery::~MainFrameUrlQuery() {
  if (!response_callback.is_null()) {
    std::move(response_callback)
        .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
  }
}

#pragma mark - SafeBrowsingTabHelper::QueryObserver

SafeBrowsingTabHelper::QueryObserver::QueryObserver(web::WebState* web_state,
                                                    PolicyDecider* decider)
    : web_state_(web_state), policy_decider_(decider) {
  DCHECK(policy_decider_);
  scoped_observation_.Observe(
      SafeBrowsingQueryManager::FromWebState(web_state));
}

SafeBrowsingTabHelper::QueryObserver::~QueryObserver() = default;

void SafeBrowsingTabHelper::QueryObserver::SafeBrowsingQueryFinished(
    SafeBrowsingQueryManager* manager,
    const SafeBrowsingQueryManager::Query& query,
    const SafeBrowsingQueryManager::Result& result,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  if (policy_decider_->IsQueryStale(query))
    return;

  // Create a policy decision using the query result.
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      policy_decider_->CreatePolicyDecision(query, result, web_state_);
  policy_decider_->HandlePolicyDecision(query, policy_decision,
                                        performed_check);
}

void SafeBrowsingTabHelper::QueryObserver::SafeBrowsingSyncQueryFinished(
    SafeBrowsingQueryManager* manager,
    const SafeBrowsingQueryManager::Query& query,
    const SafeBrowsingQueryManager::Result& result,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  if (policy_decider_->IsQueryStale(query)) {
    return;
  }

  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      policy_decider_->CreatePolicyDecision(query, result, web_state_);
  policy_decider_->HandlePolicyDecision(query, policy_decision,
                                        performed_check);
}

void SafeBrowsingTabHelper::QueryObserver::SafeBrowsingQueryManagerDestroyed(
    SafeBrowsingQueryManager* manager) {
  DCHECK(scoped_observation_.IsObservingSource(manager));
  scoped_observation_.Reset();
}

#pragma mark - SafeBrowsingTabHelper::NavigationObserver

SafeBrowsingTabHelper::NavigationObserver::NavigationObserver(
    web::WebState* web_state,
    PolicyDecider* policy_decider)
    : policy_decider_(policy_decider) {
  DCHECK(policy_decider_);
  scoped_observation_.Observe(web_state);
}

SafeBrowsingTabHelper::NavigationObserver::~NavigationObserver() = default;

void SafeBrowsingTabHelper::NavigationObserver::DidRedirectNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  policy_decider_->UpdateForMainFrameServerRedirect();
}

void SafeBrowsingTabHelper::NavigationObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->HasCommitted() &&
      !navigation_context->IsSameDocument()) {
    policy_decider_->UpdateForMainFrameDocumentChange();
  }
}

void SafeBrowsingTabHelper::NavigationObserver::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK(scoped_observation_.IsObservingSource(web_state));
  scoped_observation_.Reset();
}
