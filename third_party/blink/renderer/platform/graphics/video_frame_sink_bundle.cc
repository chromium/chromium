// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/video_frame_sink_bundle.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "build/build_config.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/viz_util.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

mojom::blink::EmbeddedFrameSinkProvider* g_frame_sink_provider_override =
    nullptr;

std::unique_ptr<VideoFrameSinkBundle>& GetThreadFrameSinkBundlePtr() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<std::unique_ptr<VideoFrameSinkBundle>>, bundle, ());
  return *bundle;
}

}  // namespace

VideoFrameSinkBundle::VideoFrameSinkBundle(base::PassKey<VideoFrameSinkBundle>,
                                           uint32_t client_id)
    : id_(GenerateFrameSinkBundleId(client_id)) {
  mojo::Remote<mojom::blink::EmbeddedFrameSinkProvider> host_provider;
  mojom::blink::EmbeddedFrameSinkProvider* provider;
  if (g_frame_sink_provider_override) {
    provider = g_frame_sink_provider_override;
  } else {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        host_provider.BindNewPipeAndPassReceiver());
    provider = host_provider.get();
  }
  provider->RegisterEmbeddedFrameSinkBundle(
      id_, bundle_.BindNewPipeAndPassReceiver(),
      receiver_.BindNewPipeAndPassRemote());
  bundle_.set_disconnect_handler(base::BindOnce(
      &VideoFrameSinkBundle::OnDisconnected, base::Unretained(this)));
}

VideoFrameSinkBundle::~VideoFrameSinkBundle() = default;

// static
VideoFrameSinkBundle& VideoFrameSinkBundle::GetOrCreateSharedInstance(
    uint32_t client_id) {
  auto& bundle_ptr = GetThreadFrameSinkBundlePtr();
  if (bundle_ptr) {
    // Renderers only use a single client ID with Viz, so this must always be
    // true. If for whatever reason it changes, we would need to maintain a
    // thread-local mapping from client ID to VideoFrameSinkBundle instead of
    // sharing a single thread-local instance.
    DCHECK_EQ(bundle_ptr->bundle_id().client_id(), client_id);
    return *bundle_ptr;
  }

  bundle_ptr = std::make_unique<VideoFrameSinkBundle>(
      base::PassKey<VideoFrameSinkBundle>(), client_id);
  return *bundle_ptr;
}

// static
VideoFrameSinkBundle* VideoFrameSinkBundle::GetSharedInstanceForTesting() {
  return GetThreadFrameSinkBundlePtr().get();
}

// static
void VideoFrameSinkBundle::DestroySharedInstanceForTesting() {
  GetThreadFrameSinkBundlePtr().reset();
}

// static
void VideoFrameSinkBundle::SetFrameSinkProviderForTesting(
    mojom::blink::EmbeddedFrameSinkProvider* provider) {
  g_frame_sink_provider_override = provider;
}

void VideoFrameSinkBundle::SetBeginFrameObserver(
    std::unique_ptr<BeginFrameObserver> observer) {
  begin_frame_observer_ = std::move(observer);
  if (begin_frame_observer_) {
    begin_frame_observer_->OnBeginFrameCompletionEnabled(
        !sinks_needing_begin_frames_.empty());
  }
}

base::WeakPtr<VideoFrameSinkBundle> VideoFrameSinkBundle::AddClient(
    const viz::FrameSinkId& frame_sink_id,
    viz::mojom::blink::CompositorFrameSinkClient* client,
    mojo::Remote<mojom::blink::EmbeddedFrameSinkProvider>& frame_sink_provider,
    mojo::Receiver<viz::mojom::blink::CompositorFrameSinkClient>& receiver,
    mojo::Remote<viz::mojom::blink::CompositorFrameSink>& remote) {
  DCHECK_EQ(frame_sink_id.client_id(), id_.client_id());

  // Ensure that the bundle is created service-side before the our
  // CreateBundledCompositorFrameSink message below reaches the Viz host.
  frame_sink_provider.PauseReceiverUntilFlushCompletes(bundle_.FlushAsync());

  frame_sink_provider->CreateBundledCompositorFrameSink(
      frame_sink_id, id_, receiver.BindNewPipeAndPassRemote(),
      remote.BindNewPipeAndPassReceiver());
  clients_.Set(frame_sink_id.sink_id(), client);

  // This serves as a second synchronization barrier, this time blocking the
  // bundle from receiving any new messages until the service-side
  // CompositorFrameSinkImpl has been bound for this frame sink.
  bundle_.PauseReceiverUntilFlushCompletes(remote.FlushAsync());
  return weak_ptr_factory_.GetWeakPtr();
}

