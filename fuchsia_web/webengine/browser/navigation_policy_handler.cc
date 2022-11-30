// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/navigation_policy_handler.h"

#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "content/public/browser/navigation_handle.h"
#include "fuchsia_web/webengine/browser/navigation_policy_throttle.h"

NavigationPolicyHandler::NavigationPolicyHandler(
    fuchsia::web::NavigationPolicyProviderParams params,
    fidl::InterfaceHandle<fuchsia::web::NavigationPolicyProvider> delegate)
    : params_(std::move(params)), provider_(delegate.Bind()) {
  provider_.set_error_handler(fit::bind_member(
      this, &NavigationPolicyHandler::OnNavigationPolicyProviderDisconnected));
}

NavigationPolicyHandler::~NavigationPolicyHandler() {
  for (auto* throttle : navigation_throttles_) {
    throttle->OnNavigationPolicyProviderDisconnected(
        content::NavigationThrottle::CANCEL);
  }
  navigation_throttles_.clear();
}

void NavigationPolicyHandler::RegisterNavigationThrottle(
    NavigationPolicyThrottle* navigation_throttle) {
  navigation_throttles_.insert(navigation_throttle);
}

void NavigationPolicyHandler::RemoveNavigationThrottle(
    NavigationPolicyThrottle* navigation_throttle) {
  navigation_throttles_.erase(navigation_throttle);
}

void NavigationPolicyHandler::EvaluateRequestedNavigation(
    fuchsia::web::RequestedNavigation requested_navigation,
    fuchsia::web::NavigationPolicyProvider::EvaluateRequestedNavigationCallback
        callback) {
  provider_->EvaluateRequestedNavigation(std::move(requested_navigation),
                                         std::move(callback));
}

bool NavigationPolicyHandler::ShouldEvaluateNavigation(
    content::NavigationHandle* handle,
    fuchsia::web::NavigationPhase phase) {
  if (handle->IsInMainFrame()) {
    return (phase & params_.main_frame_phases()) == phase;
  }

  return (phase & params_.subframe_phases()) == phase;
}

bool NavigationPolicyHandler::is_provider_connected() {
  return provider_.is_bound();
}

void NavigationPolicyHandler::OnNavigationPolicyProviderDisconnected(
    zx_status_t status) {
  ZX_LOG(ERROR, status) << "NavigationPolicyProvider disconnected";
  for (auto* throttle : navigation_throttles_) {
    throttle->OnNavigationPolicyProviderDisconnected(
        content::NavigationThrottle::CANCEL);
  }
  navigation_throttles_.clear();
}
