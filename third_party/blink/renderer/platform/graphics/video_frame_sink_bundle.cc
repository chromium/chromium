// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_sink_bundle.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/graphics/viz_util.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

// We keep a thread-local map of VideoFrameSinkBundles, keyed by parent
// FrameSinkId and whether or not the submitter is a media stream.
using BundleKey = std::tuple<viz::FrameSinkId, bool>;

// NOTE: We use flat_map because of the odd key type, relatively small
// expected size, and relatively infrequent insertions and deletions.
using FrameSinkBundleMap =
    base::flat_map<BundleKey, std::unique_ptr<VideoFrameSinkBundle>>;

FrameSinkBundleMap& GetThreadFrameSinkBundleMap() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<FrameSinkBundleMap>, bundles,
                                  ());
  return *bundles;
}

}  // namespace

VideoFrameSinkBundle::VideoFrameSinkBundle(
    base::PassKey<VideoFrameSinkBundle>,
    mojom::blink::EmbeddedFrameSinkProvider& provider,
    const viz::FrameSinkId& parent_frame_sink_id,
    bool for_media_streams)
    : parent_frame_sink_id_(parent_frame_sink_id),
      for_media_streams_(for_media_streams) {
  ConnectNewBundle(provider);
}

VideoFrameSinkBundle::~VideoFrameSinkBundle() = default;

// static
VideoFrameSinkBundle& VideoFrameSinkBundle::GetOrCreateSharedInstance(
    mojom::blink::EmbeddedFrameSinkProvider& provider,
    const viz::FrameSinkId& parent_frame_sink_id,
    bool for_media_stream) {
  const BundleKey key(parent_frame_sink_id, for_media_stream);
  auto& bundle = GetThreadFrameSinkBundleMap()[key];
  if (!bundle) {
    bundle = std::make_unique<VideoFrameSinkBundle>(
        base::PassKey<VideoFrameSinkBundle>(), provider, parent_frame_sink_id,
        for_media_stream);
  } else if (bundle->is_context_lost()) {
    bundle->ConnectNewBundle(provider);
  }
  return *bundle;
}

// static
VideoFrameSinkBundle* VideoFrameSinkBundle::GetSharedInstanceForTesting(
    const viz::FrameSinkId& parent_frame_sink_id,
    bool for_media_stream) {
  const BundleKey key(parent_frame_sink_id, for_media_stream);
  auto& bundles = GetThreadFrameSinkBundleMap();
  auto it = bundles.find(key);
  if (it == bundles.end()) {
    return nullptr;
  }
  DCHECK(it->second);
  return it->second.get();
}

// static
void VideoFrameSinkBundle::DestroySharedInstancesForTesting() {
  GetThreadFrameSinkBundleMap().clear();
}

void VideoFrameSinkBundle::AddClient(
    const viz::FrameSinkId& id,
    viz::mojom::blink::CompositorFrameSinkClient* client,
    mojo::Remote<viz::mojom::blink::CompositorFrameSink>& sink) {
  clients_.Set(id.sink_id(), client);

  // This serves as a synchronization barrier, blocking the bundle from
  // receiving any new messages until the service-side CompositorFrameSinkImpl
  // has been bound for this frame sink.
  bundle_.PauseReceiverUntilFlushCompletes(sink.FlushAsync());
}

void VideoFrameSinkBundle::RemoveClient(const viz::FrameSinkId& frame_sink_id) {
  clients_.erase(frame_sink_id.sink_id());
  if (!clients_.IsEmpty()) {
    return;
  }

  // No more clients, so self-delete.
  GetThreadFrameSinkBundleMap().erase(
      BundleKey(parent_frame_sink_id_, for_media_streams_));
}

void VideoFrameSinkBundle::OnContextLost(
    const viz::FrameSinkBundleId& bundle_id) {
  if (bundle_id != id_) {
    // If this context loss report is regarding a previously used bundle ID,
    // we can ignore it. Someone else in the bundle already reported it and
    // we've since connected a new bundle.
    return;
  }

  is_context_lost_ = true;
}

void VideoFrameSinkBundle::InitializeCompositorFrameSinkType(
    uint32_t sink_id,
    viz::mojom::blink::CompositorFrameSinkType type) {
  bundle_->InitializeCompositorFrameSinkType(sink_id, type);
}

void VideoFrameSinkBundle::SetNeedsBeginFrame(uint32_t sink_id,
                                              bool needs_begin_frame) {
  // These messages are not sent often, so we don't bother batching them.
  bundle_->SetNeedsBeginFrame(sink_id, needs_begin_frame);
}

