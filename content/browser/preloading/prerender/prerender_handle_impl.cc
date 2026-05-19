// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_handle_impl.h"

#include <limits>

#include "base/debug/dump_without_crashing.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "url/gurl.h"

namespace content {

namespace {

// Returns true when the error callback should be fired. The callback does not
// need to be fired when prerendering succeed but is never activated, or it is
// intentinally cancelled by an embedder (e.g., calling the cancellation API).
// Otherwise, the callback should be fired.
bool ShouldFireErrorCallback(PrerenderFinalStatus status) {
  switch (status) {
    case PrerenderFinalStatus::kActivated:
      NOTREACHED();

    // Prerendering is not activated.
    case PrerenderFinalStatus::kDestroyed:
      return false;

    case PrerenderFinalStatus::kLowEndDevice:
    case PrerenderFinalStatus::kInvalidSchemeRedirect:
    case PrerenderFinalStatus::kInvalidSchemeNavigation:
    case PrerenderFinalStatus::kNavigationRequestBlockedByCsp:
    case PrerenderFinalStatus::kMojoBinderPolicy:
    case PrerenderFinalStatus::kRendererProcessCrashed:
    case PrerenderFinalStatus::kRendererProcessKilled:
    case PrerenderFinalStatus::kDownload:
      return true;

    // Prerendering is intentionally cancelled by the app or not activated.
    case PrerenderFinalStatus::kTriggerDestroyed:
      return false;

    case PrerenderFinalStatus::kNavigationNotCommitted:
    case PrerenderFinalStatus::kNavigationBadHttpStatus:
    case PrerenderFinalStatus::kClientCertRequested:
    case PrerenderFinalStatus::kNavigationRequestNetworkError:
    case PrerenderFinalStatus::kCancelAllHostsForTesting:
    case PrerenderFinalStatus::kDidFailLoad:
    case PrerenderFinalStatus::kStop:
    case PrerenderFinalStatus::kSslCertificateError:
    case PrerenderFinalStatus::kLoginAuthRequested:
    case PrerenderFinalStatus::kUaChangeRequiresReload:
    case PrerenderFinalStatus::kBlockedByClient:
    case PrerenderFinalStatus::kMixedContent:
    case PrerenderFinalStatus::kTriggerBackgrounded:
    case PrerenderFinalStatus::kMemoryLimitExceeded:
    case PrerenderFinalStatus::kDataSaverEnabled:
    case PrerenderFinalStatus::kTriggerUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kActivatedBeforeStarted:
    case PrerenderFinalStatus::kInactivePageRestriction:
    case PrerenderFinalStatus::kStartFailed:
    case PrerenderFinalStatus::kTimeoutBackgrounded:
    case PrerenderFinalStatus::kCrossSiteRedirectInInitialNavigation:
    case PrerenderFinalStatus::kCrossSiteNavigationInInitialNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInInitialNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInInitialNavigation:
    case PrerenderFinalStatus::kActivationNavigationParameterMismatch:
    case PrerenderFinalStatus::kActivatedInBackground:
    case PrerenderFinalStatus::kActivationNavigationDestroyedBeforeSuccess:
      return true;

    // The associated tab is closed.
    case PrerenderFinalStatus::kTabClosedByUserGesture:
    case PrerenderFinalStatus::kTabClosedWithoutUserGesture:
      return false;

    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessCrashed:
    case PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled:
    case PrerenderFinalStatus::kActivationFramePolicyNotCompatible:
    case PrerenderFinalStatus::kPreloadingDisabled:
    case PrerenderFinalStatus::kBatterySaverEnabled:
    case PrerenderFinalStatus::kActivatedDuringMainFrameNavigation:
    case PrerenderFinalStatus::kPreloadingUnsupportedByWebContents:
    case PrerenderFinalStatus::kCrossSiteRedirectInMainFrameNavigation:
    case PrerenderFinalStatus::kCrossSiteNavigationInMainFrameNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginRedirectNotOptInInMainFrameNavigation:
    case PrerenderFinalStatus::
        kSameSiteCrossOriginNavigationNotOptInInMainFrameNavigation:
    case PrerenderFinalStatus::kMemoryPressureOnTrigger:
    case PrerenderFinalStatus::kMemoryPressureAfterTriggered:
    case PrerenderFinalStatus::kPrerenderingDisabledByDevTools:
      return true;

    // This is used for speculation rules, not for embedder triggers.
    case PrerenderFinalStatus::kSpeculationRuleRemoved:
      NOTREACHED();

    case PrerenderFinalStatus::kActivatedWithAuxiliaryBrowsingContexts:
      return true;

    // These are used for speculation rules, not for embedder triggers.
    case PrerenderFinalStatus::kMaxNumOfRunningImmediatePrerendersExceeded:
    case PrerenderFinalStatus::kMaxNumOfRunningNonImmediatePrerendersExceeded:
      NOTREACHED();

    case PrerenderFinalStatus::kMaxNumOfRunningEmbedderPrerendersExceeded:
    case PrerenderFinalStatus::kPrerenderingUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kRedirectedPrerenderingUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kActivationUrlHasEffectiveUrl:
    case PrerenderFinalStatus::kJavaScriptInterfaceAdded:
    case PrerenderFinalStatus::kJavaScriptInterfaceRemoved:
    case PrerenderFinalStatus::kAllPrerenderingCanceled:
      return true;

    // window.close() is called in a prerendered page.
    case PrerenderFinalStatus::kWindowClosed:
      return false;

    case PrerenderFinalStatus::kSlowNetwork:
      return true;

    case PrerenderFinalStatus::kOtherPrerenderedPageActivated:
      return false;

    case PrerenderFinalStatus::kPrerenderFailedDuringPrefetch:
      return true;

    // Prerendering is intentionally canceled by the Delete Browsing Data
    // option or with Clear-Site-Data response headers.
    case PrerenderFinalStatus::kBrowsingDataRemoved:
      return false;
    // The PrerenderHost is reused by another prerender request.
    case PrerenderFinalStatus::kPrerenderHostReused:
      return false;
    case PrerenderFinalStatus::kFormSubmitWhenPrerendering:
    case PrerenderFinalStatus::kCrossDocumentRestart:
      return false;
  }
}

}  // namespace

PrerenderHandleImpl::PrerenderHandleImpl(
    base::WeakPtr<PrerenderHostRegistry> prerender_host_registry,
    PrerenderHostId prerender_host_id,
    const GURL& prerendering_url,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint)
    : prerender_host_id_(prerender_host_id),
      prerender_host_registry_(std::move(prerender_host_registry)),
      prerendering_url_(prerendering_url),
      no_vary_search_hint_(std::move(no_vary_search_hint)) {
  CHECK(!prerendering_url_.is_empty());
  // PrerenderHandleImpl is now designed only for embedder triggers. If you use
  // this handle for other triggers, please make sure to update the logging etc.
  auto* prerender_host =
      prerender_host_registry_->FindNonReservedHostById(prerender_host_id_);
  CHECK(prerender_host);
  CHECK_EQ(prerender_host->trigger_type(), PreloadingTriggerType::kEmbedder);
  obs_.Observe(prerender_host);
}

PrerenderHandleImpl::~PrerenderHandleImpl() {
  if (prerender_host_registry_) {
    prerender_host_registry_->CancelHost(
        prerender_host_id_, PrerenderFinalStatus::kTriggerDestroyed);
  }
}

PrerenderHostId PrerenderHandleImpl::GetPrerenderHostId() const {
  return prerender_host_id_;
}

const GURL& PrerenderHandleImpl::GetInitialPrerenderingUrl() const {
  return prerendering_url_;
}

const std::optional<net::HttpNoVarySearchData>&
PrerenderHandleImpl::GetNoVarySearchHint() const {
  return no_vary_search_hint_;
}

base::WeakPtr<PrerenderHandle> PrerenderHandleImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrerenderHandleImpl::SetPreloadingAttemptFailureReason(
    PreloadingFailureReason reason) {
  auto* prerender_host = obs_.GetSource();
  if (!prerender_host || !prerender_host->preloading_attempt()) {
    return;
  }
  prerender_host->preloading_attempt()->SetFailureReason(reason);
}

void PrerenderHandleImpl::AddActivationCallback(
    base::OnceClosure activation_callback) {
  CHECK(IsValid());
  CHECK(activation_callback);
  activation_callbacks_.push_back(std::move(activation_callback));
}

void PrerenderHandleImpl::AddErrorCallback(base::OnceClosure error_callback) {
  CHECK(IsValid());
  CHECK(error_callback);
  error_callbacks_.push_back(std::move(error_callback));
}

bool PrerenderHandleImpl::IsValid() const {
  switch (state_) {
    case State::kLoading:
    case State::kReady:
      return true;
    case State::kActivated:
    case State::kCanceled:
      return false;
  }
}

bool PrerenderHandleImpl::IsWaitingForResponseHeaders() const {
  CHECK(IsValid());
  return state_ == State::kLoading;
}

void PrerenderHandleImpl::AddOnResponseHeadersReceivedCallback(
    base::OnceClosure callback) {
  CHECK(IsWaitingForResponseHeaders());
  on_headers_received_callbacks_.push_back(std::move(callback));
}

void PrerenderHandleImpl::OnActivated() {
  CHECK_EQ(State::kReady, state_);
  state_ = State::kActivated;

  // An error should not be reported after activation.
  error_callbacks_.clear();

  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(activation_callbacks_);
  // Don't touch `this` after this line, as a callback could destroy `this`.
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void PrerenderHandleImpl::OnFailed(PrerenderFinalStatus status) {
  // The prerender page can either be activated or fail.
  // However an activated page will never receive this callback.
  CHECK(IsValid());
  state_ = State::kCanceled;

  // An activation never happen after cancellation.
  activation_callbacks_.clear();

  // Call the header callbacks to unthrottle other requests anyway.
  // If the header has already been received, on_headers_received_callbacks_
  // will be empty.
  for (auto& callback : on_headers_received_callbacks_) {
    std::move(callback).Run();
  }
  on_headers_received_callbacks_.clear();

  if (!ShouldFireErrorCallback(status)) {
    error_callbacks_.clear();
    return;
  }

  // TODO(crbug.com/41490450): Pass a cancellation reason to the callback.
  // Note that we should not expose detailed reasons to prevent embedders from
  // depending on them. Such an implicit contract with embedders would impair
  // flexibility of internal implementation.
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(error_callbacks_);
  // Don't touch `this` after this line, as a callback could destroy `this`.
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void PrerenderHandleImpl::OnHostDestroyed(PrerenderFinalStatus status) {
  obs_.Reset();
}

void PrerenderHandleImpl::OnHeadersReceived(
    NavigationHandle& navigation_handle) {
  // There is a small chance that the headers received callback
  // will be called for a cancelled prerender host because the
  // deferred destruction of the PrerenderHost.
  if (state_ == State::kCanceled) {
    return;
  }
  CHECK_EQ(state_, State::kLoading);
  state_ = State::kReady;

  for (auto& callback : on_headers_received_callbacks_) {
    std::move(callback).Run();
  }
  on_headers_received_callbacks_.clear();
}

}  // namespace content
