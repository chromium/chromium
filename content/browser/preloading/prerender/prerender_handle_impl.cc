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
  auto* prerender_host =
      prerender_host_registry_->FindNonReservedHostById(frame_tree_node_id);
  CHECK(prerender_host);
  CHECK_EQ(prerender_host->trigger_type(), PreloadingTriggerType::kEmbedder);
}

PrerenderHandleImpl::~PrerenderHandleImpl() {
  if (prerender_host_registry_) {
    prerender_host_registry_->CancelHost(
        frame_tree_node_id_, PrerenderFinalStatus::kTriggerDestroyed);
  }
}

const GURL& PrerenderHandleImpl::GetInitialPrerenderingUrl() const {
  return prerendering_url_;
}

base::WeakPtr<PrerenderHandle> PrerenderHandleImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrerenderHandleImpl::SetPreloadingAttemptFailureReason(
    PreloadingFailureReason reason) {
  if (!prerender_host_registry_)
    return;
  auto* prerender_host =
      prerender_host_registry_->FindNonReservedHostById(frame_tree_node_id_);
  if (!prerender_host) {
    return;
  }
  if (!prerender_host->preloading_attempt()) {
    return;
  }
  prerender_host->preloading_attempt()->SetFailureReason(reason);
}

}  // namespace content