void VideoFrameSinkBundle::SubmitCompositorFrame(
    uint32_t sink_id,
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    absl::optional<viz::HitTestRegionList> hit_test_region_list,
    uint64_t submit_time) {
  auto bundled_frame = viz::mojom::blink::BundledCompositorFrame::New();
  bundled_frame->local_surface_id = local_surface_id;
  bundled_frame->frame = std::move(frame);
  bundled_frame->hit_test_region_list = std::move(hit_test_region_list);
  bundled_frame->submit_time = submit_time;

  auto submission = viz::mojom::blink::BundledFrameSubmission::New();
  submission->sink_id = sink_id;
  submission->data = viz::mojom::blink::BundledFrameSubmissionData::NewFrame(
      std::move(bundled_frame));

  // Note that we generally expect this call to be nested while processing
  // OnBeginFrame() notifications, rather than at a delayed time in the future.
  // This will happen while nested within FlushNotifications(), where
  // `defer_submissions_` is true.
  submission_queue_.push_back(std::move(submission));
  if (!defer_submissions_) {
    FlushMessages();
  }
}

void VideoFrameSinkBundle::DidNotProduceFrame(uint32_t sink_id,
                                              const viz::BeginFrameAck& ack) {
  auto submission = viz::mojom::blink::BundledFrameSubmission::New();
  submission->sink_id = sink_id;
  submission->data =
      viz::mojom::blink::BundledFrameSubmissionData::NewDidNotProduceFrame(ack);

  // See the note in SubmitCompositorFrame above regarding queueing.
  submission_queue_.push_back(std::move(submission));
  if (!defer_submissions_) {
    FlushMessages();
  }
}

void VideoFrameSinkBundle::DidAllocateSharedBitmap(
    uint32_t sink_id,
    base::ReadOnlySharedMemoryRegion region,
    const gpu::Mailbox& id) {
  bundle_->DidAllocateSharedBitmap(sink_id, std::move(region), id);
}

void VideoFrameSinkBundle::DidDeleteSharedBitmap(uint32_t sink_id,
                                                 const gpu::Mailbox& id) {
  // These messages are not urgent, but they must be well-ordered with respect
  // to frame submissions. Hence they are batched in the same queue and
  // flushed whenever any other messages are fit to flush.
  submission_queue_.push_back(viz::mojom::blink::BundledFrameSubmission::New(
      sink_id,
      viz::mojom::blink::BundledFrameSubmissionData::NewDidDeleteSharedBitmap(
          id)));
}

void VideoFrameSinkBundle::FlushNotifications(
    WTF::Vector<viz::mojom::blink::BundledReturnedResourcesPtr> acks,
    WTF::Vector<viz::mojom::blink::BeginFrameInfoPtr> begin_frames,
    WTF::Vector<viz::mojom::blink::BundledReturnedResourcesPtr>
        reclaimed_resources) {
  for (const auto& entry : acks) {
    auto it = clients_.find(entry->sink_id);
    if (it == clients_.end())
      continue;
    it->value->DidReceiveCompositorFrameAck(std::move(entry->resources));
  }

  for (const auto& entry : reclaimed_resources) {
    auto it = clients_.find(entry->sink_id);
    if (it == clients_.end())
      continue;
    it->value->ReclaimResources(std::move(entry->resources));
  }

  // When OnBeginFrame() is invoked on each client, the client will typically
  // call back into us with either SubmitCompositorFrame or
  // DidNotProduceFrame. Setting `defer_submissions_` to true here ensures
  // that we'll queue those calls rather than letting them send IPC directly.
  // Then a single batch IPC is sent with all of these at the end, via
  // FlushMessages() below.
  defer_submissions_ = true;
  for (auto& entry : begin_frames) {
    auto it = clients_.find(entry->sink_id);
    if (it == clients_.end())
      continue;
    it->value->OnBeginFrame(std::move(entry->args), std::move(entry->details));
  }
  defer_submissions_ = false;

  FlushMessages();
}

void VideoFrameSinkBundle::OnBeginFramePausedChanged(uint32_t sink_id,
                                                     bool paused) {
  auto it = clients_.find(sink_id);
  if (it == clients_.end())
    return;

  it->value->OnBeginFramePausedChanged(paused);
}

void VideoFrameSinkBundle::OnCompositorFrameTransitionDirectiveProcessed(
    uint32_t sink_id,
    uint32_t sequence_id) {
  auto it = clients_.find(sink_id);
  if (it == clients_.end())
    return;

  it->value->OnCompositorFrameTransitionDirectiveProcessed(sequence_id);
}

void VideoFrameSinkBundle::ConnectNewBundle(
    mojom::blink::EmbeddedFrameSinkProvider& provider) {
  DCHECK(!bundle_ || is_context_lost_);
  is_context_lost_ = false;
  bundle_.reset();
  receiver_.reset();
  id_ = GenerateFrameSinkBundleId(parent_frame_sink_id_);
  provider.RegisterEmbeddedFrameSinkBundle(
      parent_frame_sink_id_, id_, bundle_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote());
}

void VideoFrameSinkBundle::FlushMessages() {
  if (submission_queue_.IsEmpty()) {
    return;
  }

  WTF::Vector<viz::mojom::blink::BundledFrameSubmissionPtr> submissions;
  std::swap(submissions, submission_queue_);
  bundle_->Submit(std::move(submissions));
}

}  // namespace blink
