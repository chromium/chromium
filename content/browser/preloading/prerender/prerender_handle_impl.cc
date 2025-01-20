// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_handle_impl.h"

#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "url/gurl.h"

namespace content {

PrerenderHandleImpl::PrerenderHandleImpl(
    base::WeakPtr<PrerenderHostRegistry> prerender_host_registry,
    FrameTreeNodeId frame_tree_node_id,
    const GURL& prerendering_url)
    : prerender_host_registry_(std::move(prerender_host_registry)),
      frame_tree_node_id_(frame_tree_node_id),
      prerendering_url_(prerendering_url) {
  CHECK(!prerendering_url_.is_empty());
  // PrerenderHandleImpl is now designed only for embedder triggers. If you use
  // this handle for other triggers, please make sure to update the logging etc.
  auto* prerender_host = GetPrerenderHost();
  CHECK(prerender_host);
  CHECK_EQ(prerender_host->trigger_type(), PreloadingTriggerType::kEmbedder);
  prerender_host->AddObserver(this);
}

PrerenderHandleImpl::~PrerenderHandleImpl() {
  PrerenderHost* prerender_host = GetPrerenderHost();
  if (!prerender_host) {
    return;
  }
  prerender_host->RemoveObserver(this);

  prerender_host_registry_->CancelHost(frame_tree_node_id_,
                                       PrerenderFinalStatus::kTriggerDestroyed);
}

const GURL& PrerenderHandleImpl::GetInitialPrerenderingUrl() const {
  return prerendering_url_;
}

base::WeakPtr<PrerenderHandle> PrerenderHandleImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrerenderHandleImpl::SetPreloadingAttemptFailureReason(
    PreloadingFailureReason reason) {
  auto* prerender_host = GetPrerenderHost();
  if (!prerender_host || !prerender_host->preloading_attempt()) {
    return;
  }
  prerender_host->preloading_attempt()->SetFailureReason(reason);
}

void PrerenderHandleImpl::SetActivationCallback(
    base::OnceClosure activation_callback) {
  CHECK(activation_callback);
  CHECK(!activation_callback_);
  if (was_activated_) {
    std::move(activation_callback).Run();
    return;
  }
  activation_callback_ = std::move(activation_callback);
}

void PrerenderHandleImpl::SetErrorCallback(base::OnceClosure error_callback) {
  CHECK(error_callback);
  CHECK(!error_callback_);
  error_callback_ = std::move(error_callback);
}

void PrerenderHandleImpl::OnActivated() {
  CHECK(!was_activated_);
  was_activated_ = true;

  // An error should not be reported after activation.
  error_callback_.Reset();

  if (activation_callback_) {
    std::move(activation_callback_).Run();
  }
}

void PrerenderHandleImpl::OnFailed(PrerenderFinalStatus status) {
  if (!error_callback_) {
    return;
  }

  // TODO(crbug.com/41490450): Don't fire the callback when prerendering is
  // intentionally canceled by an app (e.g., calling the cancellation API).

  // TODO(crbug.com/41490450): Pass a cancellation reason to the callback.
  // Note that we should not expose detailed reasons to prevent embedders from
  // depending on them. Such an implicit contract with embedders would impair
  // flexibility of internal implementation.
  std::move(error_callback_).Run();
}

PrerenderHost* PrerenderHandleImpl::GetPrerenderHost() {
  auto* prerender_frame_tree_node =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id_);
  if (!prerender_frame_tree_node) {
    return nullptr;
  }
  PrerenderHost& prerender_host =
      PrerenderHost::GetFromFrameTreeNode(*prerender_frame_tree_node);
  return &prerender_host;
}

}  // namespace content
