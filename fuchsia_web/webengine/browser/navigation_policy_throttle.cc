// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/navigation_policy_throttle.h"

#include "content/public/browser/navigation_handle.h"
#include "fuchsia_web/webengine/browser/navigation_policy_handler.h"

namespace {

fuchsia::web::RequestedNavigation ToRequestedNavigation(
    content::NavigationHandle* handle,
    fuchsia::web::NavigationPhase phase) {
  fuchsia::web::RequestedNavigation event;
  event.set_id(static_cast<uint64_t>(handle->GetNavigationId()));
  event.set_phase(phase);
  event.set_is_main_frame(handle->IsInMainFrame());
  event.set_is_same_document(handle->IsSameDocument());
  event.set_is_http_post(handle->IsPost());
  event.set_url(handle->GetURL().spec());
  event.set_has_gesture(handle->HasUserGesture());
  event.set_was_server_redirect(handle->WasServerRedirect());

  return event;
}

}  // namespace

NavigationPolicyThrottle::NavigationPolicyThrottle(
    content::NavigationHandle* handle,
    NavigationPolicyHandler* policy_handler)
    : NavigationThrottle(handle),
      policy_handler_(policy_handler),
      navigation_handle_(handle) {
  if (policy_handler->is_provider_connected()) {
    policy_handler_->RegisterNavigationThrottle(this);
  } else {
    policy_handler_ = nullptr;
  }
}

NavigationPolicyThrottle::~NavigationPolicyThrottle() {
  if (policy_handler_)
    policy_handler_->RemoveNavigationThrottle(this);
}

void NavigationPolicyThrottle::OnNavigationPolicyProviderDisconnected(
    content::NavigationThrottle::ThrottleCheckResult check_result) {
  policy_handler_ = nullptr;

  if (is_paused_) {
    is_paused_ = false;
    CancelDeferredNavigation(check_result);
    // DO NOT ADD CODE after this. The callback above will destroy the
    // NavigationHandle that owns this NavigationThrottle.
  }
}

void NavigationPolicyThrottle::OnRequestedNavigationEvaluated(
    fuchsia::web::NavigationDecision decision) {
  DCHECK(is_paused_);
  is_paused_ = false;

  switch (decision.Which()) {
    case fuchsia::web::NavigationDecision::kProceed:
      Resume();
      // DO NOT ADD CODE after this. The callback above might have destroyed
      // the NavigationHandle that owns this NavigationThrottle.
      break;
    case fuchsia::web::NavigationDecision::kAbort:
      CancelDeferredNavigation(content::NavigationThrottle::CANCEL);
      // DO NOT ADD CODE after this. The callback above will destroy the
      // NavigationHandle that owns this NavigationThrottle.
      break;
    default:
      NOTREACHED();
  }
}

content::NavigationThrottle::ThrottleCheckResult
NavigationPolicyThrottle::WillStartRequest() {
  return HandleNavigationPhase(fuchsia::web::NavigationPhase::START);
}

content::NavigationThrottle::ThrottleCheckResult
NavigationPolicyThrottle::WillRedirectRequest() {
  return HandleNavigationPhase(fuchsia::web::NavigationPhase::REDIRECT);
}

content::NavigationThrottle::ThrottleCheckResult
NavigationPolicyThrottle::WillFailRequest() {
  return HandleNavigationPhase(fuchsia::web::NavigationPhase::FAIL);
}

content::NavigationThrottle::ThrottleCheckResult
NavigationPolicyThrottle::WillProcessResponse() {
  return HandleNavigationPhase(fuchsia::web::NavigationPhase::PROCESS_RESPONSE);
}

const char* NavigationPolicyThrottle::GetNameForLogging() {
  return "NavigationPolicyThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
NavigationPolicyThrottle::HandleNavigationPhase(
    fuchsia::web::NavigationPhase phase) {
  DCHECK(!is_paused_);

  if (!policy_handler_) {
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::CANCEL);
  }

  if (!policy_handler_->ShouldEvaluateNavigation(navigation_handle_, phase)) {
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::PROCEED);
  }

  policy_handler_->EvaluateRequestedNavigation(
      ToRequestedNavigation(navigation_handle_, phase),
      [weak_this = weak_factory_.GetWeakPtr()](auto decision) {
        if (weak_this)
          weak_this->OnRequestedNavigationEvaluated(std::move(decision));
      });

  is_paused_ = true;
  return content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::DEFER);
}
