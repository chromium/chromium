// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_throttle_registry_impl.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/picture_in_picture/document_picture_in_picture_navigation_throttle.h"
#include "content/browser/preloading/prefetch/contamination_delay_navigation_throttle.h"
#include "content/browser/preloading/prerender/prerender_navigation_throttle.h"
#include "content/browser/preloading/prerender/prerender_subframe_navigation_throttle.h"
#include "content/browser/renderer_host/ancestor_throttle.h"
#include "content/browser/renderer_host/back_forward_cache_subframe_navigation_throttle.h"
#include "content/browser/renderer_host/blocked_scheme_navigation_throttle.h"
#include "content/browser/renderer_host/http_error_navigation_throttle.h"
#include "content/browser/renderer_host/isolated_web_app_throttle.h"
#include "content/browser/renderer_host/mixed_content_navigation_throttle.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_throttle_runner.h"
#include "content/browser/renderer_host/navigation_throttle_runner2.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/renderer_cancellation_throttle.h"
#include "content/browser/renderer_host/subframe_history_navigation_throttle.h"
#include "content/browser/webid/navigation_interceptor.h"
#include "content/common/features.h"
#include "content/public/browser/navigation_handle.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/android_spare_renderer_navigation_throttle.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

namespace {

std::unique_ptr<NavigationThrottleRunnerBase> CreateNavigationThrottleRunner(
    NavigationThrottleRegistryBase* registry,
    int64_t navigation_id,
    bool is_primary_main_frame) {
  if (base::FeatureList::IsEnabled(features::kNavigationThrottleRunner2)) {
    return std::make_unique<NavigationThrottleRunner2>(registry, navigation_id,
                                                       is_primary_main_frame);
  }
  return std::make_unique<NavigationThrottleRunner>(registry, navigation_id,
                                                    is_primary_main_frame);
}

}  // namespace

NavigationThrottleRegistryBase::~NavigationThrottleRegistryBase() = default;

NavigationThrottleRegistryImpl::NavigationThrottleRegistryImpl(
    NavigationRequest* navigation_request)
    : navigation_request_(CHECK_DEREF(navigation_request)),
      navigation_throttle_runner_(CreateNavigationThrottleRunner(
          this,
          navigation_request->GetNavigationId(),
          navigation_request->IsInPrimaryMainFrame())) {}

NavigationThrottleRegistryImpl::~NavigationThrottleRegistryImpl() = default;

