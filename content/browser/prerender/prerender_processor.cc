// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_processor.h"

#include "base/feature_list.h"
#include "content/browser/prerender/prerender_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/common/features.h"

namespace content {

PrerenderProcessor::PrerenderProcessor(
    RenderFrameHostImpl& initiator_render_frame_host)
    : initiator_render_frame_host_(initiator_render_frame_host),
      initiator_origin_(initiator_render_frame_host.GetLastCommittedOrigin()) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kPrerender2));
}

PrerenderProcessor::~PrerenderProcessor() {
  if (state_ == State::kStarted)
    CancelPrerendering();
}

// TODO(https://crbug.com/1132746): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
void PrerenderProcessor::Start(
    blink::mojom::PrerenderAttributesPtr attributes,
    mojo::PendingRemote<blink::mojom::PrerenderProcessorClient>
        pending_remote) {
  // Start() must be called only one time.
  if (state_ != State::kInitial) {
    mojo::ReportBadMessage("PP_START_TWICE");
    return;
  }
  state_ = State::kStarted;

  // TODO(https://crbug.com/1132746): Validate the origin, etc and send
  // mojo::ReportBadMessage() if necessary like the legacy prerender
  // `prerender::PrerenderProcessorImpl::Start()`.

  // TODO(https://crbug.com/1138711, https://crbug.com/1138723): Abort if the
  // initiator frame is not the main frame (i.e., iframe or pop-up window).

  prerendering_url_ = attributes->url;

  // Start prerendering.
  auto prerender_host = std::make_unique<PrerenderHost>(
      std::move(attributes),
      initiator_render_frame_host_.GetGlobalFrameRoutingId(),
      initiator_origin_);
  prerender_host->StartPrerendering();

  // Register the prerender host to PrerenderHostRegistry so that navigation can
  // find this prerendered contents.
  GetPrerenderHostRegistry().RegisterHost(prerendering_url_,
                                          std::move(prerender_host));
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
  DCHECK_EQ(state_, State::kStarted);
  state_ = State::kCancelled;
  GetPrerenderHostRegistry().AbandonHost(prerendering_url_);
}

PrerenderHostRegistry& PrerenderProcessor::GetPrerenderHostRegistry() {
  auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
      initiator_render_frame_host_.GetStoragePartition());
  return *storage_partition_impl->GetPrerenderHostRegistry();
}

}  // namespace content
