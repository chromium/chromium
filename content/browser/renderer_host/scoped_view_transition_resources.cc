// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/scoped_view_transition_resources.h"

#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_process_host.h"

namespace content {

ScopedViewTransitionResources::ScopedViewTransitionResources(
    const blink::ViewTransitionToken& transition_token,
    RenderProcessHost& render_process_host,
    bool delay_layer_tree_view_deletion)
    : transition_token_(transition_token),
      render_process_host_id_(render_process_host.GetID()),
      delay_layer_tree_view_deletion_(
          std::move(delay_layer_tree_view_deletion)) {
  GetHostFrameSinkManager()->SetViewTransitionResourcesCapturedCallback(
      transition_token,
      base::BindOnce(
          &ScopedViewTransitionResources::OnViewTransitionResourcesCaptured,
          weak_factory_.GetWeakPtr()));
}

void ScopedViewTransitionResources::OnViewTransitionResourcesCaptured() {
  is_resources_captured_ = true;

  CancelDelayProcessShutdown();
}

void ScopedViewTransitionResources::CancelDelayProcessShutdown() {
  process_shutdown_delay_runner_.RunAndReset();
}

void ScopedViewTransitionResources::MaybeDelayProcessShutdown(
    const base::TimeDelta& shutdown_delay,
    RenderFrameHostImpl& render_frame_host) {
  if (process_shutdown_delay_runner_ || is_resources_captured_ ||
      render_frame_host.GetProcess()->GetID() != render_process_host_id_) {
    return;
  }

  process_shutdown_delay_runner_ =
      render_frame_host.GetProcess()->DelayProcessShutdown(
          shutdown_delay, base::TimeDelta(),
          render_frame_host.GetSiteInstance()->GetSiteInfo());
}

ScopedViewTransitionResources::~ScopedViewTransitionResources() {
  CancelDelayProcessShutdown();
  GetHostFrameSinkManager()->ClearUnclaimedViewTransitionResources(
      transition_token_);
}

}  // namespace content