void NavigationThrottleRegistryImpl::RegisterNavigationThrottles() {
  if (navigation_request_->IsInitialWebUISyncNavigation()) {
    // Skip adding throttles for navigations to the initial WebUI.
    return;
  }
  TRACE_EVENT0("navigation",
               "NavigationThrottleRegistryImpl::RegisterNavigationThrottles");
  // Note: `throttles_` might not be empty. Some NavigationThrottles might have
  // been registered with RegisterThrottleForTesting. These must reside at the
  // end of `throttles_`. TestNavigationManagerThrottle expects that the
  // NavigationThrottles added for test are the last NavigationThrottles to
  // execute. Take them out while appending the rest of the
  // NavigationThrottles.
  std::vector<std::unique_ptr<NavigationThrottle>> testing_throttles =
      std::move(throttles_);

  // The NavigationRequest associated with the NavigationThrottles this
  // NavigationThrottleRunner manages.
  navigation_request_->GetDelegate()->CreateThrottlesForNavigation(*this);

  // Check for renderer-initiated main frame navigations to blocked URL schemes
  // (data, filesystem). This is done early as it may block the main frame
  // navigation altogether.
  BlockedSchemeNavigationThrottle::MaybeCreateAndAdd(*this);

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kAndroidWarmUpSpareRendererWithTimeout) &&
      features::kAndroidSpareRendererAddNavigationThrottle.Get()) {
    AndroidSpareRendererNavigationThrottle::CreateAndAdd(*this);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // Prevent cross-document navigations from document picture-in-picture
  // windows.
  DocumentPictureInPictureNavigationThrottle::MaybeCreateAndAdd(*this);

  AncestorThrottle::CreateAndAdd(*this);

  // Check for mixed content. This is done after the AncestorThrottle and the
  // FormSubmissionThrottle so that when folks block mixed content with a CSP
  // policy, they don't get a warning. They'll still get a warning in the
  // console about CSP blocking the load.
  MixedContentNavigationThrottle::CreateAndAdd(*this);

  // Delay response processing for certain prefetch responses where it might
  // otherwise reveal information about cross-site state.
  ContaminationDelayNavigationThrottle::MaybeCreateAndAdd(*this);

  // Block certain requests that are not permitted for prerendering.
  PrerenderNavigationThrottle::MaybeCreateAndAdd(*this);

  // Defer cross-origin subframe loading during prerendering state.
  PrerenderSubframeNavigationThrottle::MaybeCreateAndAdd(*this);

  // Prevent navigations to/from Isolated Web Apps.
  IsolatedWebAppThrottle::MaybeCreateAndAdd(*this);

  devtools_instrumentation::CreateAndAddNavigationThrottles(*this);

  // Make main frame navigations with error HTTP status code and an empty body
  // commit an error page instead. Note that this should take lower priority
  // than other throttles that might care about those navigations, e.g.
  // throttles handling pages with 407 errors that require extra authentication.
  HttpErrorNavigationThrottle::MaybeCreateAndAdd(*this);

  // Wait for renderer-initiated navigation cancelation window to end. This will
  // wait for the JS task that starts the navigation to finish, so add it close
  // to the end to not delay running other throttles.
  RendererCancellationThrottle::MaybeCreateAndAdd(*this);

  // Defer any cross-document subframe history navigations if there is an
  // associated main-frame same-document history navigation in progress, until
  // the main frame has had an opportunity to fire a navigate event in the
  // renderer. If the navigate event cancels the history navigation, the
  // subframe navigations should not proceed.
  SubframeHistoryNavigationThrottle::MaybeCreateAndAdd(*this);

  // Defer subframe navigation in bfcached page if it hasn't sent a network
  // request.
  // This must be the last throttle to run. See https://crrev.com/c/5316738.
  BackForwardCacheSubframeNavigationThrottle::MaybeCreateAndAdd(*this);

  // Maybe add a throttle to manage navigations from relying parties to FedCM
  // identity providers.
  content::webid::NavigationInterceptor::MaybeCreateAndAdd(*this);

  // DO NOT ADD any throttles after this line.

  // Insert all testing NavigationThrottles last.
  throttles_.insert(throttles_.end(),
                    std::make_move_iterator(testing_throttles.begin()),
                    std::make_move_iterator(testing_throttles.end()));

  base::UmaHistogramCounts100("Navigation.ThrottleCount", throttles_.size());
}

void NavigationThrottleRegistryImpl::
    RegisterNavigationThrottlesForCommitWithoutUrlLoader() {
  if (navigation_request_->IsInitialWebUISyncNavigation()) {
    // Skip adding throttles for navigations to the initial WebUI.
    return;
  }
  // Note: `throttles_` might not be empty. Some NavigationThrottles might have
  // been registered with RegisterThrottleForTesting. These must reside at the
  // end of `throttles_`. TestNavigationManagerThrottle expects that the
  // NavigationThrottles added for test are the last NavigationThrottles to
  // execute. Take them out while appending the rest of the
  // NavigationThrottles.
  std::vector<std::unique_ptr<NavigationThrottle>> testing_throttles =
      std::move(throttles_);

  // Defer any same-document subframe history navigations if there is an
  // associated main-frame same-document history navigation in progress, until
  // the main frame has had an opportunity to fire a navigate event in the
  // renderer. If the navigate event cancels the history navigation, the
  // subframe navigations should not proceed.
  SubframeHistoryNavigationThrottle::MaybeCreateAndAdd(*this);

  // Defer cross-origin about:srcdoc subframe loading during prerendering state.
  PrerenderSubframeNavigationThrottle::MaybeCreateAndAdd(*this);

  // Defer subframe navigation in bfcached page.
  BackForwardCacheSubframeNavigationThrottle::MaybeCreateAndAdd(*this);

  RendererCancellationThrottle::MaybeCreateAndAdd(*this);

#if !BUILDFLAG(IS_ANDROID)
  // Prevent cross-document navigations from document picture-in-picture
  // windows.
  DocumentPictureInPictureNavigationThrottle::MaybeCreateAndAdd(*this);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Insert all testing NavigationThrottles last.
  throttles_.insert(throttles_.end(),
                    std::make_move_iterator(testing_throttles.begin()),
                    std::make_move_iterator(testing_throttles.end()));
}

void NavigationThrottleRegistryImpl::ProcessNavigationEvent(
    NavigationThrottleEvent event) {
  base::WeakPtr<NavigationThrottleRegistryImpl> weak_ref =
      weak_factory_.GetWeakPtr();
  navigation_throttle_runner_->ProcessNavigationEvent(event);
  // DO NOT ADD CODE BETWEEN THIS AND THE WEAK_REF CHECK BELOW, as the
  // NavigationHandle might have been deleted by the previous call.
  if (!weak_ref) {
    // The NavigationEvent handling might have destroyed NavigationHandle
    // and its owning this instance. Return immediately.
    return;
  }
  if (!deferring_throttles_.empty() && first_deferral_callback_for_testing_) {
    std::move(first_deferral_callback_for_testing_).Run();  // IN-TEST
  }
}

