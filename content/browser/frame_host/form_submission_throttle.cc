// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/form_submission_throttle.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

FormSubmissionThrottle::FormSubmissionThrottle(NavigationHandle* handle)
    : NavigationThrottle(handle) {}

// static
std::unique_ptr<NavigationThrottle>
FormSubmissionThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!handle->IsFormSubmission())
    return nullptr;

  return std::unique_ptr<NavigationThrottle>(
      new FormSubmissionThrottle(handle));
}

FormSubmissionThrottle::~FormSubmissionThrottle() {}

NavigationThrottle::ThrottleCheckResult
FormSubmissionThrottle::WillStartRequest() {
  return CheckContentSecurityPolicyFormAction(false /* was_server_redirect */);
}

NavigationThrottle::ThrottleCheckResult
FormSubmissionThrottle::WillRedirectRequest() {
  return CheckContentSecurityPolicyFormAction(true /* was_server_redirect */);
}

const char* FormSubmissionThrottle::GetNameForLogging() {
  return "FormSubmissionThrottle";
}

NavigationThrottle::ThrottleCheckResult
FormSubmissionThrottle::CheckContentSecurityPolicyFormAction(
    bool was_server_redirect) {
  // TODO(arthursonzogni): form-action is enforced on the wrong RenderFrameHost.
  // The navigating one is used instead of the one that has initiated the form
  // submission. The renderer side checks are still in place and are used for
  // the moment instead. For redirects, the behavior was already broken before
  // using the browser side checks.
  // See https://crbug.com/700964 and https://crbug.com/798698.
  //
  // In absence of redirects, the target URL is sufficiently verified against
  // the form-action CSP by the frame that hosts the form element + initiates
  // form submission + declares the form-action CSP (i.e. the same frame does
  // all those 4 things). Because in this scenario there are no frame or
  // renderer boundaries crossed, we don't have to worry about one (potentially
  // compromised) renderer being responsible for enforcing the CSP of another
  // (victim) renderer. Therefore it is okay to return early and do no further
  // browser-side checks.
  if (!was_server_redirect)
    return NavigationThrottle::PROCEED;

  NavigationRequest* request = NavigationRequest::From(navigation_handle());

  if (request->common_params().initiator_csp_info.should_check_main_world_csp ==
      CSPDisposition::DO_NOT_CHECK) {
    return NavigationThrottle::PROCEED;
  }

  const GURL& url = request->GetURL();

  // TODO(arthursonzogni): This is not the right RenderFrameHostImpl. The one
  // that has initiated the navigation must be used instead.
  // See https://crbug.com/700964
  RenderFrameHostImpl* render_frame =
      request->frame_tree_node()->current_frame_host();

  // TODO(estark): Move this check into NavigationRequest and split it into (1)
  // check report-only CSP, (2) upgrade request if needed, (3) check enforced
  // CSP to match how frame-src works. https://crbug.com/713388
  if (render_frame->IsAllowedByCsp(
          CSPDirective::FormAction, url, was_server_redirect,
          false /* is_response_check */,
          request->common_params().source_location.value_or(SourceLocation()),
          CSPContext::CHECK_ALL_CSP, true /* is_form_submission */)) {
    return NavigationThrottle::PROCEED;
  }

  return NavigationThrottle::CANCEL;
}

}  // namespace content
