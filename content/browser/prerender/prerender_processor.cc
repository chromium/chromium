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

  // The prerender request from a page being prerendered should be deferred
  // until activation by the Mojo capability control.
  DCHECK_NE(RenderFrameHostImpl::LifecycleStateImpl::kPrerendering,
            initiator_render_frame_host.lifecycle_state());

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
  DCHECK_EQ(state_, State::kInitial);
  state_ = State::kStarted;

  if (!registry_)
    return;

  // TODO(https://crbug.com/1176054): This is a tentative behavior. We plan to
  // support cross-origin prerendering later.
  CHECK(
      initiator_origin_.IsSameOriginWith(url::Origin::Create(attributes->url)));
  CHECK(attributes->url.SchemeIsHTTPOrHTTPS());
  CHECK_EQ(attributes->trigger_type,
           blink::mojom::PrerenderTriggerType::kSpeculationRule);

  prerender_frame_tree_node_id_ = registry_->CreateAndStartHost(
      std::move(attributes), initiator_render_frame_host_);
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
  registry_->OnTriggerDestroyed(prerender_frame_tree_node_id_);
}

}  // namespace content