void NavigationThrottleRegistryImpl::ResumeProcessingNavigationEvent(
    NavigationThrottle* resuming_throttle) {
  auto it = deferring_throttles_.find(resuming_throttle);
  if (it == deferring_throttles_.end()) {
    // TODO(https://crbug.com/411238078): Upgrade to CHECK_EQ once remaining
    // known cases are fixed. Until then, collect dump data and ignore the
    // resume request to avoid bypassing required throttle checks.
    const char* deferring_throttle_name =
        deferring_throttles_.empty()
            ? "null"
            : (*deferring_throttles_.begin())->GetNameForLogging();
    SCOPED_CRASH_KEY_STRING32("Bug411238078", "expected_throttle",
                              deferring_throttle_name);
    SCOPED_CRASH_KEY_STRING32("Bug411238078", "actual_throttle",
                              resuming_throttle->GetNameForLogging());
    base::debug::DumpWithoutCrashing();
    return;
  }
  deferring_throttles_.erase(it);

  navigation_throttle_runner_->ResumeProcessingNavigationEvent(
      resuming_throttle);
  // DO NOT ADD CODE AFTER THIS, as the NavigationHandle might have been deleted
  // by the previous call.
}

void NavigationThrottleRegistryImpl::OnDeferProcessingNavigationEvent(
    NavigationThrottle* deferring_throttle) {
  deferring_throttles_.insert(deferring_throttle);
}

const std::set<NavigationThrottle*>&
NavigationThrottleRegistryImpl::GetDeferringThrottles() const {
  return deferring_throttles_;
}

void NavigationThrottleRegistryImpl::SetFirstDeferralCallbackForTesting(
    base::OnceClosure callback) {
  CHECK(deferring_throttles_.empty());
  first_deferral_callback_for_testing_ = std::move(callback);
}

NavigationHandle& NavigationThrottleRegistryImpl::GetNavigationHandle() {
  return *navigation_request_;
}

void NavigationThrottleRegistryImpl::AddThrottle(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  CHECK(navigation_throttle);
  TRACE_EVENT1("navigation", "NavigationThrottleRegistryImpl::AddThrottle",
               "navigation_throttle", navigation_throttle->GetNameForLogging());
  CHECK(!navigation_request_->IsInitialWebUISyncNavigation());
  throttles_.push_back(std::move(navigation_throttle));
}

bool NavigationThrottleRegistryImpl::HasThrottle(const std::string& name) {
  return std::ranges::find_if(throttles_, [name](const auto& throttle) {
           return throttle->GetNameForLogging() == name;
         }) != throttles_.end();
}

bool NavigationThrottleRegistryImpl::EraseThrottleForTesting(
    const std::string& name) {
  return std::erase_if(throttles_, [name](const auto& throttle) {
    return throttle->GetNameForLogging() == name;
  });
}

bool NavigationThrottleRegistryImpl::IsHTTPOrHTTPS() {
  static bool is_cache_enabled = base::FeatureList::IsEnabled(
      features::kNavigationThrottleRegistryAttributeCache);
  // The cached properties are only safe to access at throttle registration
  // time, and not safe afterward because the URL could change (e.g., due to
  // redirects).
  CHECK_LE(navigation_request_->state(),
           NavigationRequest::NavigationState::WILL_START_REQUEST);

  if (!is_cache_enabled) {
    return GetNavigationHandle().GetURL().SchemeIsHTTPOrHTTPS();
  }
  if (!is_http_or_https_.has_value()) {
    is_http_or_https_ = GetNavigationHandle().GetURL().SchemeIsHTTPOrHTTPS();
  }
  return *is_http_or_https_;
}

void NavigationThrottleRegistryImpl::OnEventProcessed(
    NavigationThrottleEvent event,
    NavigationThrottle::ThrottleCheckResult result) {
  navigation_request_->OnNavigationEventProcessed(event, result);
}

std::vector<std::unique_ptr<NavigationThrottle>>&
NavigationThrottleRegistryImpl::GetThrottles() {
  return throttles_;
}

NavigationThrottle& NavigationThrottleRegistryImpl::GetThrottleAtIndex(
    size_t index) {
  CHECK_LT(index, throttles_.size());
  return *throttles_[index];
}

}  // namespace content
