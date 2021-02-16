// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_processor.h"

#include "base/feature_list.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderProcessor::PrerenderProcessor(
    RenderFrameHostImpl& initiator_render_frame_host)
    : initiator_render_frame_host_(initiator_render_frame_host),
      initiator_origin_(initiator_render_frame_host.GetLastCommittedOrigin()) {
  DCHECK(blink::features::IsPrerender2Enabled());
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
  switch (attributes->rel_type) {
    case blink::mojom::PrerenderRelType::kPrerender:
      break;
    case blink::mojom::PrerenderRelType::kNext:
      return;
  }

  // TODO(https://crbug.com/1132746): Validate the origin, etc and send
  // mojo::ReportBadMessage() if necessary like
  // `NoStatePrefetchProcessorImpl::Start()`.

  // TODO(https://crbug.com/1138711, https://crbug.com/1138723): Abort if the
  // initiator frame is not the main frame (i.e., iframe or pop-up window).

  auto* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(&initiator_render_frame_host_));

  if (!web_contents)
    return;

  prerender_frame_tree_node_id_ = GetPrerenderHostRegistry().CreateAndStartHost(
      std::move(attributes), *web_contents, initiator_origin_);
}

void PrerenderProcessor::Cancel() {
  // Cancel() must be called after Start().
  if (state_ != State::kStarted) {
    mojo::ReportBadMessage("PP_CANCEL_BEFORE_START");
    return;
  }
  CancelPrerendering();
}

void PrerenderProcessor::CancelPrerendering() {
  TRACE_EVENT0("navigation", "PrerenderProcessor::CancelPrerendering");
  DCHECK_EQ(state_, State::kStarted);
  state_ = State::kCancelled;
  GetPrerenderHostRegistry().AbandonHost(prerender_frame_tree_node_id_);
}

PrerenderHostRegistry& PrerenderProcessor::GetPrerenderHostRegistry() {
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      initiator_render_frame_host_.GetStoragePartition());
  return *storage_partition_impl->GetPrerenderHostRegistry();
}

}  // namespace content
