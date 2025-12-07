// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/data_sharing_tab_helper.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/collaboration/public/collaboration_flow_entry_point.h"
#import "components/data_sharing/public/data_sharing_utils.h"
#import "ios/chrome/browser/collaboration/model/data_sharing_tab_helper_delegate.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Return whether the navigation should be handled if it is a share URL.
bool ShouldHandleShareURLNavigation(
    web::WebStatePolicyDecider::RequestInfo request_info) {
  // Make sure to keep it in sync between platforms.
  // LINT.IfChange(ShouldHandleShareURLNavigation)
  if (!request_info.target_frame_is_main) {
    return false;
  }

  if (request_info.is_user_initiated && !request_info.user_tapped_recently) {
    return false;
  }

  return true;
  // LINT.ThenChange(/chrome/browser/data_sharing/data_sharing_navigation_throttle.cc:ShouldHandleShareURLNavigation)
}

// Closes `weak_web_state` if the url interception ended up with an empty page.
void CloseWebStateIfEmptyPage(base::WeakPtr<web::WebState> weak_web_state) {
  web::WebState* web_state = weak_web_state.get();
  if (!web_state || web_state->IsBeingDestroyed()) {
    return;
  }

  const GURL& url = web_state->GetLastCommittedURL();
  if (!url.is_valid() || url.IsAboutBlank() || url.is_empty()) {
    // This will destroy the WebState, so the `web_state` and `url` will
    // become invalid after this line and they must not be used anymore.
    return web_state->CloseWebState();
  }
}

// Returns a closure that invokes `CloseWebStateIfEmptyPage` with `web_state`.
base::OnceClosure CloseWebStateIfEmptyPageClosure(web::WebState* web_state) {
  return base::BindOnce(&CloseWebStateIfEmptyPage, web_state->GetWeakPtr());
}

}  // namespace

DataSharingTabHelper::DataSharingTabHelper(web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state) {}

DataSharingTabHelper::~DataSharingTabHelper() = default;

void DataSharingTabHelper::SetDelegate(DataSharingTabHelperDelegate* delegate) {
  delegate_ = delegate;
}

void DataSharingTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    web::WebStatePolicyDecider::RequestInfo request_info,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  const GURL url = net::GURLWithNSURL(request.URL);
  if (!ShouldInterceptRequestforUrl(url)) {
    std::move(callback).Run(PolicyDecision::Allow());
    return;
  }

  // Invoking `callback` may destroy the WebState and thus the current
  // object. To avoid a crash (see https://crbug.com/434172545 for more
  // information) create a callback that will try to close the WebState
  // (if it still exists) and chain it with `callback`.
  callback =
      std::move(callback).Then(CloseWebStateIfEmptyPageClosure(web_state()));

  if (ShouldHandleShareURLNavigation(request_info)) {
    delegate_->HandleShareURLNavigationIntercepted(
        url, collaboration::GetEntryPointFromPageTransition(
                 request_info.transition_type));
  }

  // From this point, the code must not access `this` as it may have been
  // destroyed. It is okay to use `callback` since it is a local variable.
  std::move(callback).Run(PolicyDecision::Cancel());
}

bool DataSharingTabHelper::ShouldInterceptRequestforUrl(const GURL& url) const {
  // Cannot intercept a request if there is no delegate installed or if
  // joining shared tab groups is not allowed.
  if (!delegate_ || !delegate_->IsAllowedToJoinSharedTabGroups()) {
    return false;
  }

  // For data_sharing::DataSharingUtils::ShouldInterceptNavigationForShareURL.
  using data_sharing::DataSharingUtils;

  // Only intercept data sharing urls.
  return DataSharingUtils::ShouldInterceptNavigationForShareURL(url);
}
