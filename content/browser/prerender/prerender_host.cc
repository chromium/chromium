// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/feature_list.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderHost::PrerenderHost(
    blink::mojom::PrerenderAttributesPtr attributes,
    const GlobalFrameRoutingId& initiator_render_frame_host_id,
    const url::Origin& initiator_origin)
    : attributes_(std::move(attributes)),
      initiator_render_frame_host_id_(initiator_render_frame_host_id),
      initiator_origin_(initiator_origin) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2));
}

// TODO(https://crbug.com/1132746): Abort ongoing prerendering and notify the
// mojo capability controller in the destructor.
PrerenderHost::~PrerenderHost() = default;

// TODO(https://crbug.com/1132746): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
void PrerenderHost::StartPrerendering() {
  // The initiator render frame host may have already been closed. In that case,
  // abort the prerendering request.
  auto* initiator_render_frame_host =
      RenderFrameHostImpl::FromID(initiator_render_frame_host_id_);
  if (!initiator_render_frame_host)
    return;

  // TODO(https://crbug.com/1138711, https://crbug.com/1138723): Abort if the
  // initiator frame is not the main frame (i.e., iframe or pop-up window).

  // Create a new WebContents for prerendering.
  WebContents::CreateParams web_contents_params(
      initiator_render_frame_host->GetBrowserContext());
  // TODO(https://crbug.com/1132746): Set up other fields of
  // `web_contents_params` as well, and add tests for them.
  prerendered_contents_ = WebContents::Create(web_contents_params);

  // Observe events about the prerendering contents.
  Observe(prerendered_contents_.get());

  // Start prerendering navigation.
  NavigationController::LoadURLParams load_url_params(attributes_->url);
  load_url_params.initiator_origin = initiator_origin_;
  // TODO(https://crbug.com/1132746): Set up other fields of `load_url_params`
  // as well, and add tests for them.
  prerendered_contents_->GetController().LoadURLWithParams(load_url_params);
}

void PrerenderHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  // The prerendered contents are considered ready for activation when it
  // reaches DidFinishNavigation.
  DCHECK(!is_ready_for_activation_);
  is_ready_for_activation_ = true;

  // Stop observing the events about the prerendered contents.
  Observe(nullptr);
}

bool PrerenderHost::ActivatePrerenderedContents(
    RenderFrameHostImpl& current_render_frame_host) {
  DCHECK(is_ready_for_activation_);
  is_ready_for_activation_ = false;

  auto* current_web_contents =
      WebContents::FromRenderFrameHost(&current_render_frame_host);
  if (!current_web_contents)
    return false;

  // Activate the prerendered contents.
  WebContentsDelegate* delegate = current_web_contents->GetDelegate();
  DCHECK(delegate);
  DCHECK(prerendered_contents_);
  // Tentatively use Portal's activation function.
  // TODO(https://crbug.com/1132746): Replace this with the MPArch.
  std::unique_ptr<WebContents> predecessor_web_contents =
      delegate->ActivatePortalWebContents(current_web_contents,
                                          std::move(prerendered_contents_));
  // Stop loading on the predecessor WebContents.
  predecessor_web_contents->Stop();

  // TODO(https://crbug.com/1132752): Notify the mojo capability controller that
  // the prerendered contents get activated.

  return true;
}

}  // namespace content
