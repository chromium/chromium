// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/interstitial_page_navigator_impl.h"

#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

InterstitialPageNavigatorImpl::InterstitialPageNavigatorImpl(
    InterstitialPageImpl* interstitial,
    NavigationControllerImpl* navigation_controller)
    : interstitial_(interstitial),
      controller_(navigation_controller),
      enabled_(true) {}

InterstitialPageNavigatorImpl::~InterstitialPageNavigatorImpl() {}

NavigatorDelegate* InterstitialPageNavigatorImpl::GetDelegate() {
  return interstitial_;
}

NavigationController* InterstitialPageNavigatorImpl::GetController() {
  return controller_;
}

void InterstitialPageNavigatorImpl::DidNavigate(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& input_params,
    std::unique_ptr<NavigationRequest> navigation_request,
    bool was_within_same_document) {
  // Do not proceed if the interstitial itself has been disabled.
  if (!enabled_)
    return;

  navigation_request->DidCommitNavigation(input_params, true, false, GURL(),
                                          NAVIGATION_TYPE_NEW_PAGE);
  navigation_request.reset();

  // TODO(nasko): Move implementation here, but for the time being call out
  // to the interstitial page code.
  interstitial_->DidNavigate(render_frame_host->render_view_host(),
                             input_params);
}

void InterstitialPageNavigatorImpl::Disable() {
  enabled_ = false;

  // This is no longer safe to access.
  controller_ = nullptr;
}

}  // namespace content
