// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_SINK_BUNDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_SINK_BUNDLE_H_

#include <stdint.h>

#include "base/types/pass_key.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-blink-forward.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame_sinks/embedded_frame_sink.mojom-blink.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Encapsulates a thread-local FrameSinkBundle connection for a given parent
// FrameSinkId which contains one or more VideoFrameSubmitters. This is
// responsible for demultiplexing batched communication from Viz, as well as for
// aggregating and apporopriately batching most outgoing communication to Viz on
// behalf of each VideoFrameSubmitter.
class PLATFORM_EXPORT VideoFrameSinkBundle
    : public viz::mojom::blink::FrameSinkBundleClient {
 public:
  VideoFrameSinkBundle(base::PassKey<VideoFrameSinkBundle>,
                       mojom::blink::EmbeddedFrameSinkProvider& provider,
                       const viz::FrameSinkId& parent_frame_sink_id,
                       bool for_media_streams);

  VideoFrameSinkBundle(const VideoFrameSinkBundle&) = delete;
  VideoFrameSinkBundle& operator=(const VideoFrameSinkBundle&) = delete;
  ~VideoFrameSinkBundle() override;

  // Acquires a lazily initialized VideoFrameSinkBundle instance dedicated to
  // the given parent FrameSinkId and submitter type (media stream submitters vs
  // video submitters cannot be bundled together) for the calling thread. If a
  // matching instance already exists for these parameters it is reused;
  // otherwise if a new FrameSinkBundle connection to Viz must be established
  // for this object, it is done using `provider`.
  static VideoFrameSinkBundle& GetOrCreateSharedInstance(
      mojom::blink::EmbeddedFrameSinkProvider& provider,
      const viz::FrameSinkId& parent_frame_sink_id,
      bool for_media_streams);

  // Acquires an instance that would be returned by GetOrCreateSharedInstance
  // for the same parameters, but does not create an appropriate instance if one
  // does not exist. Instead, this returns null in that case.
  static VideoFrameSinkBundle* GetSharedInstanceForTesting(
      const viz::FrameSinkId& parent_frame_sink_id,
      bool for_media_streams);

  // Ensures that all shared instances are torn down.
  static void DestroySharedInstancesForTesting();

  const viz::FrameSinkBundleId& bundle_id() const { return id_; }
  bool is_context_lost() const { return is_context_lost_; }

  // Adds a new client to this bundle, to receive batch notifications from Viz.
  // `client` must outlive this object or be explicitly removed by
  // RemoveClient() before being destroyed.
  void AddClient(const viz::FrameSinkId& id,
                 viz::mojom::blink::CompositorFrameSinkClient* client,
                 mojo::Remote<viz::mojom::blink::CompositorFrameSink>& sink);
  void RemoveClient(const viz::FrameSinkId& id);

  // Notifies the bundle that one of its clients has been disconnected from Viz.
  // `bundle_id` indicates the bundle ID that was in use by this object at the
  // time the client was added, and is used to differentiate meaningful new
  // disconnection events from stale ones already observed by this object.
  void OnContextLost(const viz::FrameSinkBundleId& bundle_id);

  // Helper methods used by VideoFrameSubmitters to communicate potentially
  // batched requests to Viz. These correspond closely to methods on the
  // CompositorFrameSink interface.
  void InitializeCompositorFrameSinkType(
      uint32_t sink_id,
      viz::mojom::blink::CompositorFrameSinkType);
  void SetNeedsBeginFrame(uint32_t sink_id, bool needs_begin_frame);
  void SubmitCompositorFrame(
      uint32_t sink_id,
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      absl::optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time);
  void DidNotProduceFrame(uint32_t sink_id, const viz::BeginFrameAck& ack);
  void DidAllocateSharedBitmap(uint32_t sink_id,
                               base::ReadOnlySharedMemoryRegion region,
                               const gpu::Mailbox& id);
  void DidDeleteSharedBitmap(uint32_t sink_id, const gpu::Mailbox& id);

  // viz::mojom::blink::FrameSinkBundleClient implementation:
  void FlushNotifications(
      WTF::Vector<viz::mojom::blink::BundledReturnedResourcesPtr> acks,
      WTF::Vector<viz::mojom::blink::BeginFrameInfoPtr> begin_frames,
      WTF::Vector<viz::mojom::blink::BundledReturnedResourcesPtr>
          reclaimed_resources) override;
  void OnBeginFramePausedChanged(uint32_t sink_id, bool paused) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sink_id,
      uint32_t sequence_id) override;

 private:
  // Connects (or re-connects) to Viz to grant this object control over a unique
  // FrameSinkBundle/Client interface connection. Must only be called on a newly
  // initialized VideoFrameSinkBundle, or once the bundle's context has been
  // lost.
  void ConnectNewBundle(mojom::blink::EmbeddedFrameSinkProvider& provider);

  void FlushMessages();

  const viz::FrameSinkId parent_frame_sink_id_;
  const bool for_media_streams_;

  viz::FrameSinkBundleId id_;
  bool is_context_lost_ = false;
  mojo::Remote<viz::mojom::blink::FrameSinkBundle> bundle_;
  mojo::Receiver<viz::mojom::blink::FrameSinkBundleClient> receiver_{this};
  WTF::HashMap<uint32_t, viz::mojom::blink::CompositorFrameSinkClient*>
      clients_;

  bool defer_submissions_ = false;
  WTF::Vector<viz::mojom::blink::BundledFrameSubmissionPtr> submission_queue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VIDEO_FRAME_SINK_BUNDLE_H_
