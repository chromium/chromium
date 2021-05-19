// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_processor.h"

#include "base/feature_list.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderProcessor::PrerenderProcessor(
    RenderFrameHostImpl& initiator_render_frame_host)
    : initiator_render_frame_host_(initiator_render_frame_host),
      initiator_origin_(initiator_render_frame_host.GetLastCommittedOrigin()),
      registry_(
          initiator_render_frame_host.delegate()->GetPrerenderHostRegistry()) {
  DCHECK(blink::features::IsPrerender2Enabled());
  observation_.Observe(registry_);
}

PrerenderProcessor::~PrerenderProcessor() {
  if (state_ == State::kStarted)
    CancelPrerendering();
}

// TODO(https://crbug.com/1132746): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
void PrerenderProcessor::Start(
    blink::mojom::PrerenderAttributesPtr attributes) {
  // Start() must be called only one time.
  if (state_ != State::kInitial) {
    mojo::ReportBadMessage("PP_START_TWICE");
    return;
  }
  state_ = State::kStarted;

  // Abort cross-origin prerendering.
  // TODO(https://crbug.com/1176054): This is a tentative behavior. We plan to
  // support cross-origin prerendering later.
  if (!initiator_origin_.IsSameOriginWith(
          url::Origin::Create(attributes->url))) {
    mojo::ReportBadMessage("PP_CROSS_ORIGIN");
    return;
  }

  // Prerendering is only supported for <link rel=prerender>.
  // We may want to support it for <link rel=next> if NoStatePrefetch re-enables
  // it again. See https://crbug.com/1161545.
  switch (attributes->trigger_type) {
    case blink::mojom::PrerenderTriggerType::kLinkRelPrerender:
      break;
    case blink::mojom::PrerenderTriggerType::kLinkRelNext:
      return;
  }

  // TODO(https://crbug.com/1132746): Validate the origin, etc and send
  // mojo::ReportBadMessage() if necessary like
  // `NoStatePrefetchProcessorImpl::Start()`.

  // TODO(https://crbug.com/1138711, https://crbug.com/1138723): Abort if the
  // initiator frame is not the main frame (i.e., iframe or pop-up window).

  // The origin may have changed if a same-site navigation occurred in the frame
  // after the PrerenderProcessor was created.
  if (initiator_render_frame_host_.GetLastCommittedOrigin() !=
      initiator_origin_) {
    return;
  }

  // Report bad message if asked to prerender webUI.
  std::string scheme = attributes->url.scheme();
  const auto& webui_schemes = URLDataManagerBackend::GetWebUISchemes();
  if (base::Contains(webui_schemes, scheme)) {
    mojo::ReportBadMessage("PP_WEBUI");
    return;
  }

  if (!registry_)
    return;
  prerender_frame_tree_node_id_ = registry_->CreateAndStartHost(
      std::move(attributes), initiator_render_frame_host_);
}

void PrerenderProcessor::Cancel() {
  // Cancel() must be called after Start().
  if (state_ != State::kStarted) {
    mojo::ReportBadMessage("PP_CANCEL_BEFORE_START");
    return;
  }
  CancelPrerendering();
}

void PrerenderProcessor::OnRegistryDestroyed() {
  DCHECK(registry_);
  registry_ = nullptr;
  observation_.Reset();
}

void PrerenderProcessor::CancelPrerendering() {
  TRACE_EVENT0("navigation", "PrerenderProcessor::CancelPrerendering");
  DCHECK_EQ(state_, State::kStarted);
  state_ = State::kCancelled;

  if (!registry_)
    return;
  registry_->AbandonHost(prerender_frame_tree_node_id_);
}

}  // namespace content