void VideoFrameSinkBundle::RemoveClient(const viz::FrameSinkId& frame_sink_id) {
  clients_.erase(frame_sink_id.sink_id());
}

void VideoFrameSinkBundle::InitializeCompositorFrameSinkType(
    uint32_t sink_id,
    viz::mojom::blink::CompositorFrameSinkType type) {
  bundle_->InitializeCompositorFrameSinkType(sink_id, type);
}

void VideoFrameSinkBundle::SetNeedsBeginFrame(uint32_t sink_id,
                                              bool needs_begin_frame) {
  DVLOG(2) << __func__ << " this " << this << " sink_id " << sink_id
           << " needs_begin_frame " << needs_begin_frame;
  bool was_empty = sinks_needing_begin_frames_.empty();
  if (needs_begin_frame) {
    sinks_needing_begin_frames_.insert(sink_id);
  } else {
    sinks_needing_begin_frames_.erase(sink_id);
  }
  if (begin_frame_observer_) {
    if (was_empty && !sinks_needing_begin_frames_.empty()) {
      begin_frame_observer_->OnBeginFrameCompletionEnabled(true);
    } else if (!was_empty && sinks_needing_begin_frames_.empty()) {
      begin_frame_observer_->OnBeginFrameCompletionEnabled(false);
    }
  }
  // These messages are not sent often, so we don't bother batching them.
  bundle_->SetNeedsBeginFrame(sink_id, needs_begin_frame);
}

void VideoFrameSinkBundle::SetWantsBeginFrameAcks(uint32_t sink_id) {
  // These messages are not sent often, so we don't bother batching them.
  bundle_->SetWantsBeginFrameAcks(sink_id);
}

void VideoFrameSinkBundle::SubmitCompositorFrame(
    uint32_t sink_id,
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    std::optional<viz::HitTestRegionList> hit_test_region_list,
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
    const viz::SharedBitmapId& id) {
  bundle_->DidAllocateSharedBitmap(sink_id, std::move(region), id);
}

void VideoFrameSinkBundle::DidDeleteSharedBitmap(
    uint32_t sink_id,
    const viz::SharedBitmapId& id) {
  // These messages are not urgent, but they must be well-ordered with respect
  // to frame submissions. Hence they are batched in the same queue and
  // flushed whenever any other messages are fit to flush.
  submission_queue_.push_back(viz::mojom::blink::BundledFrameSubmission::New(
      sink_id,
      viz::mojom::blink::BundledFrameSubmissionData::NewDidDeleteSharedBitmap(
          id)));
}

#if BUILDFLAG(IS_ANDROID)
void VideoFrameSinkBundle::SetThreadIds(
    uint32_t sink_id,
    const WTF::Vector<int32_t>& thread_ids) {
  bundle_->SetThreadIds(sink_id, thread_ids);
}
#endif

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
    it->value->OnBeginFrame(std::move(entry->args), std::move(entry->details),
                            entry->frame_ack, std::move(entry->resources));
  }
  defer_submissions_ = false;

  FlushMessages();

  if (begin_frame_observer_ && begin_frames.size())
    begin_frame_observer_->OnBeginFrameCompletion();
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

void VideoFrameSinkBundle::OnDisconnected() {
  if (disconnect_handler_for_testing_) {
    std::move(disconnect_handler_for_testing_).Run();
  }

  // If the bundle was disconnected, Viz must have terminated. Self-delete so
  // that a new bundle is created when the next client reconnects to Viz.
  GetThreadFrameSinkBundlePtr().reset();
}

void VideoFrameSinkBundle::FlushMessages() {
  if (submission_queue_.empty()) {
    return;
  }

  WTF::Vector<viz::mojom::blink::BundledFrameSubmissionPtr> submissions;
  std::swap(submissions, submission_queue_);
  bundle_->Submit(std::move(submissions));
}

}  // namespace blink
