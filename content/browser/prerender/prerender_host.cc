// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderHost::PrerenderHost(blink::mojom::PrerenderAttributesPtr attributes,
                             const url::Origin& initiator_origin,
                             BrowserContext& browser_context)
    : attributes_(std::move(attributes)), initiator_origin_(initiator_origin) {
  DCHECK(blink::features::IsPrerender2Enabled());
  // Create a new WebContents for prerendering.
  WebContents::CreateParams web_contents_params(&browser_context);
  // TODO(https://crbug.com/1132746): Set up other fields of
  // `web_contents_params` as well, and add tests for them.
  prerendered_contents_ = WebContents::Create(web_contents_params);
  frame_tree_node_id_ =
      prerendered_contents_->GetMainFrame()->GetFrameTreeNodeId();
}

// TODO(https://crbug.com/1132746): Abort ongoing prerendering and notify the
// mojo capability controller in the destructor.
PrerenderHost::~PrerenderHost() {
  if (!final_status_)
    RecordFinalStatus(FinalStatus::kDestroyed);
}

// TODO(https://crbug.com/1132746): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
void PrerenderHost::StartPrerendering() {
  TRACE_EVENT0("navigation", "PrerenderHost::StartPrerendering");
  // Observe events about the prerendering contents.
  Observe(prerendered_contents_.get());

  // Start prerendering navigation.
  NavigationController::LoadURLParams load_url_params(attributes_->url);
  load_url_params.initiator_origin = initiator_origin_;
  load_url_params.is_prerendering = true;
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
  TRACE_EVENT1("navigation", "PrerenderHost::ActivatePrerenderedContents",
               "render_frame_host",
               base::trace_event::ToTracedValue(&current_render_frame_host));
  DCHECK_EQ(blink::features::kPrerender2Param.Get(),
            blink::features::Prerender2ActivationMode::kEnabled);

  DCHECK(is_ready_for_activation_);
  is_ready_for_activation_ = false;

  auto* current_web_contents =
      WebContents::FromRenderFrameHost(&current_render_frame_host);
  if (!current_web_contents)
    return false;

  // Merge browsing history.
  prerendered_contents_->GetController().CopyStateFromAndPrune(
      &current_web_contents->GetController(), /*replace_entry=*/false);

  // Activate the prerendered contents.
  WebContentsDelegate* delegate = current_web_contents->GetDelegate();
  DCHECK(delegate);
  DCHECK(prerendered_contents_);
  static_cast<RenderFrameHostImpl*>(prerendered_contents_->GetMainFrame())
      ->OnPrerenderedPageActivated();
  // Tentatively use Portal's activation function.
  // TODO(https://crbug.com/1132746): Replace this with the MPArch.
  std::unique_ptr<WebContents> predecessor_web_contents =
      delegate->ActivatePortalWebContents(current_web_contents,
                                          std::move(prerendered_contents_));

  // Stop loading on the predecessor WebContents.
  predecessor_web_contents->Stop();

  // TODO(https://crbug.com/1142658): Notify renderer processes that the
  // contents get activated.

  // TODO(https://crbug.com/1132752): Notify the mojo capability controller that
  // the prerendered contents get activated.

  RecordFinalStatus(FinalStatus::kActivated);
  return true;
}

RenderFrameHostImpl* PrerenderHost::GetPrerenderedMainFrameHostForTesting() {
  DCHECK(prerendered_contents_);
  return static_cast<RenderFrameHostImpl*>(
      prerendered_contents_->GetMainFrame());
}

void PrerenderHost::RecordFinalStatus(FinalStatus status) {
  DCHECK(!final_status_);
  final_status_ = status;
  base::UmaHistogramEnumeration(
      "Prerender.Experimental.PrerenderHostFinalStatus", status);
}

const GURL& PrerenderHost::GetInitialUrl() const {
  return attributes_->url;
}

}  // namespace content
